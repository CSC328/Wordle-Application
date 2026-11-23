# Group 17 - Task 3 Test Summary

CPSC 328 Network and Secure Programming, Professor Walther.

This summary covers terminal results supplied by Charanya Ananthula on
November 22, 2026, for application commit
`81f567241544fa1c987ca5fe6e016194804a3674`.
The final checks ran in the university acad Linux environment using IPv4.
The team retains the detailed terminal evidence separately.

## Final build and regression

| Check | Actual result |
| --- | --- |
| Clean client, server and static-library build | Successful; no compiler warnings or errors shown |
| Library suite | All 21 cases passed, totaling 278 checks |
| Client suite | 27 tests run: 26 passed, one skipped, no failures |
| IPv6 test | Skipped with `[Errno 99] Cannot assign requested address` |

The client Python suite uses a local test peer. The real C client and C server
were also exercised together in the manual checks below.

## Nine design tests

A separate controlled bank containing only APPLE made the gameplay checks
repeatable.

| Test | Input or action | Expected result | Actual result |
| --- | --- | --- | --- |
| Default startup and connection | Start server and client with the port omitted | Connect on 53280, receive greeting and answer | Passed |
| Invalid port | Start client and server with 70000 | Reject the port and return to the shell | Passed |
| Missing bank | Start server from a test directory without words.txt | Error and exit | Passed |
| Invalid guesses | Enter CAT and, separately, AB12! | Reject without using an attempt | Passed; prompt remained Guess 1 of 6 |
| Letter feedback | Answer APPLE; guesses APRON and ALLEY | APRON: G G - - -. ALLEY: G Y - Y - | Both rows matched; board and alphabet updated |
| Win and replay | Guess lowercase apple; separately run APPLE, y, APPLE, n | Win; replay on the same connection with reset state; quit | Passed; two rounds, reset board, six guesses and full alphabet |
| Loss and quit | Six accepted wrong guesses, then n | Reveal APPLE after six attempts; disconnect | Passed; sixth accepted guess was STONE |
| Simultaneous clients | A guesses APRON and quits; B guesses ALLEY, then APPLE | Independent games; B continues after A exits | Passed; server connection order confirmed overlapping sessions |
| Active-session shutdown | Two clients wait at guess prompts; interrupt the server | Both detect disconnect; parent and children exit | Passed; both reported disconnect and no server processes remained |

## Additional checks

- Custom-port connections on 54000 succeeded.
- An empty word bank produced an error and the server exited.
- The child process disappeared after normal client quit while the same
  server parent remained; no zombie was shown.
- The server accepted another connection after an abruptly interrupted client.
- Letter counts, lowercase handling, remaining attempts and the unguessed
  alphabet behaved as expected.

## Observations and scope

The IPv6 skip is not counted as a pass. The recorded integration checks
establish the tested IPv4 behavior.

Some manual connection attempts around server stop/restart steps failed with
Connection refused; one earlier localhost attempt reported Cannot assign
requested address. The cause of each attempt was not isolated. Later
connections and the recorded acceptance checks succeeded. Keep the server
listening before initiating a client connection.

Pasted or joined input caused extra retry prompts during one manual loss
test. The client still lost after six accepted wrong guesses and quit on n.
Replay was subsequently verified with the exact scripted sequence
APPLE, y, APPLE, n in one client invocation.

## Repeating the automated checks

With the game server stopped, from the project directory:

```sh
make clean
make programs
make test
python3 tests/test_client.py
```

The final regression finished without test failures. Team review and the
members' real Gantt start/end times and actual hours are recorded separately.

