# Tasks 2.1-2.4 - teammate handoff

The library is ready for client/server development. Start by extracting the ZIP,
opening a Linux terminal in `cpsc328wordle`, and running:

```sh
make
make test
```

## Completed tasks

| Gantt task | Delivered work |
| --- | --- |
| 2.1 - Set up C project and Makefile | Shared header/source, static archive target, client/server starters, build and clean targets |
| 2.2 - Implement partial sends and line receiver | send_all, send_line, per-connection recv_line; partial I/O and EINTR handling |
| 2.3 - Implement port and protocol validation | parse_port, parse_message, normalize_word; exact protocol and aliases |
| 2.4 - Test library framing and error paths | 21 executable cases, 278 checks, captured results and memory-check record |

Use the submitted final design: C, Linux, `fork()` on the server, and default
port **53280**. Earlier proposed diagrams using threads or port 5000 are not
the basis of this implementation.

## Shared API

Both files include `wordle_common.h`. Do not copy the common code into either
program. The Makefile links each program to `libwordle.a`.

All functions below return **0 for success and -1 for failure**, with errno,
except recv_line, which returns a LineResult enum.

```c
int parse_port(const char *text, uint16_t *out_port);
int normalize_word(const char *text, char out_word[WORD_LENGTH + 1]);
int send_all(int fd, const void *buffer, size_t length);
int send_line(int fd, const char *text);
int parse_message(const char *line, WordleMessage *out_message);

void line_reader_init(LineReader *reader, int fd);
LineResult recv_line(LineReader *reader, char *out, size_t capacity);
```

### Port and word validation

```c
uint16_t port = DEFAULT_PORT;
if (argc == 3 && parse_port(argv[2], &port) == -1) {
    /* Client: print usage/error and stop. */
}
```

The client must first require argc == 2 or 3; the server must require argc == 1
or 2 and parse argv[1] only when present. parse_port accepts digits only and
1024-65535, rejects overflow, and leaves the output unchanged on failure.
Leading zeroes are accepted. `port` is in host byte order: use `htons(port)`
when filling a socket address.

```c
char word[WORD_LENGTH + 1];
if (normalize_word(user_input, word) == -1) {
    /* Invalid: re-prompt without spending a guess. */
}
```

normalize_word trims surrounding ASCII whitespace, uppercases ASCII letters,
and requires exactly five letters. It does not check a dictionary. It leaves
output unchanged on failure and supports using the same input/output buffer.
Read a whole terminal line first; drain overlong terminal input before asking
again. Reject embedded NUL input instead of treating the prefix as a guess.

### Sending a message

```c
if (send_line(connected_fd, "READY") == -1) {
    /* Connection failed: report and close. */
}
```

Pass printable text without `\n`. It adds exactly one newline. At most 127
text bytes are allowed. Do not include a terminating NUL in send_all's length.
send_all retries interrupted/partial sends; any other failure ends the session.
It is not a nonblocking I/O event loop. send_line validates framing, while
parse_message validates protocol syntax.

### Receiving and parsing a message

Initialize the reader **once** for each connected socket and reuse it:

```c
LineReader reader;
char line[MAX_FRAME];
WordleMessage message;
LineResult result;

line_reader_init(&reader, connected_fd);

/* Repeat this part whenever this connection needs another message. */
result = recv_line(&reader, line, sizeof(line));
if (result != RECV_LINE_OK) {
    /* EOF or error: leave the session loop and close this connection. */
} else if (parse_message(line, &message) == -1) {
    /* Complete line, but invalid command: client errors or server sends ERROR. */
} else {
    /* Check the expected message type and current session state. */
}
```

Do not reinitialize the reader after every message: it may already contain
the next message. Do not mix recv_line with direct recv/read calls on that
socket; buffered bytes would be skipped. The functions do not close sockets.

