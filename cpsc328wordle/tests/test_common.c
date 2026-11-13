#include "wordle_common.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <unistd.h>

/* GNU ld test wrappers make short I/O and EINTR repeatable.
 * They are linked into this test executable only, never libwordle.a.
 */
ssize_t __real_send(int fd, const void *data, size_t length, int flags);
ssize_t __real_recv(int fd, void *data, size_t length, int flags);

static size_t send_limit;
static size_t recv_limit;
static int interrupt_send;
static int interrupt_recv;
static unsigned int send_calls;
static unsigned int recv_calls;
static unsigned int checks;
static unsigned int cases;

ssize_t __wrap_send(int fd, const void *data, size_t length, int flags)
{
    ++send_calls;
    if (interrupt_send) {
        interrupt_send = 0;
        errno = EINTR;
        return -1;
    }
    if (send_limit != 0 && length > send_limit) {
        length = send_limit;
    }
    return __real_send(fd, data, length, flags);
}

ssize_t __wrap_recv(int fd, void *data, size_t length, int flags)
{
    ++recv_calls;
    if (interrupt_recv) {
        interrupt_recv = 0;
        errno = EINTR;
        return -1;
    }
    if (recv_limit != 0 && length > recv_limit) {
        length = recv_limit;
    }
    return __real_recv(fd, data, length, flags);
}

#define CHECK(expression) do { \
    ++checks; \
    if (!(expression)) { \
        fprintf(stderr, "FAIL line %d: %s (errno=%d)\n", \
                __LINE__, #expression, errno); \
        exit(EXIT_FAILURE); \
    } \
} while (0)

static void passed(const char *name)
{
    ++cases;
    printf("PASS %02u: %s\n", cases, name);
}

static void pair(int fd[2])
{
    CHECK(socketpair(AF_UNIX, SOCK_STREAM, 0, fd) == 0);
}

static void close_pair(int fd[2])
{
    CHECK(close(fd[0]) == 0);
    CHECK(close(fd[1]) == 0);
}

static void test_ports(void)
{
    const char *invalid[] = {
        "", "1023", "0", "65536", "70000", "-1", "+53280", " 53280",
        "53280 ", "53280abc", "53.280", "53280\n",
        "99999999999999999999999999999999999999999999"
    };
    uint16_t port = 12;
    size_t i;
    CHECK(parse_port("1024", &port) == 0 && port == 1024);
    CHECK(parse_port("65535", &port) == 0 && port == 65535);
    CHECK(parse_port("53280", &port) == 0 && port == DEFAULT_PORT);
    CHECK(parse_port("0053280", &port) == 0 && port == DEFAULT_PORT);
    for (i = 0; i < sizeof(invalid) / sizeof(invalid[0]); ++i) {
        port = 12;
        CHECK(parse_port(invalid[i], &port) == -1);
        CHECK(port == 12);
    }
    CHECK(parse_port(NULL, &port) == -1);
    CHECK(parse_port("53280", NULL) == -1);
    passed("ports: boundaries, default, invalid input and overflow");
}

static void test_words(void)
{
    const char *invalid[] = {"", "CAT", "APPLES", "AB12!", "AP LE", "12345",
                             "APPL\xC9", "     "};
    char word[WORD_LENGTH + 1];
    char same[32] = "  apple\n";
    size_t i;
    CHECK(normalize_word(" \tapple\r\n", word) == 0);
    CHECK(strcmp(word, "APPLE") == 0);
    CHECK(normalize_word("PlAnT", word) == 0 && strcmp(word, "PLANT") == 0);
    CHECK(normalize_word(same, same) == 0 && strcmp(same, "APPLE") == 0);
    for (i = 0; i < sizeof(invalid) / sizeof(invalid[0]); ++i) {
        strcpy(word, "KEEP!");
        CHECK(normalize_word(invalid[i], word) == -1);
        CHECK(strcmp(word, "KEEP!") == 0);
    }
    CHECK(normalize_word(NULL, word) == -1);
    CHECK(normalize_word("APPLE", NULL) == -1);
    passed("words: trimming, uppercase, five ASCII letters and invalid guesses");
}

