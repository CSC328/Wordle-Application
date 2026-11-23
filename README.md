# Group 17 - Network Wordle Application

CPSC 328 Network and Secure Programming, Professor Walther.

The C client, concurrent TCP server and static `libwordle.a` are in
**[cpsc328wordle/](cpsc328wordle/)**. The server uses `fork()` for independent
client sessions. The default port is **53280**.

## Build and run

Use Linux with GCC, GNU Make and GNU binutils:

```sh
git clone https://github.com/CSC328/Wordle-Application.git
cd Wordle-Application/cpsc328wordle
make programs
```

Start `./server` from that directory so it can read `words.txt`.
In another terminal on the same machine, run `./client localhost`.

## Team

| Member | Responsibility |
| --- | --- |
| **Shivamani Jadi - project leader** | Client and game logic |
| Giorgi Kuparadze | Server, word bank and concurrent sessions |
| Charanya Ananthula | Shared library, README, documentation and test records |

## Documentation and tests

- [Application README](cpsc328wordle/README.md): build/run instructions, gameplay, library choice, protocol, known issues and submission.
- [Task 3 test summary](cpsc328wordle/docs/TASK3_TEST_SUMMARY.md): final acad verification and manual integration results.

On November 22, 2026, the recorded acad build succeeded; the library passed
21 cases / 278 checks; the client suite had 26 passes, one IPv6 environment
skip and no failures. Real client/server gameplay, replay, concurrency,
process cleanup, shutdown and startup-error checks are recorded in Task 3.

Earlier handoff documents preserve component-stage history. Final team review,
Gantt actuals and submission remain separate handoff steps.

