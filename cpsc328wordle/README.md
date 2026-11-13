# Group 17 - Network Wordle

CPSC 328, Professor Walther. Tasks 2.1-2.4 handoff, November 12, 2026.

## Current status

The shared C library, project setup, Makefile and library tests are complete.
`client.c` and `server.c` are explicitly marked starters for the remaining tasks.
They currently print an unfinished-work message and exit with failure. This is
not a playable Wordle application or the final application submission.

## Team and responsibilities

| Member | Primary responsibility |
| --- | --- |
| **Shivamani Jadi - project leader** | Client, game logic, terminal interface, integration, client tests |
| Giorgi Kuparadze | Server, word bank, concurrent child processes, server tests |
| Charanya Ananthula | Shared library, README, documentation, test records |

## Project description

The completed application will let several players play Wordle independently
through a concurrent TCP server. The server holds the word bank and sends a
random five-letter word. The client handles guesses, six attempts, duplicate
letters, feedback, the unguessed alphabet, replay and quitting.

This package follows `Group_17_Network_Program_Design_Final.pdf`: C on Linux,
POSIX sockets, a static library, a fork-based server, and default port 53280.
The Gantt chart's task numbers 2.1-2.4 identify the work completed here.

## Files

- `wordle_common.h`: shared constants, types and function declarations.
- `wordle_common.c`: the six common functions and reader initialization.
- `Makefile`: library, client, server, test and cleanup targets.
- `tests/test_common.c`: executable tests for library communication and errors.
- `client.c`, `server.c`: teammate-owned starter files to replace.
- `HANDOFF.md`: API contracts, example calls and remaining responsibilities.
- `docs/TEST_RESULTS.md`: tests performed, results and limits.
- `docs/test-output.txt`, `docs/sanitizer-output.txt`: captured verification output.
- `docs/TASK_STATUS.md`: task mapping and Gantt recording instructions.

## Build the library and run tests

Use a Linux shell with GCC, GNU Make, GNU binutils (`ar` and the GNU linker),
and standard Linux C headers. Windows users should use the university's Linux
environment or WSL, rather than running these commands directly in PowerShell.

```sh
cd cpsc328wordle
make
make test
```

`make` creates `wordle_common.o` and `libwordle.a`. The archive is generated
locally so it matches the machine where the project will run.
`make test` links against that archive and runs the library test suite.

Other commands:

```sh
make programs       # Builds client and server; both are currently starters.
make client         # Builds only the client and its library dependency.
make server         # Builds only the server and its library dependency.
make sanitize       # AddressSanitizer and UndefinedBehaviorSanitizer.
make clean          # Removes generated objects, archive and executables.
```

The test target alone uses GNU linker wrappers to force partial I/O and EINTR.
Those wrappers are not present in the production library or program links.
On an environment where LeakSanitizer is unavailable, the other sanitizer
checks can be run with `ASAN_OPTIONS=detect_leaks=0 make sanitize`. That disables
leak checking; see `docs/TEST_RESULTS.md` for exactly what was verified here.

## Compile and run the completed programs

After Shiva and Giorgi replace their starters:

```sh
make programs
./server
./client localhost
```

For a custom port, use the same value on both sides:

```sh
./server 54000
./client localhost 54000
```

The server accepts `server [port]`; the client accepts
`client hostname [port]`. Both use 53280 when the port is omitted.
Explicit ports must contain decimal digits only and be in 1024-65535.
The future server must load `words.txt` from its working directory and reject
a missing or empty valid-word bank. The future client must reject missing
hostname or extra arguments. These startup behaviors belong to the later
client/server tasks, not the completed common library.

## Static library choice

`libwordle.a` is a static library. Linking its required code into each program
avoids needing a separate runtime shared-library path on acad. Both programs
include `wordle_common.h` and link with `-L. -lwordle` after their object files.
Changing the shared source requires rebuilding the archive and relinking both
programs; the Makefile handles those dependencies.

## Protocol and complete-message handling