static void test_messages(void)
{
    const char *valid[] = {"HELLO", "READY", "WORD", "BYE", "QUIT",
                           "WORD APPLE", "ERROR BAD_COMMAND", "ERROR invalid request"};
    const MessageType types[] = {MSG_HELLO, MSG_READY, MSG_WORD_REQUEST, MSG_BYE,
                                 MSG_QUIT, MSG_WORD, MSG_ERROR, MSG_ERROR};
    const char *invalid[] = {"", "hello", "READY NOW", " HELLO", "HELLO ",
        "WORD apple", "WORD APPLES", "WORD CAT", "WORD  APPLE", "WORD APPLE X",
        "BYE NOW", "QUIT NOW", "ERROR", "ERROR ", "ERROR  BAD_COMMAND",
        "ERROR reason ", "HELLO\n", "WORD\tAPPLE", "UNKNOWN", "ER\xFF"};
    WordleMessage message;
    size_t i;
    char limit[MAX_TEXT + 2];
    for (i = 0; i < sizeof(valid) / sizeof(valid[0]); ++i) {
        CHECK(parse_message(valid[i], &message) == 0 && message.type == types[i]);
    }
    CHECK(parse_message("WORD APPLE", &message) == 0);
    CHECK(strcmp(message.word, "APPLE") == 0 && message.reason[0] == '\0');
    CHECK(parse_message("ERROR invalid request", &message) == 0);
    CHECK(strcmp(message.reason, "invalid request") == 0);
    for (i = 0; i < sizeof(invalid) / sizeof(invalid[0]); ++i) {
        CHECK(parse_message(invalid[i], &message) == -1);
        CHECK(message.type == MSG_INVALID);
    }
    memcpy(limit, "ERROR ", 6);
    memset(limit + 6, 'X', MAX_TEXT - 6);
    limit[MAX_TEXT] = '\0';
    CHECK(parse_message(limit, &message) == 0);
    limit[MAX_TEXT] = 'X';
    limit[MAX_TEXT + 1] = '\0';
    CHECK(parse_message(limit, &message) == -1);
    CHECK(parse_message(NULL, &message) == -1);
    CHECK(parse_message("HELLO", NULL) == -1);
    passed("protocol: exact commands, both aliases, WORD payload and ERROR reason");
}

static void test_sending(void)
{
    int fd[2];
    char wire[64];
    char too_long[MAX_TEXT + 2];
    ssize_t n;
    pair(fd);
    send_limit = 2;
    interrupt_send = 1;
    send_calls = 0;
    CHECK(send_line(fd[0], "WORD APPLE") == 0);
    CHECK(send_calls >= 7);
    CHECK(shutdown(fd[0], SHUT_WR) == 0);
    n = __real_recv(fd[1], wire, sizeof(wire), MSG_WAITALL);
    CHECK(n == 11 && memcmp(wire, "WORD APPLE\n", 11) == 0);
    send_limit = 0;
    close_pair(fd);
    passed("short sends and EINTR: all bytes and exactly one newline arrive");

    pair(fd);
    memset(too_long, 'X', sizeof(too_long) - 1);
    too_long[sizeof(too_long) - 1] = '\0';
    CHECK(send_line(fd[0], too_long) == -1 && errno == EMSGSIZE);
    CHECK(send_line(fd[0], "HELLO\n") == -1);
    CHECK(send_line(fd[0], "HELLO\r") == -1);
    CHECK(send_line(fd[0], "HE\tLLO") == -1);
    CHECK(send_line(fd[0], "\xFF") == -1);
    CHECK(send_line(fd[0], NULL) == -1);
    CHECK(__real_recv(fd[1], wire, sizeof(wire), MSG_DONTWAIT) == -1);
    CHECK(errno == EAGAIN || errno == EWOULDBLOCK);
    CHECK(send_all(fd[0], NULL, 0) == 0);
    CHECK(send_all(fd[0], NULL, 2) == -1);
    close_pair(fd);
    CHECK(send_all(-1, "X", 1) == -1 && errno == EBADF);
    passed("send validation: invalid frames send no bytes; bad arguments fail");

    pair(fd);
    CHECK(close(fd[1]) == 0);
    CHECK(send_line(fd[0], "HELLO") == -1);
    CHECK(errno == EPIPE || errno == ECONNRESET);
    CHECK(close(fd[0]) == 0);
    passed("broken peer: failure is returned without terminating on SIGPIPE");
}

