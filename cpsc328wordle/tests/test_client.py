#!/usr/bin/env python3
"""Client checks using a local TCP test peer, not the unfinished server.

TCP listener setup follows echoServer.py; see docs/CLIENT_SOURCES.md [6].
"""

import errno
import os
from pathlib import Path
import re
import select
import socket
import subprocess
import sys
import threading
import time
import unittest


ROOT = Path(__file__).resolve().parents[1]
CLIENT = Path(os.environ.get("WORDLE_CLIENT", str(ROOT / "client"))).resolve()
TIMEOUT = 5
ROW = re.compile(r"^([1-6])\. ([A-Z_](?: [A-Z_]){4}) \| ([GY_\-](?: [GY_\-]){4}) $", re.M)


class Peer:
    def __init__(self, script=None, words=("APPLE",), port=0, family=socket.AF_INET):
        # Setup adapted from P-5.zip: examples/sockets/echoPython/echoServer.py.
        self.listener = socket.socket(family, socket.SOCK_STREAM)
        self.listener.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        host = "::1" if family == socket.AF_INET6 else "127.0.0.1"
        try:
            self.listener.bind((host, port))
            self.listener.listen(1)
        except OSError:
            self.listener.close()
            raise
        self.listener.settimeout(TIMEOUT)
        # End of adapted listener setup; protocol replies below are test fixtures.
        self.host = host
        self.port = self.listener.getsockname()[1]
        self.script = script
        self.words = iter(words)
        self.commands = []
        self.error = None
        self.release = threading.Event()
        self.thread = threading.Thread(target=self.serve, daemon=True)

    def __enter__(self):
        self.thread.start()
        return self

    def __exit__(self, kind, value, traceback):
        self.release.set()
        self.thread.join(TIMEOUT + 1)
        self.listener.close()
        if kind is None:
            if self.thread.is_alive():
                raise AssertionError("Test peer did not finish")
            if self.error is not None:
                raise self.error

    def receive(self, connection):
        line = bytearray()
        while True:
            ch = connection.recv(1)
            if not ch:
                assert not line, "Client closed during a message"
                return None
            if ch == b"\n":
                command = line.decode("ascii")
                self.commands.append(command)
                return command
            line.extend(ch)
            assert len(line) < 128, "Client sent an oversized message"

    def expect(self, connection, command):
        actual = self.receive(connection)
        assert actual == command, (actual, command)

    def normal(self, connection):
        connection.sendall(b"HELLO\n")
        while True:
            command = self.receive(connection)
            if command == "BYE":
                self.expect(connection, None)
                return
            assert command == "READY", command
            connection.sendall(f"WORD {next(self.words)}\n".encode("ascii"))

    def serve(self):
        try:
            connection, _ = self.listener.accept()
            with connection:
                connection.settimeout(TIMEOUT)
                (self.script or self.normal)(connection)
        except Exception as error:
            self.error = error


def run_client(peer, data=b"", hostname=None, port_text=None, default=False):
    command = [str(CLIENT), hostname or peer.host]
    if not default:
        command.append(port_text or str(peer.port))
    return subprocess.run(command, input=data, stdout=subprocess.PIPE,
                          stderr=subprocess.PIPE, timeout=TIMEOUT, check=False)


def first_feedback(output):
    rows = ROW.findall(output.decode())
    return next(feedback.replace(" ", "") for _, word, feedback in rows
                if "_" not in word)


def read_until(process, marker):
    data = bytearray()
    deadline = time.monotonic() + TIMEOUT
    while marker not in data:
        remaining = deadline - time.monotonic()
        if remaining <= 0 or not select.select([process.stdout], [], [], remaining)[0]:
            raise AssertionError(f"Client never displayed {marker!r}")
        chunk = os.read(process.stdout.fileno(), 4096)
        if not chunk:
            raise AssertionError(f"Client exited before {marker!r}: {bytes(data)!r}")
        data.extend(chunk)
    return bytes(data)


