# Group 17 - Network Wordle Application

CPSC 328, Professor Walther.

The project is in **[cpsc328wordle/](cpsc328wordle/)**. Tasks **2.1-2.4** (shared library, project setup, validation and library tests) are complete. The client and server are starter files for the remaining team tasks; the full game is not playable yet.

## Build and test

Use Linux with GCC, GNU Make and GNU binutils:

```sh
git clone https://github.com/CSC328/Wordle-Application.git
cd Wordle-Application/cpsc328wordle
make
make test
```

## Team

| Member | Responsibility |
| --- | --- |
| Shivamani Jadi - project leader | Client, tasks 2.5-2.8 |
| Giorgi Kuparadze | Server, tasks 2.9-2.12 |
| Charanya Ananthula | Shared library, tasks 2.1-2.4, documentation |

Read the **[handoff guide](cpsc328wordle/HANDOFF.md)** before implementing the client or server. See the **[project README](cpsc328wordle/README.md)** for protocol details and submission instructions, and **[test results](cpsc328wordle/docs/TEST_RESULTS.md)** for the verification scope.

The shared-library test suite passed 21 cases with 278 checks. The final design uses C, a static `libwordle.a`, TCP, a fork-based server, and default port **53280**. Full client/server integration, testing on acad and actual Gantt times remain team work.