static void test_receiving(void)
{
    int fd[2];
    LineReader reader;
    char line[MAX_FRAME];
    unsigned int calls_before;
    pair(fd);
    line_reader_init(&reader, fd[1]);
    CHECK(send_all(fd[0], "WORD APPLE\n", 11) == 0);
    recv_limit = 2;
    interrupt_recv = 1;
    recv_calls = 0;
    CHECK(recv_line(&reader, line, sizeof(line)) == RECV_LINE_OK);
    CHECK(strcmp(line, "WORD APPLE") == 0 && recv_calls >= 7);
    recv_limit = 0;
    close_pair(fd);
    passed("fragmented receives and EINTR: wait for the complete line");

    pair(fd);
    line_reader_init(&reader, fd[1]);
    CHECK(send_all(fd[0], "HELLO\nREADY\nBYE\n", 16) == 0);
    CHECK(recv_line(&reader, line, sizeof(line)) == RECV_LINE_OK);
    CHECK(strcmp(line, "HELLO") == 0);
    /* The remaining two buffered messages must not need another recv call. */
    calls_before = recv_calls;
    CHECK(recv_line(&reader, line, sizeof(line)) == RECV_LINE_OK);
    CHECK(strcmp(line, "READY") == 0);
    CHECK(recv_line(&reader, line, sizeof(line)) == RECV_LINE_OK);
    CHECK(strcmp(line, "BYE") == 0 && recv_calls == calls_before);
    close_pair(fd);
    passed("coalesced receives: preserve the next two messages in the reader");

    pair(fd);
    line_reader_init(&reader, fd[1]);
    CHECK(send_all(fd[0], "HELLO\nWORD AP", 13) == 0);
    CHECK(recv_line(&reader, line, sizeof(line)) == RECV_LINE_OK);
    CHECK(strcmp(line, "HELLO") == 0);
    CHECK(send_all(fd[0], "PLE\n", 4) == 0);
    CHECK(recv_line(&reader, line, sizeof(line)) == RECV_LINE_OK);
    CHECK(strcmp(line, "WORD APPLE") == 0);
    close_pair(fd);
    passed("mixed receives: retain a partial next frame until its newline");
}

static void test_frame_limits(void)
{
    int fd[2];
    char text[MAX_TEXT + 2];
    char line[MAX_FRAME];
    LineReader reader;
    pair(fd);
    line_reader_init(&reader, fd[1]);
    memset(text, 'X', MAX_TEXT);
    text[MAX_TEXT] = '\0';
    CHECK(send_line(fd[0], text) == 0);
    CHECK(recv_line(&reader, line, sizeof(line)) == RECV_LINE_OK);
    CHECK(strlen(line) == MAX_TEXT && strcmp(line, text) == 0);
    close_pair(fd);
    passed("frame boundary: 127 text bytes plus LF fit with a local NUL");

    pair(fd);
    line_reader_init(&reader, fd[1]);
    memset(text, 'X', MAX_TEXT + 1);
    text[MAX_TEXT + 1] = '\n';
    CHECK(send_all(fd[0], text, sizeof(text)) == 0);
    CHECK(recv_line(&reader, line, sizeof(line)) == RECV_LINE_TOO_LONG);
    CHECK(line[0] == '\0');
    CHECK(recv_line(&reader, line, sizeof(line)) == RECV_LINE_TOO_LONG);
    close_pair(fd);
    passed("overlong frame: reject and keep reader terminal");

    pair(fd);
    line_reader_init(&reader, fd[1]);
    CHECK(send_line(fd[0], "HELLO") == 0);
    CHECK(recv_line(&reader, line, 4) == RECV_LINE_NO_SPACE);
    CHECK(line[0] == '\0');
    close_pair(fd);
    passed("small output buffer: fail safely rather than truncate a command");
}

