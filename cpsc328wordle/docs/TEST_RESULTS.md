# Library verification results

Executed: November 12, 2026, 02:10 PM EST.
Environment: Linux, GCC 14.2.0, GNU Make and GNU binutils.

## Reproducible commands

```sh
make clean
make -j2 CFLAGS='-std=c11 -Wall -Wextra -Wpedantic -Werror -O2' all programs test
ASAN_OPTIONS=detect_leaks=0 make sanitize
ar t libwordle.a
nm -g --defined-only libwordle.a
```

## Observed results

- Strict C11 build: no compiler warnings or errors.
- Static archive created; the tests link against libwordle.a.
- Client/server starter files compile with the provided link targets.
- Normal suite: all 21 test cases passed, with 278 checks.
- AddressSanitizer/UndefinedBehaviorSanitizer suite: all 21 cases passed,
  with no reported address or undefined-behavior errors.
- Leak detection was not completed. Running make sanitize with default
  options encountered an environment error from LeakSanitizer under ptrace.
  The successful run used detect_leaks=0; it must not be presented as a
  successful leak check.

See test-output.txt and sanitizer-output.txt for the captured successful runs.

## Inputs and expected results tested

| Case | Inputs/setup | Expected result | Actual |
| --- | --- | --- | --- |
| 1 | Ports 1024, 65535, 53280, 0053280; invalid text/overflow | Valid ports accepted; others rejected | Pass |
| 2 | Mixed/lowercase and padded APPLE; CAT, APPLES, AB12!, non-ASCII | Exactly five ASCII letters normalized; invalid values rejected | Pass |
| 3 | HELLO, READY, WORD, WORD APPLE, BYE, QUIT, ERROR reasons; malformed variants | Exact syntax accepted; extra/wrong tokens rejected | Pass |
| 4 | send limited to two bytes; first send interrupted | All WORD APPLE bytes plus one LF received | Pass |
| 5 | Overlong/control-character frames, NULL inputs, bad descriptor | Reject invalid send without transmitting a prefix | Pass |
| 6 | Receiver closed before HELLO | Return an error; no SIGPIPE process termination | Pass |
| 7 | recv limited to two bytes; first receive interrupted | Complete WORD APPLE returned without LF | Pass |
| 8 | HELLO, READY, BYE in one stream write | Return three separate ordered lines; retain remainder | Pass |
| 9 | HELLO and partial WORD AP, then PLE and LF | Preserve partial second line until complete | Pass |
| 10 | 127 printable bytes plus LF | Accept the maximum frame and NUL-terminate output | Pass |
| 11 | 128 printable bytes plus LF | Report frame too long; do not recover on that reader | Pass |
| 12 | HELLO into a four-byte output array | Report insufficient capacity without accepting a prefix | Pass |
| 13 | Embedded NUL, CR, tab or non-ASCII byte | Report invalid framing | Pass |
| 14 | EOF with no partial line | Report clean EOF | Pass |
| 15 | HELLO without LF, followed by EOF | Report truncated frame | Pass |
| 16 | Partial HE, SO_RCVTIMEO set, no completion | Report timeout without accepting partial input | Pass |
| 17 | Invalid reader/socket/output arguments | Return error safely | Pass |
| 18 | Empty LF-only line | Framing succeeds; protocol parsing rejects it | Pass |
| 19 | Two separate sockets/readers with different pending lines | No mixing between connection buffers | Pass |
| 20 | 512 KiB data sent to a separate process with short sends | All bytes arrive in order with expected values | Pass |
| 21 | Two-endpoint HELLO/READY/WORD exchange, second request WORD, then QUIT | Both aliases and repeated rounds parse correctly | Pass |

## Scope of evidence

Tests use real local stream sockets (socketpair) to exercise byte-stream
communication. Test-only GNU linker wrappers deterministically cap the amount
sent/received and inject EINTR. The large-data test uses a separate receiving
process. Tests do not replace send/recv with an entirely simulated transport.

The source library and the static archive are checked independently by the
sanitized and normal builds. The suite has a 20-second watchdog.

These are library tests, not full application acceptance tests. No acad login,
remote-host connection, actual game scoring, complete concurrent server,
word-bank handling, Ctrl+C shutdown or final submission was tested. Those
remain integration work after the client and server are implemented.