| recv_line result | Meaning and caller action |
| --- | --- |
| RECV_LINE_OK (1) | One complete line; inspect/parse it |
| RECV_LINE_EOF (0) | Peer disconnected between frames; close session |
| RECV_LINE_TRUNCATED | Peer closed before LF; close session |
| RECV_LINE_BAD_FRAME | Control/non-ASCII byte before LF; close session |
| RECV_LINE_TOO_LONG | More than 128 wire bytes including LF; close session |
| RECV_LINE_NO_SPACE | Output array cannot hold text and NUL; fix caller and close session |
| RECV_LINE_TIMEOUT | Configured blocking-socket receive timeout expired; close session |
| RECV_LINE_IO_ERROR | Invalid call/socket or other receive error; close session |

A terminal EOF/error leaves the reader unusable for further messages. The
output is empty on a runtime receive error. A NULL output pointer or zero
capacity is an invalid call, so no output can be provided. Use initialized
readers with blocking sockets. The library sets no socket timeouts itself.

### Parsed message types

| Wire text without LF | message.type | Payload |
| --- | --- | --- |
| HELLO | MSG_HELLO | None |
| READY | MSG_READY | None |
| WORD | MSG_WORD_REQUEST | None |
| WORD APPLE | MSG_WORD | message.word = "APPLE" |
| BYE | MSG_BYE | None |
| QUIT | MSG_QUIT | None |
| ERROR BAD_COMMAND | MSG_ERROR | message.reason = "BAD_COMMAND" |

parse_message returns MSG_INVALID on invalid syntax. It does **not** decide
whether a valid message is allowed at that point. For example HELLO is valid
syntax, but a server must not accept it as a client request. Incoming wire
WORD payloads must already be uppercase; use normalize_word for player/bank
input, not for correcting invalid protocol replies.

## Shiva - tasks 2.5-2.8

Replace `client.c`'s starter main. Implement hostname/optional-port arguments,
TCP connection, instructions, a numbered board, remaining guesses, and the
unguessed alphabet. After HELLO send READY, then require MSG_WORD. Keep its
answer in memory; do not reveal it during play.

Validate guesses with normalize_word. Score exact matches first, then count
remaining copies for wrong-position matches, so repeated letters are not
overcounted. Accept at most six valid guesses. Re-prompt invalid guesses
without using a turn. On win/loss, show the result and ask y/n; invalid replay
answers re-prompt. Replay uses another READY on the same socket and resets
round state. Quitting/input EOF sends BYE when possible and closes the socket.
Unexpected replies, ERROR, truncated frames or disconnection end the session.

Build with `make client`. The run interface is `./client hostname [port]`.

## Giorgi - tasks 2.9-2.12

Replace `server.c`'s starter main. Implement optional-port arguments, words.txt
loading and a listening TCP socket. Normalize bank entries and keep only valid
five-letter words. A missing file or zero valid words must fail startup.

Use fork per connection, as the final design specifies. The parent closes its
copy of the accepted socket and resumes accepting clients. The child closes
its inherited listener, initializes its own reader, sends HELLO, and handles
MSG_READY or MSG_WORD_REQUEST by sending a random `WORD <word>` response.
Build that response with snprintf, checking the return value. MSG_BYE or
MSG_QUIT closes the connected socket and ends the child. Reap children in the
parent to prevent zombies. Handle fork/accept failure and Ctrl+C cleanup.

After fork, seed child word selection so every child does not inherit exactly
the same random sequence. The bank can be inherited read-only. An invalid
message should send a bounded printable ERROR reason; the team must choose
and document whether that complete-command error retries or closes. A framing
or I/O error always closes that child session. The parent and other sessions
must keep running. No guesses or scoring belong in the server.

Build with `make server`. The run interface is `./server [port]`.

## Before the final submission

The team still needs integration and full-system tests on acad, the finished
word bank, final README review, and actual Gantt times. Use all nine scenarios
in the submitted final design, including simultaneous clients and shutdown.
Then package the completed directory as `group17_wordle.zip`, submit through
turnin328 for project wordle, and submit the Gantt separately to D2L.
