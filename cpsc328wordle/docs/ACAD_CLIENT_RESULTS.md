# ACAD verification - tasks 2.1-2.8

Evidence: terminal output supplied by Shivamani Jadi after running the
review package on ACAD. These results were reported by Shiva; this record
does not claim a separate remote login or execution by the assistant.

## Commands executed

```sh
make clean
make client
make test
python3 tests/test_client.py
```

## Observed results

| Check | Result |
| --- | --- |
| C client and unchanged static library | Compiled and linked successfully, with no warnings shown. |
| Cherry's original library tests | 21 cases passed, 278 checks. |
| Client test suite | Ran 27 tests: 26 passed, one skipped, no failures. |
| IPv6 test | Skipped: [Errno 99] Cannot assign requested address. |
| Localhost and default port 53280 | Passed. |

The terminal summary was `OK (skipped=1)`. The skipped test could not bind
the IPv6 loopback address on ACAD. It does not establish an IPv6 client
failure. IPv4/localhost, repeated-letter scoring, board/alphabet, replay,
:quit, protocol validation and cleanup checks passed.

Cherry's shared source/header, Makefile, original tests and handoff records
are preserved. Shiva's implementation is in client.c, with separate client
tests and source citations. These results support component completion
through task 2.8.

Giorgi's tasks 2.9-2.12 remain: the word bank, listening socket, forked client
sessions, signals/reaping and server integration. The server starter is
unchanged. Full application integration, concurrency and server shutdown
tests remain for the next team stage.
