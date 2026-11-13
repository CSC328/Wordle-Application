#include "wordle_common.h"

#include <errno.h>
#include <string.h>
#include <sys/socket.h>

int parse_port(const char *text, uint16_t *out_port)
{
    unsigned int value = 0;
    const unsigned char *p = (const unsigned char *)text;

    if (text == NULL || out_port == NULL || *text == '\0') {
        errno = EINVAL;
        return -1;
    }
    for (; *p != '\0'; ++p) {
        unsigned int digit;
        if (*p < '0' || *p > '9') {
            errno = EINVAL;
            return -1;
        }
        digit = (unsigned int)(*p - '0');
        if (value > (65535U - digit) / 10U) {
            errno = ERANGE;
            return -1;
        }
        value = value * 10U + digit;
    }
    if (value < 1024U) {
        errno = ERANGE;
        return -1;
    }
    *out_port = (uint16_t)value;
    return 0;
}

static int ascii_space(unsigned char ch)
{
    return ch == ' ' || ch == '\t' || ch == '\n' || ch == '\r' ||
           ch == '\v' || ch == '\f';
}

int normalize_word(const char *text, char out_word[WORD_LENGTH + 1])
{
    size_t start = 0;
    size_t end;
    size_t i;
    char word[WORD_LENGTH + 1];

    if (text == NULL || out_word == NULL) {
        errno = EINVAL;
        return -1;
    }
    end = strlen(text);
    while (start < end && ascii_space((unsigned char)text[start])) {
        ++start;
    }
    while (end > start && ascii_space((unsigned char)text[end - 1])) {
        --end;
    }
    if (end - start != WORD_LENGTH) {
        errno = EINVAL;
        return -1;
    }
    for (i = 0; i < WORD_LENGTH; ++i) {
        unsigned char ch = (unsigned char)text[start + i];
        if (ch >= 'a' && ch <= 'z') {
            ch = (unsigned char)(ch - 'a' + 'A');
        }
        if (ch < 'A' || ch > 'Z') {
            errno = EINVAL;
            return -1;
        }
        word[i] = (char)ch;
    }
    word[WORD_LENGTH] = '\0';
    memcpy(out_word, word, sizeof(word));
    return 0;
}

int send_all(int fd, const void *buffer, size_t length)
{
    const unsigned char *bytes = buffer;
    size_t sent = 0;

    if (fd < 0) {
        errno = EBADF;
        return -1;
    }
    if (buffer == NULL && length != 0) {
        errno = EINVAL;
        return -1;
    }
    while (sent < length) {
        /* Linux: a disconnected peer becomes an error, not SIGPIPE death. */
        ssize_t n = send(fd, bytes + sent, length - sent, MSG_NOSIGNAL);
        if (n < 0) {
            if (errno == EINTR) {
                continue;
            }
            return -1;
        }
        if (n == 0) {
            errno = EPIPE;
            return -1;
        }
        sent += (size_t)n;
    }
    return 0;
}

int send_line(int fd, const char *text)
{
    char frame[MAX_FRAME];
    size_t length = 0;

    if (text == NULL) {
        errno = EINVAL;
        return -1;
    }
    while (text[length] != '\0') {
        unsigned char ch = (unsigned char)text[length];
        if (length >= MAX_TEXT) {
            errno = EMSGSIZE;
            return -1;
        }
        if (ch < 32 || ch > 126) {
            errno = EINVAL;
            return -1;
        }
        frame[length++] = (char)ch;
    }
    frame[length++] = '\n';
    return send_all(fd, frame, length);
}

void line_reader_init(LineReader *reader, int fd)
{
    if (reader != NULL) {
        memset(reader, 0, sizeof(*reader));
        reader->fd = fd;
        reader->last_result = RECV_LINE_OK;
    }
}

static LineResult finish_read(LineReader *reader, char *out, LineResult result)
{
    out[0] = '\0';
    reader->terminal = 1;
    reader->last_result = result;
    return result;
}

LineResult recv_line(LineReader *reader, char *out, size_t capacity)
{
    size_t length = 0;

    if (reader == NULL || out == NULL || capacity == 0) {
        errno = EINVAL;
        return RECV_LINE_IO_ERROR;
    }
    out[0] = '\0';
    if (reader->terminal) {
        return reader->last_result;
    }
    for (;;) {
        unsigned char ch;
        if (reader->begin == reader->end) {
            ssize_t n;
            do {
                n = recv(reader->fd, reader->pending,
                         sizeof(reader->pending), 0);
            } while (n < 0 && errno == EINTR);
            if (n == 0) {
                return finish_read(reader, out, length == 0 ? RECV_LINE_EOF :
                                                           RECV_LINE_TRUNCATED);
            }
            if (n < 0) {
                LineResult result = (errno == EAGAIN || errno == EWOULDBLOCK) ?
                                    RECV_LINE_TIMEOUT : RECV_LINE_IO_ERROR;
                return finish_read(reader, out, result);
            }
            reader->begin = 0;
            reader->end = (size_t)n;
        }
        ch = reader->pending[reader->begin++];
        if (ch == '\n') {
            out[length] = '\0';
            return RECV_LINE_OK;
        }
        if (ch < 32 || ch > 126) {
            return finish_read(reader, out, RECV_LINE_BAD_FRAME);
        }
        if (length >= MAX_TEXT) {
            return finish_read(reader, out, RECV_LINE_TOO_LONG);
        }
        if (length >= capacity - 1) {
            return finish_read(reader, out, RECV_LINE_NO_SPACE);
        }
        out[length++] = (char)ch;
    }
}

int parse_message(const char *line, WordleMessage *out_message)
{
    size_t length = 0;
    size_t i;
    WordleMessage message = {0};

    if (line == NULL || out_message == NULL) {
        errno = EINVAL;
        return -1;
    }
    while (line[length] != '\0') {
        unsigned char ch = (unsigned char)line[length];
        if (length >= MAX_TEXT || ch < 32 || ch > 126) {
            goto invalid;
        }
        ++length;
    }
    if (strcmp(line, "HELLO") == 0) {
        message.type = MSG_HELLO;
    } else if (strcmp(line, "READY") == 0) {
        message.type = MSG_READY;
    } else if (strcmp(line, "WORD") == 0) {
        message.type = MSG_WORD_REQUEST;
    } else if (strcmp(line, "BYE") == 0) {
        message.type = MSG_BYE;
    } else if (strcmp(line, "QUIT") == 0) {
        message.type = MSG_QUIT;
    } else if (length == 5 + WORD_LENGTH && strncmp(line, "WORD ", 5) == 0) {
        for (i = 5; i < length; ++i) {
            if (line[i] < 'A' || line[i] > 'Z') {
                goto invalid;
            }
        }
        message.type = MSG_WORD;
        memcpy(message.word, line + 5, WORD_LENGTH + 1);
    } else if (length > 6 && strncmp(line, "ERROR ", 6) == 0 &&
               line[6] != ' ' && line[length - 1] != ' ') {
        message.type = MSG_ERROR;
        memcpy(message.reason, line + 6, length - 6 + 1);
    } else {
        goto invalid;
    }
    *out_message = message;
    return 0;

invalid:
    memset(out_message, 0, sizeof(*out_message));
    errno = EPROTO;
    return -1;
}