| Direction | Message before the terminating newline | Meaning |
| --- | --- | --- |
| Server to client | `HELLO` | Session greeting after connection |
| Client to server | `READY` or `WORD` | Request one random word |
| Server to client | `WORD APPLE` | Five-letter uppercase answer for the round |
| Client to server | `BYE` or `QUIT` | End this session |
| Server to client | `ERROR BAD_COMMAND` | Example of `ERROR <reason>` |

The normal client should send READY and BYE; the server must also accept WORD
and QUIT as required aliases. A bare WORD is a request; WORD followed by one
space and exactly five uppercase letters is a response.

Each wire message ends with exactly one LF byte (`\n`). Call `send_line()`
without a newline; it appends LF and calls `send_all()`. Never transmit a C
string's terminating NUL. `send_all()` continues after partial writes and
retries EINTR. Linux MSG_NOSIGNAL prevents a broken peer from killing the
process through SIGPIPE.

`recv_line()` reads until LF and preserves any bytes after that line in a
per-connection reader. This handles both fragmented and combined messages.
It returns text without LF, plus a local terminating NUL. A single send/recv
call does not define a message boundary.

Implementation limits chosen for this library: a frame is at most 128 wire
bytes including LF, so text is at most 127 printable ASCII bytes. Use
`char line[MAX_FRAME]`: 127 text bytes plus NUL fit. CR, embedded NUL, tabs and
other control/non-ASCII bytes are invalid in a wire frame. Blank lines are
complete frames but invalid protocol messages. ERROR reasons may contain
internal spaces but must be nonempty and have no surrounding spaces.

Message keywords are uppercase and exact. `normalize_word()` accepts lowercase
player/bank input; `parse_message()` requires an uppercase five-letter WORD
payload so malformed replies are not silently corrected.

`parse_message()` checks message syntax. The client/server must separately
check message direction and session state. The client expects HELLO first,
then a WORD reply for each READY; ERROR or an unexpected reply ends that
client session. The server accepts word requests and quit commands from its
client; unexpected commands should produce an ERROR response. The library
does not decide whether a server retries or closes after such an error.

After receiving the word, the client plays locally without sending guesses
or results to the server. Replay sends another READY over the same connection.
Quitting sends BYE before closing when the connection still works. The server
child then closes its connected socket and exits; the parent keeps listening.
No BYE acknowledgment is required by this protocol.

On clean EOF or any terminal receive error, close the affected session.
Do not call `recv_line()` to recover after a malformed or partial frame.
No time limit is imposed by the library. It works with blocking sockets;
if a caller sets SO_RCVTIMEO, timeout is reported distinctly. In particular,
do not impose an idle timeout while the player is thinking unless the team
explicitly revises its design.

## Known limitations and remaining work

No known failing library cases were found in the executed tests. The client,
word-bank server, fork handling, signals/reaping, gameplay and full-system
tests are still outstanding. `words.txt` is not supplied because that is part
of Giorgi's word-bank task. Testing was performed on the available Linux
workspace, not on acad. The team must rebuild and test on acad before final
submission. This package does not establish that the complete application
meets its acceptance tests.

## Teamwork and time records

The final design assigns client, server and common-library work to the three
members above. The documented communication plan uses Telegram, Google Meet
and GitHub. The Gantt chart breaks work into design, implementation, integration,
testing and submission. This handoff supplies a stable common API so the client
and server owners can implement their parts independently.

All members still need to review the integrated code and final README and
record their actual work start/end times and hours. No historical meetings,
student work hours, completed future tasks or acad test results are asserted
by this package. Fill those records using what actually happened.

## Final submission after all remaining tasks are complete

The application is due November 23, 2026, at 11:59 p.m. The professor requires
a ZIP with a directory containing the README, all source files, Makefile and
the word bank needed to run the program. For this group the final name is
case-sensitive: `group17_wordle.zip`.

From the parent of the completed project directory:

```sh
cd cpsc328wordle
make clean
make programs test
# Perform the remaining full-application tests on acad.
make clean
cd ..
zip -r group17_wordle.zip cpsc328wordle
```

Submit the finished ZIP through `turnin328` on acad for project `wordle`.
Submit the completed Gantt separately to D2L, including owners, estimates,
actual hours, and estimated/actual start and end times. The task-handoff ZIP
provided now must not be submitted as the finished game.
