# Group 17 - Network Wordle

CPSC 328 Network and Secure Programming, Professor Walther.
Application documentation updated November 22, 2026.

## Team and responsibilities

| Member | Primary responsibility |
| --- | --- |
| **Shivamani Jadi - project leader** | Client implementation, game logic, board and alphabet display, replay, client tests and integration |
| Giorgi Kuparadze | Server implementation, word bank, forked sessions, signals, child-process cleanup and server tests |
| Charanya Ananthula (Cherry) | Shared library, README, design diagrams, documentation and test records |

## Project description

This is a terminal Wordle game written in C for Linux. A concurrent TCP server
loads a word bank and selects a random five-letter answer for each request.
The client handles the game locally: six valid guesses, letter feedback, the
board, remaining unguessed letters, win/loss messages and replay.

The server creates a separate child process with `fork()` for each connection.
Clients can play independently. Both programs include `wordle_common.h` and
link the static library `libwordle.a`.

## Requirements and compilation

Use Linux with GCC, GNU Make, GNU binutils (`ar` and the GNU linker), and the
standard Linux C headers. Python 3 is needed only for the automated client
tests. Run the commands below inside the extracted `cpsc328wordle` directory.

Build both programs and their library:

```sh
make clean
make programs
```

This creates `client`, `server` and `libwordle.a`. To build one program,
use `make client` or `make server`; its library dependency is built too.
The default `make` target builds the library alone, so use `make programs`
for the complete application. `make clean` removes generated build files.

## Running the server

Usage:

```text
./server [port]
```

Start the server from the project directory containing `words.txt`:

```sh
./server
```

Without a port argument, the server listens on **53280**. Wait for its
listening message before starting clients. To choose a different port:

```sh
./server 54000
```

Explicit ports must contain decimal digits only and be between 1024 and 65535.
Press Ctrl+C in the server terminal to stop it. The parent closes its listener,
terminates active children and reaps them. SIGTERM also requests shutdown.

### Word bank

The supplied `words.txt` contains ten words, one per line. Valid entries have
exactly five ASCII letters after surrounding whitespace is trimmed. Entries
are converted to uppercase; blank and invalid entries are ignored.

The server requires at least one valid entry and accepts at most 1000.
A missing bank, a bank with no valid entries, more than 1000 valid entries,
or a bank read error causes startup to fail. The bank is loaded at startup;
restart the server after changing it.

## Running the client

Usage:

```text
./client hostname [port]
```

In another terminal on the same machine, start a client on the default port:

```sh
./client localhost
```

For the custom-port server above, this same-machine IPv4 example uses 54000:

```sh
./client 127.0.0.1 54000
```

For a server on a different machine, replace the hostname with that server's
hostname or IPv4 address. Use the same port as the server. Open additional
client terminals to play simultaneously.

### Playing

- Enter five letters per guess. Lowercase input is converted to uppercase,
  and surrounding whitespace is trimmed.
- Invalid input does not use an attempt. A five-letter guess is accepted
  without a dictionary-membership check.
- `G` means the letter is in the correct position.
- `Y` means a matching copy of the letter is in another position.
- `-` means there is no remaining matching copy. Exact matches are counted
  first, so repeated letters do not receive more credit than the answer contains.
- The board shows all six rows, guesses remaining and the unguessed alphabet.
- A correct guess wins. After six incorrect valid guesses, the client reveals
  the answer and displays a loss.
- At the replay prompt, enter `y` to start another round on the same connection
  or `n` to quit. Uppercase Y and N also work.
- Enter `:quit` at a guess or replay prompt to quit. End-of-input also ends
  the client session.

## Static library and shared functions

The project uses a **static library**, `libwordle.a`. Linking the required
common code into each executable avoids a separate runtime shared-library
search path on acad. Rebuilding the library relinks dependent programs through
the Makefile.

| Function | Shared purpose |
| --- | --- |
| `parse_port()` | Validate decimal port input, range and overflow |
| `send_all()` | Complete partial sends and retry interrupted sends |
| `send_line()` | Validate frame text and append one LF byte |
| `recv_line()` | Assemble a complete line and retain bytes for the next message |
| `parse_message()` | Validate exact protocol syntax |
| `normalize_word()` | Trim, uppercase and validate five ASCII letters |

Each connected socket has its own reader state. Game scoring stays in the
client; bank loading and answer selection stay in the server.

## Protocol and complete-message handling

Messages use printable ASCII text terminated by one LF byte. In the examples
below, `\n` represents that terminating byte.

| Direction | Wire-message example | Meaning |
| --- | --- | --- |
| Server to client | `HELLO\n` | Greeting after connection |
| Client to server | `READY\n` or `WORD\n` | Request an answer |
| Server to client | `WORD APPLE\n` | Reply with exactly five uppercase letters |
| Client to server | `BYE\n` or `QUIT\n` | End the session |
| Server to client | `ERROR BAD_COMMAND\n` | Invalid command or message direction |
| Server to client | `ERROR BAD_FRAME\n` | Malformed, oversized or truncated frame, when a final reply can be sent |

