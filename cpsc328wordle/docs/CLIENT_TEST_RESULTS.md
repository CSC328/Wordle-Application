# Client verification results - tasks 2.5-2.8

Executed November 12, 2026, in the available Linux workspace.
Environment: Linux x86_64, GCC 14.2.0, glibc 2.41 and Python 3.12.14.
These are local results, not acad results.
Shiva's later acad run is recorded separately in ACAD_CLIENT_RESULTS.md.

## Build and library regression

```sh
make -j2 client test CFLAGS='-std=c11 -Wall -Wextra -Wpedantic -Werror -O2'
python3 tests/test_client.py
```

- The client and unchanged library built with no warnings or errors.
- The client linked to the static libwordle.a using the existing Makefile.
- The original library suite passed 21 cases / 278 checks.
- The client suite passed all 27 tests, with no skipped tests.

Captured output: client-build-output.txt and client-test-output.txt.

## Client checks

The tests run the actual compiled C client against a small Python TCP peer
on loopback. That peer supplies known answers, records client messages and
checks that the socket closes. It is a test fixture, not the team's server.

| Area | Verified behavior |
| --- | --- |
| Arguments | Missing hostname, empty hostname and extra arguments fail with usage. |
| Ports and hostnames | Default 53280, custom port, leading zeroes, invalid ports, hostname failure and refused connection. |
| Address handling | localhost and direct IPv4/IPv6 loopback connections. |
| Guess validation | Short/long words, digits, spaces inside words, blank lines, embedded NUL, non-ASCII input and overlong terminal lines do not consume guesses. |
| Normalization | Lowercase/mixed-case guesses and surrounding terminal whitespace are accepted. |
| Scoring | The design's APPLE/APRON case gives G G - - -; 16 additional repeated-letter pairs verify exact-match priority and copy limits. |
| Board and alphabet | Six numbered rows, feedback, remaining count, removal of all guessed letters and the empty-alphabet case. |
| Result | Correct guesses win, including the sixth guess; six wrong guesses lose and reveal the answer. |
| Replay | Uppercase/lowercase choices, surrounding whitespace, invalid choices, one socket across rounds and fresh board/alphabet state. |
| Hidden answer | Receiving the answer and making an incorrect guess do not print the secret word. |
| Protocol | HELLO before READY, WORD payload validation, ERROR, wrong-direction messages, fragmented replies and buffered extra replies. |
| Framing | Truncated replies, invalid bytes, missing LF, oversized replies and a valid maximum-size ERROR frame. |
| Cleanup | n, :quit and EOF during guesses/replay send BYE and close; a final unterminated input line is handled. Unexpected replies and server disconnects close with failure. |
| Waiting for input | Disconnection, ERROR and unexpected replies are detected at both the guess and replay prompts while stdin stays open. |
| Closed terminal input | A launch with stdin already closed fails safely if its descriptor is reused by the socket. |
| Network ownership | Guesses and results stay local; the fixture receives only READY and BYE. |

For APPLE against PAPAL, the expected feedback is Y Y G - Y. For APPLE
against PPPPP, it is - G G - -. These cases check that a matching copy is
not counted twice.

## Address and undefined-behavior checks

A separate build directory was used, with the same client, shared source,
header and Makefile:

```sh
make -B client \
  CFLAGS='-std=c11 -Wall -Wextra -Wpedantic -Werror -g -O1 -fsanitize=address,undefined -fno-omit-frame-pointer -fno-pie' \
  LDFLAGS='-fsanitize=address,undefined -no-pie'
WORDLE_CLIENT=/absolute/path/to/sanitized/client \
  ASAN_OPTIONS=detect_leaks=0 python3 tests/test_client.py
```

All 27 client tests passed without reported address or undefined-behavior
errors. Captured output: client-sanitizer-build-output.txt and
client-sanitizer-output.txt. Leak detection was disabled, so this is not
evidence of a successful leak check.

## Remaining team verification

The real server is still the existing starter. Its listener, word bank,
forked sessions, signal handling and child cleanup belong to tasks 2.9-2.12.
Full client/server integration, simultaneous clients, server shutdown and
the remote acad run have not been verified by this client test suite.

Network reads use the existing blocking LineReader contract and do not add
a message or player-idle deadline. Normal quitting and input EOF send BYE;
an external process signal uses the operating system's descriptor cleanup.
Input waiting uses the class example's select pattern. A socket descriptor
outside select's FD_SETSIZE range produces an error before any FD_SET call.

Course-source references are recorded in the client header, inline adapted
sections and CLIENT_SOURCES.md. The course archive's compiled example
programs were not used in the build.

No Gantt hours, historical completion dates, final submission results or
GitHub pushes are claimed here.
