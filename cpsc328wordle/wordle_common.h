#ifndef WORDLE_COMMON_H
#define WORDLE_COMMON_H

#include <stddef.h>
#include <stdint.h>

/* Both programs must use the same values. */
#define DEFAULT_PORT 53280
#define WORD_LENGTH 5
#define MAX_ATTEMPTS 6
#define MAX_FRAME 128          /* Maximum wire bytes, including LF. */
#define MAX_TEXT (MAX_FRAME - 1)

typedef enum {
    MSG_INVALID = 0,
    MSG_HELLO,
    MSG_READY,
    MSG_WORD_REQUEST,         /* The bare command WORD. */
    MSG_WORD,                 /* A reply such as WORD APPLE. */
    MSG_BYE,
    MSG_QUIT,
    MSG_ERROR
} MessageType;

typedef struct {
    MessageType type;
    char word[WORD_LENGTH + 1];
    char reason[MAX_TEXT + 1];
} WordleMessage;

typedef enum {
    RECV_LINE_OK = 1,
    RECV_LINE_EOF = 0,
    RECV_LINE_IO_ERROR = -1,
    RECV_LINE_TRUNCATED = -2,
    RECV_LINE_BAD_FRAME = -3,
    RECV_LINE_TOO_LONG = -4,
    RECV_LINE_NO_SPACE = -5,
    RECV_LINE_TIMEOUT = -6
} LineResult;

/* Create a separate reader for EACH connected socket, not the listener. */
typedef struct {
    int fd;
    unsigned char pending[MAX_FRAME * 2];
    size_t begin;
    size_t end;
    int terminal;
    LineResult last_result;
} LineReader;

/* Return 0 on success or -1 on failure (with errno), except recv_line. */
int parse_port(const char *text, uint16_t *out_port);
int normalize_word(const char *text, char out_word[WORD_LENGTH + 1]);
int send_all(int fd, const void *buffer, size_t length);
int send_line(int fd, const char *text);
int parse_message(const char *line, WordleMessage *out_message);

void line_reader_init(LineReader *reader, int fd);
/* Returns a LineResult. Output has no LF and is NUL-terminated on success.
 * Use char line[MAX_FRAME]; 127 text bytes + local NUL fit in 128 bytes.
 * EOF/error ends this reader: close the session; do not try to resynchronize.
 * The library never closes a descriptor owned by its caller.
 */
LineResult recv_line(LineReader *reader, char *out, size_t capacity);

#endif
