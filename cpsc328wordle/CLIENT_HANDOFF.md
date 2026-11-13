# Shiva - client tasks 2.5-2.8

The client is implemented and passed the local checks listed below. Shiva also
rebuilt and tested it on acad; see docs/ACAD_CLIENT_RESULTS.md. The server is still Giorgi's
starter, so the complete game still needs client/server integration.

## Completed client work

| Task | Implementation |
| --- | --- |
| 2.5 - Arguments and client connect | Requires a hostname, accepts an optional decimal port, uses 53280 by default, resolves the host, and opens a TCP connection. |
| 2.6 - Guesses and repeated-letter scoring | Accepts five letters, normalizes lowercase input, rejects invalid guesses without using an attempt, and scores exact matches before remaining letter copies. |
| 2.7 - Board, alphabet and replay | Shows six numbered rows, G/Y/- feedback, guesses remaining, unguessed letters, win/loss messages, and a validated y/n replay prompt. Each replay resets the round. |
| 2.8 - Client protocol and cleanup | Requires HELLO, sends READY, requires WORD, and sends BYE on n, :quit or input EOF. Reuses one LineReader and closes the connection on protocol or I/O errors. |

Guesses are processed locally. Only READY and BYE are sent by this client.
The answer is shown after a loss; it is not printed when the word is received.
The client also checks for server disconnection while waiting for a guess or
replay answer. It does not put a time limit on the player's turn.
Enter `:quit` at either prompt to leave, as shown in the client flowchart.

## Repository review

Reviewed all 14 tracked files in CSC328/Wordle-Application at commit
`54cf66f93f3c8c94e24dd85240c94eee58cb04e2`. The seven uploaded implementation
and handoff files matched that repository exactly.

| Existing file | Review and treatment |
| --- | --- |
| Repository README.md | Checked the project directory, ownership and build instructions; retained. |
| cpsc328wordle/.gitignore | Checked build-output exclusions; retained. |
| cpsc328wordle/HANDOFF.md | Followed the shared API, message order, input rules and task ownership; retained. |
| cpsc328wordle/Makefile | Used the existing client, static-library and test targets; retained. |
| cpsc328wordle/README.md | Checked requirements, framing, aliases, limits and submission instructions; retained. |
| cpsc328wordle/client.c | Replaced the starter with tasks 2.5-2.8. |
| cpsc328wordle/docs/TASK_STATUS.md | Checked the completed 2.1-2.4 record; retained. |
| cpsc328wordle/docs/TEST_RESULTS.md | Checked the earlier verification scope and sanitizer limitation; retained. |
| cpsc328wordle/docs/test-output.txt | Checked the 21-case, 278-check record; retained. |
| cpsc328wordle/docs/sanitizer-output.txt | Checked the earlier sanitizer record; retained. |
| cpsc328wordle/server.c | Confirmed it is Giorgi's starter for 2.9-2.12; retained. |
| cpsc328wordle/tests/test_common.c | Read and reran the existing tests; retained. |
| cpsc328wordle/wordle_common.c | Reused validation, framing, sending and receiving; retained. |
| cpsc328wordle/wordle_common.h | Used its constants, types and function declarations; retained. |

The original READMEs and HANDOFF describe the earlier library handoff. This
file records the added client work. All existing files except client.c are
byte-for-byte unchanged, including Cherry's library and tests.

The Ponytail reference was reviewed for its reuse and minimal-change guidance.
The client keeps the repository's C style: small static helpers, snake_case
names, fixed-size arrays, and explicit error checks. It uses the existing
shared functions rather than adding another implementation of them.

The additional P-5.zip course examples were reviewed before finalizing the
client. Address lookup and connection follow client_allAddrs.c; input waiting
uses the select pattern from daytimed.c. The file header and adapted sections
have citations in the same format as the earlier Pipes assignment.
See docs/CLIENT_SOURCES.md for the exact archive paths and authors.

## Check on acad

The ZIP includes the project sources, existing library tests, new client
tests and captured results. It contains no compiled executables or archives.

From the extracted review directory:

```sh
cd Wordle_Application_Client_Review/cpsc328wordle
make client
make test
python3 tests/test_client.py
```

Expected results: the client builds, the existing library suite passes
21 cases / 278 checks, and the client suite passes 27 tests. The Python tests
start their own temporary local TCP peer, so Giorgi's server and words.txt
are not needed for these checks. Python 3.7 or newer is used only for testing.
The application itself remains C.

The default-port and IPv6 checks report a skip if that port or IPv6 is not
available. All 27 tests ran without skips in the recorded workspace run.

After Giorgi's server is ready, verify the normal run interface too:

```sh
./client localhost
./client localhost 54000
```

Start the real server separately on the matching port. Running the current
server.c starter is not an integration test.

## Files to update in your GitHub checkout

Replace cpsc328wordle/client.c and add the client test, this handoff and the
new docs/client-* logs plus docs/CLIENT_TEST_RESULTS.md. Keep any newer team
changes when copying into your checkout.

The supplied patch contains exactly those changes relative to the reviewed
commit. From your repository root, you can check and apply it with:

```sh
git apply --check /path/to/Wordle_Shiva_Client_Tasks_2_5_to_2_8.patch
git apply /path/to/Wordle_Shiva_Client_Tasks_2_5_to_2_8.patch
```

Review the diff and run the checks on acad before committing and pushing.
This client update is being added to the team repository after Shiva's acad
review. Record actual Gantt times from your own work records.

See docs/CLIENT_TEST_RESULTS.md for the executed checks and remaining scope.