static void test_receive_errors(void)
{
    const unsigned char invalid[] = {'\0', '\r', '\t', 0xFF};
    size_t i;
    char line[MAX_FRAME];
    LineReader reader;
    int fd[2];
    struct timeval timeout = {0, 100000};

    for (i = 0; i < sizeof(invalid); ++i) {
        unsigned char frame[] = {'H', 'I', invalid[i], '\n'};
        pair(fd);
        line_reader_init(&reader, fd[1]);
        CHECK(send_all(fd[0], frame, sizeof(frame)) == 0);
        CHECK(recv_line(&reader, line, sizeof(line)) == RECV_LINE_BAD_FRAME);
        CHECK(line[0] == '\0');
        close_pair(fd);
    }
    passed("framing errors: embedded NUL, CR, tab and non-ASCII byte");

    pair(fd);
    line_reader_init(&reader, fd[1]);
    CHECK(shutdown(fd[0], SHUT_WR) == 0);
    CHECK(recv_line(&reader, line, sizeof(line)) == RECV_LINE_EOF);
    CHECK(recv_line(&reader, line, sizeof(line)) == RECV_LINE_EOF);
    close_pair(fd);
    passed("clean EOF: a disconnect between messages is detected");

    pair(fd);
    line_reader_init(&reader, fd[1]);
    CHECK(send_all(fd[0], "HELLO", 5) == 0);
    CHECK(shutdown(fd[0], SHUT_WR) == 0);
    CHECK(recv_line(&reader, line, sizeof(line)) == RECV_LINE_TRUNCATED);
    CHECK(line[0] == '\0');
    close_pair(fd);
    passed("truncated frame: EOF before LF is not a complete HELLO");

    pair(fd);
    line_reader_init(&reader, fd[1]);
    CHECK(setsockopt(fd[1], SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) == 0);
    CHECK(send_all(fd[0], "HE", 2) == 0);
    CHECK(recv_line(&reader, line, sizeof(line)) == RECV_LINE_TIMEOUT);
    CHECK(line[0] == '\0');
    close_pair(fd);
    passed("configured socket timeout: incomplete data is not accepted");

    line_reader_init(&reader, -1);
    CHECK(recv_line(&reader, line, sizeof(line)) == RECV_LINE_IO_ERROR);
    CHECK(recv_line(NULL, line, sizeof(line)) == RECV_LINE_IO_ERROR);
    CHECK(recv_line(&reader, NULL, sizeof(line)) == RECV_LINE_IO_ERROR);
    CHECK(recv_line(&reader, line, 0) == RECV_LINE_IO_ERROR);
    passed("receiver errors: invalid socket and invalid arguments");

    pair(fd);
    line_reader_init(&reader, fd[1]);
    CHECK(send_line(fd[0], "") == 0);
    CHECK(recv_line(&reader, line, sizeof(line)) == RECV_LINE_OK && line[0] == '\0');
    {
        WordleMessage message;
        CHECK(parse_message(line, &message) == -1);
    }
    close_pair(fd);
    passed("empty line: complete framing, but not a valid protocol command");
}

static void test_connection_isolation(void)
{
    int a[2];
    int b[2];
    LineReader one;
    LineReader two;
    char line[MAX_FRAME];
    pair(a);
    pair(b);
    line_reader_init(&one, a[1]);
    line_reader_init(&two, b[1]);
    CHECK(send_all(a[0], "HELLO\nREADY\n", 12) == 0);
    CHECK(send_line(b[0], "BYE") == 0);
    CHECK(recv_line(&one, line, sizeof(line)) == RECV_LINE_OK);
    CHECK(strcmp(line, "HELLO") == 0);
    CHECK(recv_line(&two, line, sizeof(line)) == RECV_LINE_OK);
    CHECK(strcmp(line, "BYE") == 0);
    CHECK(recv_line(&one, line, sizeof(line)) == RECV_LINE_OK);
    CHECK(strcmp(line, "READY") == 0);
    close_pair(a);
    close_pair(b);
    passed("per-connection buffers: two independent readers do not mix messages");
}

