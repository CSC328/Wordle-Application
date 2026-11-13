# Giorgi - server integration review

Reviewed on November 12, 2026, using the published client and shared library
from commit `a6d64d508925f70547ba95207d63091e35b2ae5f`.

The server in `wordle-server-fixed.zip` passed the local integration checks.
Both issues found in the first server review are resolved: parent shutdown
closes the listener and terminates/reaps its children, and malformed frames
receive `ERROR BAD_FRAME` when the connection can still send before closing.

## Files added or replaced

- `server.c`: copied unchanged from Giorgi's corrected ZIP.
- `words.txt`: the ten-word bank from Giorgi's first ZIP; the corrected ZIP
  explicitly says to retain this bank.
- This review records the checks and current run instructions.

Shiva's client, Cherry's common library, the Makefile, existing tests, and all
earlier documentation are unchanged. Earlier handoff documents describe the
stage when the server was still a starter; this review records its integration.
No compiled executables, object files, or static archives are committed.

## Verification

The C programs and shared library built with GCC using C11 and
`-Wall -Wextra -Wpedantic -Werror -O2`.

| Checks | Result |
| --- | --- |
| Existing shared-library suite | 21 cases / 278 checks passed |
| Existing client suite | 27 tests passed; no skips |
| Real server and integration checks | 18 checks passed |

Server checks covered default port 53280 and a custom port, invalid arguments,
missing/empty/invalid word banks, normalized entries, protocol aliases,
fragmented/coalesced messages, invalid commands, and an occupied port.

The real C client and server completed invalid-guess handling, repeated-letter
feedback, a win, replay on the same connection, a six-guess loss, and quitting.
Two simultaneous clients ran in separate child processes; completed and
abruptly disconnected sessions were reaped while the parent kept accepting.

Shutdown checks sent SIGINT and SIGTERM to the parent with multiple active
sessions, including an incomplete request. They confirmed clean parent exit,
closed sessions, a closed listener, and no surviving server children. A
foreground-style process-group SIGINT and 12 connection/shutdown races also
passed. Framing checks covered invalid bytes, oversized/truncated frames,
the maximum complete frame, and normal EOF.

The server uses an IPv4 listener and accepts at most 1000 valid word-bank
entries. The 1000-entry boundary and rejection of 1001 entries were checked.
The supplied bank contains ten valid entries.

These are local Linux results. The integrated server has not yet been tested
on acad. The client's earlier acad results remain in `ACAD_CLIENT_RESULTS.md`.
Server allocation/fork failures were reviewed in the source but not forced
during execution. No server sanitizer or leak-check result is claimed here.
The application is C; Python is used only for automated checks.

## Run on acad

Inside `cpsc328wordle`, build and run the existing checks:

```sh
make clean
make programs
make test
python3 tests/test_client.py
```

Start the server from that directory so it can find `words.txt`:

```sh
./server
```

In another terminal, enter the same directory and run:

```sh
./client localhost
```

For a custom port, use `./server 54000` and `./client localhost 54000`.
Check a win/loss, replay, two simultaneous clients, and Ctrl+C shutdown of
the active server. The team still needs to record its actual acad integration
results and complete the final submission/Gantt records.
