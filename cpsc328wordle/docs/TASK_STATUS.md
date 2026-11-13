# Completion record for tasks 2.1-2.4

Prepared November 12, 2026. Source: Wordle_Project_Gantt.xlsx, Gantt Chart,
rows 27-30; Group_17_Network_Program_Design_Final.pdf, sections 4-8 and 12;
and the professor's application/turn-in instructions.

| Task | Owner in Gantt | Original estimated hours | Status | Evidence |
| --- | --- | --- | --- | --- |
| 2.1 | Cherry | 2 | Completed for handoff | make builds libwordle.a; make programs compiles clearly marked starters |
| 2.2 | Cherry | 6 | Completed for handoff | send_all, send_line, line_reader_init and recv_line; partial I/O/error tests |
| 2.3 | Cherry | 4 | Completed for handoff | parse_port, parse_message and normalize_word; validation tests |
| 2.4 | Cherry | 4 | Completed for handoff | 21 test cases / 278 checks passed; test source and logs included |

Only these four implementation tasks are claimed complete. Rows 31 onward
remain with their assigned owners. Client/server starters are build scaffolds,
not completed tasks 2.5-2.12. These files were created without changing the
submitted final design, original Gantt, or team assessment.

## Gantt update to record after reviewing this handoff

For tasks 2.1-2.4, set progress in D27:D30 to 100% after accepting the work.
Enter actual work times in the chart's actual-start/actual-end/actual-hours
fields using your real student/team work records. Keep the original planned
dates and estimates. No historical dates or student effort were filled in here.
Any zero-valued midnight timestamp does not establish the actual time of work.

The professor requires estimated and actual start/end times in the final
Gantt as well as estimated/actual effort and task owners. Check the final
chart for that detail; the source chart currently shows date-only planned
entries for these four tasks. This package does not claim the final Gantt is
ready to submit.

## Requirement coverage

| Requirement | What is done now | Remaining application responsibility |
| --- | --- | --- |
| Common static library in C/Linux | Six functions in libwordle.a, header and Makefile | Call it from both real programs |
| Partial TCP sends | send_all loops; send_line appends LF | Check errors and close connection |
| Complete messages | Buffered LF receiver, remainder retained per connection | Create/reuse a reader per connected client |
| Input validation | Decimal 1024-65535, five-letter word normalization | Check argc, hostname, terminal line length and bank contents |
| Protocol | Greeting, requests, replies, ERROR and both aliases parsed | Enforce message direction/state and server error policy |
| Error-path tests | EOF, truncation, invalid bytes, size, timeout, broken peer | Integration/concurrency tests on acad |
| Submission rules | Correct final group17_wordle.zip instructions in README | Complete the game, README review, Gantt and final ZIP |

The team assessment contains personal evaluations. It was not edited or
reused as implementation evidence.