static void test_large_send(void)
{
    int fd[2];
    int status;
    pid_t child;
    size_t length = 512U * 1024U;
    unsigned char *data = malloc(length);
    size_t i;
    CHECK(data != NULL);
    for (i = 0; i < length; ++i) {
        data[i] = (unsigned char)(i % 251U);
    }
    pair(fd);
    child = fork();
    CHECK(child >= 0);
    if (child == 0) {
        unsigned char chunk[4096];
        size_t total = 0;
        ssize_t n;
        close(fd[0]);
        while ((n = __real_recv(fd[1], chunk, sizeof(chunk), 0)) > 0) {
            for (i = 0; i < (size_t)n; ++i) {
                if (chunk[i] != (unsigned char)((total + i) % 251U)) {
                    _exit(2);
                }
            }
            total += (size_t)n;
        }
        close(fd[1]);
        _exit(n == 0 && total == length ? 0 : 3);
    }
    CHECK(close(fd[1]) == 0);
    send_limit = 4096;
    CHECK(send_all(fd[0], data, length) == 0);
    send_limit = 0;
    CHECK(close(fd[0]) == 0);
    CHECK(waitpid(child, &status, 0) == child);
    CHECK(WIFEXITED(status) && WEXITSTATUS(status) == 0);
    free(data);
    passed("large send: a separate receiver verifies every byte of 512 KiB");
}

static void test_exchange(void)
{
    int fd[2];
    LineReader client_reader;
    LineReader server_reader;
    WordleMessage message;
    char line[MAX_FRAME];
    pair(fd);
    line_reader_init(&client_reader, fd[0]);
    line_reader_init(&server_reader, fd[1]);
    CHECK(send_line(fd[1], "HELLO") == 0);
    CHECK(recv_line(&client_reader, line, sizeof(line)) == RECV_LINE_OK);
    CHECK(parse_message(line, &message) == 0 && message.type == MSG_HELLO);
    CHECK(send_line(fd[0], "READY") == 0);
    CHECK(recv_line(&server_reader, line, sizeof(line)) == RECV_LINE_OK);
    CHECK(parse_message(line, &message) == 0 && message.type == MSG_READY);
    CHECK(send_line(fd[1], "WORD APPLE") == 0);
    CHECK(recv_line(&client_reader, line, sizeof(line)) == RECV_LINE_OK);
    CHECK(parse_message(line, &message) == 0 && message.type == MSG_WORD);
    CHECK(strcmp(message.word, "APPLE") == 0);
    CHECK(send_line(fd[0], "WORD") == 0);
    CHECK(recv_line(&server_reader, line, sizeof(line)) == RECV_LINE_OK);
    CHECK(parse_message(line, &message) == 0 && message.type == MSG_WORD_REQUEST);
    CHECK(send_line(fd[1], "WORD PLANT") == 0);
    CHECK(recv_line(&client_reader, line, sizeof(line)) == RECV_LINE_OK);
    CHECK(parse_message(line, &message) == 0 && strcmp(message.word, "PLANT") == 0);
    CHECK(send_line(fd[0], "QUIT") == 0);
    CHECK(recv_line(&server_reader, line, sizeof(line)) == RECV_LINE_OK);
    CHECK(parse_message(line, &message) == 0 && message.type == MSG_QUIT);
    close_pair(fd);
    passed("two-endpoint library exchange: greeting, word, replay alias, quit alias");
}

int main(void)
{
    alarm(20);                 /* A blocked test must fail, not hang forever. */
    test_ports();
    test_words();
    test_messages();
    test_sending();
    test_receiving();
    test_frame_limits();
    test_receive_errors();
    test_connection_isolation();
    test_large_send();
    test_exchange();
    alarm(0);
    printf("\nAll %u test cases passed (%u checks).\n", cases, checks);
    return EXIT_SUCCESS;
}