The client normally sends READY and BYE. The server also accepts WORD and
QUIT as the required aliases. A bare WORD is a request; WORD followed by one
space and five uppercase letters is an answer. Keywords and spacing are exact.
The ERROR message syntax is `ERROR <reason>`, with a nonempty printable reason.

After HELLO, the client sends READY and waits for a WORD reply. Guesses and
scoring are local; they are not sent to the server. Replay sends another READY
over the existing connection and resets the board, attempts and alphabet.
On a normal quit, the client sends BYE and closes. The server child closes
its connected socket and exits; the parent continues accepting clients.
There is no BYE acknowledgment.

TCP can split a message between reads or combine several messages in one read.
`send_all()` loops until all bytes are sent and retries EINTR.
`recv_line()` waits for LF and preserves extra bytes in the connection's
reader, so both programs know when one complete message has arrived.

The maximum frame is **128 bytes including LF**, leaving up to 127 text bytes.
The receiving program also reserves a local terminating NUL; that NUL is
never transmitted. CR, NUL and other control or non-ASCII bytes are invalid
inside a frame. Invalid commands receive ERROR BAD_COMMAND; framing errors
end the affected connection, with ERROR BAD_FRAME attempted first.
The client reports ERROR, unexpected replies and connection loss, then closes.
The library imposes no idle timeout while a player is thinking.

## Tests and recorded acad results

With the game server stopped, run:

```sh
make programs
make test
python3 tests/test_client.py
```

Stopping the game server first leaves the default port available to the client
test harness. The Python suite uses a local test peer; real client/server
integration was checked separately.

The final acad terminal results supplied by Cherry on November 22, 2026,
used application commit `81f567241544fa1c987ca5fe6e016194804a3674`.

| Check | Recorded result |
| --- | --- |
| Clean client/server/library build | Successful; no warnings or errors shown |
| Library suite | 21 cases and 278 checks passed |
| Client suite | 27 tests run: 26 passed, one skipped, no failures |
| Real server and client | Default/custom ports, invalid guesses, letter feedback, repeated letters, lowercase input, win/loss, replay/reset and quitting passed |
| Concurrent sessions and cleanup | Two overlapping clients, independent games, continued play after another client quit, child reaping and shutdown with active clients passed |
| Startup errors | Invalid client/server ports and missing/empty bank checks passed |

See [Task 3 test summary](docs/TASK3_TEST_SUMMARY.md) for inputs,
expected/actual results, final regression counts and testing limits.
Earlier handoff and component-status documents describe earlier project stages;
this README and the Task 3 summary describe the integrated application.

## Known issues and limits

- The server listens on IPv4. The client can try addresses returned for a
  hostname; use an IPv4 server address where needed.
- The acad client suite skipped its IPv6 test with
  `[Errno 99] Cannot assign requested address`. This is recorded as a skip,
  not a passed IPv6 check.
- Some manual connection attempts during stop/restart steps failed with
  Connection refused; one earlier localhost attempt reported Cannot assign
  requested address. The exact cause of each attempt was not isolated.
  Later connections and the recorded acceptance checks succeeded. Connection
  observations are retained in the Task 3 summary.
- Random selection may choose the same word again. A one-word test bank always
  returns that word.
- The executed IPv4 acceptance checks showed no reproducible gameplay defect.
  These results describe the recorded test scope.

## Teamwork and completion

The Gantt chart divided the project into design, component implementation,
integration/testing, and documentation/submission. The team assigned a client
owner, server owner and shared-library/documentation owner. Defining the common
header and protocol allowed the components to use the same validation and
message framing. The separately developed components were integrated through
the library and stored in the team's GitHub repository.

Cherry ran the recorded final acad checks and documented the results.
All three members are assigned to contribute to and proofread the README,
with Charanya responsible for its completion. Each member records their own
actual work start/end times and hours in the Gantt chart.

Class-example adaptations and their source mapping for the client and its
test harness are documented in `client.c` and `docs/CLIENT_SOURCES.md`.

## Final submission

The application deadline is November 23, 2026, at 11:59 p.m.
The submission must be a ZIP containing the project directory with the README,
all source/header files, Makefile and the normal word bank needed to run it.

After the final review, from inside the completed `cpsc328wordle` directory:

```sh
make clean
cd ..
zip -r group17_wordle.zip cpsc328wordle
```

Submit `group17_wordle.zip` through `turnin328` on acad for project `wordle`.
Submit the completed Gantt separately to D2L, with task owners, estimated and
actual hours, and both estimated and actual start/end times.