class ClientTests(unittest.TestCase):
    def success(self, result):
        self.assertEqual(result.returncode, 0, result.stderr.decode())
        self.assertEqual(result.stderr, b"")
        self.assertIn(b"Goodbye.", result.stdout)

    def test_arguments(self):
        for args in ([], [""], ["localhost", "54000", "extra"]):
            with self.subTest(args=args):
                result = subprocess.run([str(CLIENT)] + args, capture_output=True,
                                        timeout=TIMEOUT, check=False)
                self.assertNotEqual(result.returncode, 0)
                self.assertIn(b"Usage:", result.stderr)

    def test_invalid_ports(self):
        for port in ("", "0", "1023", "65536", "70000", "-1", "+53280",
                     " 53280", "53280 ", "53280x", "53.280", "9" * 100):
            with self.subTest(port=port):
                result = subprocess.run([str(CLIENT), "localhost", port],
                                        capture_output=True, timeout=TIMEOUT, check=False)
                self.assertNotEqual(result.returncode, 0)
                self.assertIn(b"1024-65535", result.stderr)

    def test_connection_refused(self):
        with socket.socket() as listener:
            listener.bind(("127.0.0.1", 0))
            port = listener.getsockname()[1]
        result = subprocess.run([str(CLIENT), "127.0.0.1", str(port)],
                                capture_output=True, timeout=TIMEOUT, check=False)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn(b"Cannot connect", result.stderr)

    def test_hostname_error(self):
        result = subprocess.run([str(CLIENT), "256.256.256.256"],
                                capture_output=True, timeout=TIMEOUT, check=False)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn(b"Cannot resolve", result.stderr)

    def test_default_port_and_hostname(self):
        try:
            peer = Peer(port=53280)
        except OSError as error:
            if error.errno == errno.EADDRINUSE:
                self.skipTest("Default port 53280 is already in use")
            raise
        with peer:
            result = run_client(peer, b"APPLE\nn\n", hostname="localhost", default=True)
        self.success(result)
        self.assertIn(b"port 53280", result.stdout)
        self.assertEqual(peer.commands, ["READY", "BYE"])

    def test_custom_port_with_leading_zeroes(self):
        with Peer() as peer:
            result = run_client(peer, b"APPLE\nn\n", port_text=f"000{peer.port}")
        self.success(result)

    def test_ipv6_when_available(self):
        try:
            peer = Peer(family=socket.AF_INET6)
        except OSError as error:
            self.skipTest(str(error))
        with peer:
            result = run_client(peer, b"APPLE\nn\n")
        self.success(result)

    def test_design_feedback_and_local_guesses(self):
        with Peer() as peer:
            result = run_client(peer, b"APRON\nAPPLE\nn\n")
        self.success(result)
        self.assertEqual(first_feedback(result.stdout), "GG---")
        self.assertEqual(peer.commands, ["READY", "BYE"])

    def test_repeated_letters(self):
        examples = [
            ("APPLE", "PAPAL", "YYG-Y"), ("APPLE", "ALLEY", "GY-Y-"),
            ("APPLE", "PPPPP", "-GG--"), ("ABBEY", "BABBB", "YYG--"),
            ("LEVEL", "LEECH", "GGY--"), ("LEVEL", "ELLEE", "YYYG-"),
            ("BANAL", "AAAAA", "-G-G-"), ("SASSY", "ASSAY", "YYG-G"),
            ("CIVIC", "ICICI", "YYYY-"), ("EERIE", "EEEEE", "GG--G"),
            ("ROTOR", "TORRO", "YGYYY"), ("GEESE", "EEEEE", "-GG-G"),
            ("ABABA", "BAAAB", "YYGYY"), ("FLUFF", "FFFFF", "G--GG"),
            ("ALLOT", "LLAMA", "YGY--"), ("MAMMA", "AMMMM", "YYGG-"),
        ]
        for answer, guess, expected in examples:
            with self.subTest(answer=answer, guess=guess):
                with Peer(words=(answer,)) as peer:
                    result = run_client(peer, f"{guess}\n{answer}\nn\n".encode())
                self.success(result)
                self.assertEqual(first_feedback(result.stdout), expected)

    def test_invalid_guesses_do_not_use_attempts(self):
        bad = [b"CAT", b"AB12!", b"", b"APPLES", b":quitx", b"AP LE", b"APPLE\x00x",
               b"\x00APPLE", b"a" * 3000, b"APPL\xff"]
        with Peer() as peer:
            result = run_client(peer, b"\n".join(bad) + b"\n \tApPlE\r\nn\n")
        self.success(result)
        self.assertEqual(result.stdout.count(b"This did not use a guess."), len(bad))
        self.assertEqual(result.stdout.count(b"Guess 1 of 6:"), len(bad) + 1)
        self.assertNotIn(b"Guess 2 of 6:", result.stdout)
        self.assertIn(b"Guesses remaining: 5", result.stdout)

    def test_board_alphabet_and_hidden_answer(self):
        with Peer() as peer:
            result = run_client(peer, b"APRON\n")
        self.success(result)
        rows = ROW.findall(result.stdout.decode())
        self.assertEqual(len(rows), 12)
        self.assertTrue(all("_" in word for _, word, _ in rows[:6]))
        self.assertEqual(rows[6], ("1", "A P R O N", "G G - - -"))
        alphabet = " ".join(ch for ch in "ABCDEFGHIJKLMNOPQRSTUVWXYZ" if ch not in "APRON")
        self.assertIn(f"Unguessed letters: {alphabet}\n".encode(), result.stdout)
        self.assertNotIn(b"APPLE", result.stdout)

    def test_win_on_sixth_guess(self):
        with Peer() as peer:
            result = run_client(peer, b"CROWN\n" * 5 + b"APPLE\nn\n")
        self.success(result)
        self.assertIn(b"You won!", result.stdout)
        self.assertNotIn(b"You lost.", result.stdout)
        self.assertIn(b"Guesses remaining: 0", result.stdout)

    def test_alphabet_when_all_letters_have_been_guessed(self):
        with Peer(words=("LEVEL",)) as peer:
            result = run_client(peer, b"ABCDE\nFGHIJ\nKLMNO\nPQRST\nUVWXY\nZABCD\nn\n")
        self.success(result)
        self.assertIn(b"Unguessed letters: (none)", result.stdout)

    def test_loss_after_six_guesses(self):
        with Peer() as peer:
            result = run_client(peer, b"CROWN\n" * 6 + b"n\n")
        self.success(result)
        self.assertIn(b"You lost. The word was APPLE.", result.stdout)
        self.assertNotIn(b"Guess 7", result.stdout)

    def test_replay_resets_round_on_same_connection(self):
        with Peer(words=("APPLE", "LEVEL")) as peer:
            result = run_client(peer, b"APPLE\n \tY \r\nLEVEL\nN\n")
        self.success(result)
        self.assertEqual(peer.commands, ["READY", "READY", "BYE"])
        self.assertEqual(result.stdout.count(b"You won!"), 2)
        self.assertEqual(result.stdout.count(b"Guesses remaining: 6"), 2)
        self.assertEqual(result.stdout.count(b"Unguessed letters: A B C D E F G H I J K L M N O P Q R S T U V W X Y Z"), 2)

    def test_invalid_replay_answers(self):
        invalid = [b"yes", b"", b"nn", b"y n", b"y\x00", b"x" * 3000]
        with Peer() as peer:
            result = run_client(peer, b"APPLE\n" + b"\n".join(invalid) + b"\nn\n")
        self.success(result)
        self.assertEqual(result.stdout.count(b"Enter y or n."), len(invalid))
        self.assertEqual(peer.commands, ["READY", "BYE"])

    def test_eof_during_guess_or_replay(self):
        for data in (b"", b"CRO", b"A" * 3000, b"APPLE\n"):
            with self.subTest(data_length=len(data)):
                with Peer() as peer:
                    result = run_client(peer, data)
                self.success(result)
                self.assertEqual(peer.commands, ["READY", "BYE"])

    def test_eof_after_final_line_without_newline(self):
        with Peer() as peer:
            result = run_client(peer, b"apple")
        self.success(result)
        self.assertIn(b"You won!", result.stdout)

    def test_closed_stdin_does_not_become_socket_input(self):
        def script(connection):
            connection.sendall(b"HELLO\n")
            peer.expect(connection, "READY")
            connection.sendall(b"WORD APPLE\n")
            peer.expect(connection, None)
        wrapper = "import os,sys; os.close(0); os.execv(sys.argv[1],sys.argv[1:])"
        with Peer(script=script) as peer:
            result = subprocess.run([sys.executable, "-c", wrapper, str(CLIENT),
                                     peer.host, str(peer.port)], input=b"",
                                    stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                                    timeout=TIMEOUT, check=False)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn(b"Cannot monitor", result.stderr)

    def test_quit_command_during_guess_or_replay(self):
        for data in (b":quit\n", b" \t:quit \r\n", b"CROWN\n:quit\n",
                     b"APPLE\n:quit\n", b"APPLE\n \t:quit \r\n", b":quit"):
            with self.subTest(data=data):
                with Peer() as peer:
                    result = run_client(peer, data)
                self.success(result)
                self.assertEqual(peer.commands, ["READY", "BYE"])
                self.assertNotIn(b"This did not use a guess.", result.stdout)
                self.assertNotIn(b"Enter y or n.", result.stdout)

    def check_bad_message(self, frame, greeting=False, expected=b"Server"):
        def script(connection):
            if not greeting:
                connection.sendall(b"HELLO\n")
                peer.expect(connection, "READY")
            if frame:
                connection.sendall(frame)
            connection.shutdown(socket.SHUT_WR)
            peer.expect(connection, None)
        with Peer(script=script) as peer:
            result = run_client(peer)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn(expected, result.stderr)
        self.assertNotIn(b"Goodbye.", result.stdout)

    def test_wrong_message_direction_or_state(self):
        for frame, greeting in ((b"WORD APPLE\n", True), (b"READY\n", True),
                                (b"HELLO\n", False), (b"BYE\n", False),
                                (b"WORD\n", False)):
            with self.subTest(frame=frame, greeting=greeting):
                self.check_bad_message(frame, greeting, b"Unexpected server response")

    def test_malformed_frames_and_payloads(self):
        frames = [b"", b"WORD APPLE", b"WORD apple\n", b"WORD APPLES\n",
                  b"WORD APP\n", b"WORD  APPLE\n", b"WORD APPLE\r\n",
                  b"WORD AP\x00LE\n", b"WORD AP\xffLE\n", b"X" * 128 + b"\n", b"\n"]
        for frame in frames:
            with self.subTest(frame=frame[:30]):
                self.check_bad_message(frame)

    def test_server_error_and_maximum_frame(self):
        for reason in (b"BAD_COMMAND", b"R" * 121):
            with self.subTest(length=len(reason)):
                self.check_bad_message(b"ERROR " + reason + b"\n", expected=b"Server error: " + reason)

    def test_fragmented_replies(self):
        def script(connection):
            for ch in b"HELLO\n":
                connection.sendall(bytes([ch]))
                time.sleep(0.001)
            peer.expect(connection, "READY")
            for ch in b"WORD APPLE\n":
                connection.sendall(bytes([ch]))
                time.sleep(0.001)
            peer.expect(connection, "BYE")
            peer.expect(connection, None)
        with Peer(script=script) as peer:
            result = run_client(peer, b"APPLE\nn\n")
        self.success(result)

    def test_buffered_unexpected_reply_is_not_ignored(self):
        def script(connection):
            connection.sendall(b"HELLO\n")
            peer.expect(connection, "READY")
            connection.sendall(b"WORD APPLE\nREADY\n")
            peer.expect(connection, None)
        with Peer(script=script) as peer:
            result = run_client(peer)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn(b"Unexpected server message during input", result.stderr)

    def test_disconnect_or_error_while_waiting_for_input(self):
        for frame in (b"", b"ERROR SESSION_ENDED\n", b"WORD APPLE\n", b"HEL"):
            for replay in (False, True):
                with self.subTest(frame=frame, replay=replay):
                    def script(connection):
                        connection.sendall(b"HELLO\n")
                        peer.expect(connection, "READY")
                        connection.sendall(b"WORD APPLE\n")
                        assert peer.release.wait(TIMEOUT)
                        if frame:
                            connection.sendall(frame)
                        connection.shutdown(socket.SHUT_WR)
                        peer.expect(connection, None)
                    with Peer(script=script) as peer:
                        process = subprocess.Popen([str(CLIENT), peer.host, str(peer.port)],
                                                   stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                                                   stderr=subprocess.PIPE)
                        try:
                            read_until(process, b"Guess 1 of 6:")
                            if replay:
                                process.stdin.write(b"APPLE\n")
                                process.stdin.flush()
                                read_until(process, b"Play again? (y/n):")
                            peer.release.set()
                            process.wait(timeout=TIMEOUT)
                            _, error = process.communicate(timeout=TIMEOUT)
                            self.assertNotEqual(process.returncode, 0)
                            self.assertTrue(error)
                        finally:
                            if process.poll() is None:
                                process.kill()
                                process.communicate()

    def test_player_can_wait_without_an_idle_timeout(self):
        with Peer() as peer:
            process = subprocess.Popen([str(CLIENT), peer.host, str(peer.port)],
                                       stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                                       stderr=subprocess.PIPE)
            try:
                read_until(process, b"Guess 1 of 6:")
                time.sleep(0.1)
                self.assertIsNone(process.poll())
                output, error = process.communicate(b"APPLE\nn\n", timeout=TIMEOUT)
                self.assertEqual(process.returncode, 0, error)
                self.assertIn(b"You won!", output)
            finally:
                if process.poll() is None:
                    process.kill()
                    process.communicate()


if __name__ == "__main__":
    if not CLIENT.is_file():
        raise SystemExit("Build the client first: make client")
    unittest.main(verbosity=2)
