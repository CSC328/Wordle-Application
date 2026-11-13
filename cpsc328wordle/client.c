/*************************************************************************
 * Name:          Shivamani Jadi
 * Major:         Computer Science
 * Creation Date: November 12, 2026
 * Due Date:      November 23, 2026
 * Course:        CPSC 328 - Network and Secure Programming
 * Professor:     Reiley Walther
 * Assignment:    Group 17 Wordle Application, client tasks 2.5-2.8
 * Filename:      client.c
 * Purpose:       Connect to the word server and run the local Wordle game.
 * Compile:       make client
 * Usage:         ./client hostname [port]
 *
 * Citations
 * [1] R. Walther, modified from L. Frye. "CLAs.c." Date not listed.
 *     P-5.zip: examples/CLAs.c.
 *     Retrieved: November 12, 2026 from Prof. Reiley Walther's provided
 *     CPSC 328 class files. Used for argc/argv and usage-message structure.
 * [2] Dr. Lisa Frye. "client_allAddrs.c." Modified September 1, 2021.
 *     P-5.zip: examples/sockets/sum/client_allAddrs.c.
 *     Retrieved: November 12, 2026 from the same class files.
 *     Used for getaddrinfo, trying addresses, socket, connect and close.
 * [3] W. Richard Stevens, Bill Fenner and Andrew M. Rudoff. "utils.c,"
 *     readline(), from UNIX Network Programming, Volume 1, Third Edition.
 *     Documentation modified by Dr. Lisa Frye, October 2017.
 *     P-5.zip: examples/sockets/sum/utils.c.
 *     Retrieved: November 12, 2026 from the same class files.
 *     Used for the byte-at-a-time line-input and EOF pattern.
 * [4] "daytimed.c." Author and date not listed in the provided file.
 *     P-5.zip: examples/sockets/UDPtimed/daytimed.c.
 *     Retrieved: November 12, 2026 from the same class files.
 *     Used for FD_ZERO, FD_SET, select and FD_ISSET on two descriptors.
 * [5] Charanya Ananthula. Group 17 shared-library handoff, November 2026.
 *     wordle_common.h, wordle_common.c and HANDOFF.md, tasks 2.1-2.4.
 *     Used unchanged for validation and complete protocol messages.
 *
 * Game behavior follows the submitted design, sections 5, 6, 8 and 9.1.
 * Scoring, board, alphabet and replay are project-specific client code.
 * See docs/CLIENT_SOURCES.md for the source-to-code mapping.
 *************************************************************************/

#include "wordle_common.h"

#include <ctype.h>
#include <errno.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>

#define INPUT_SIZE 256

typedef enum {
    INPUT_ERROR = -1,
    INPUT_EOF = 0,
    INPUT_OK = 1,
    INPUT_INVALID = 2
} InputResult;

/* Connection pattern adapted from client_allAddrs.c [2], cited above. */
static int connect_to_server(const char *hostname, uint16_t port)
{
    struct addrinfo hints = {0};
    struct addrinfo *addresses;
    struct addrinfo *address;
    char service[6];
    int fd = -1;
    int last_error = ECONNREFUSED;
    int result;

    snprintf(service, sizeof(service), "%u", (unsigned int)port);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    hints.ai_flags = AI_NUMERICSERV;

    result = getaddrinfo(hostname, service, &hints, &addresses);
    if (result != 0) {
        fprintf(stderr, "Cannot resolve %s: %s\n", hostname,
                gai_strerror(result));
        return -1;
    }
    for (address = addresses; address != NULL; address = address->ai_next) {
        fd = socket(address->ai_family, address->ai_socktype,
                    address->ai_protocol);
        if (fd == -1) {
            last_error = errno;
            continue;
        }
        if (connect(fd, address->ai_addr, address->ai_addrlen) == 0) {
            break;
        }
        last_error = errno;
        close(fd);
        fd = -1;
    }
    freeaddrinfo(addresses);
    if (fd == -1) {
        errno = last_error;
        perror("Cannot connect to server");
    }
    return fd;
}
/* End of adapted connection pattern. */

static int receive_message(LineReader *reader, WordleMessage *message)
{
    char line[MAX_FRAME];
    LineResult result = recv_line(reader, line, sizeof(line));

    if (result != RECV_LINE_OK) {
        switch (result) {
        case RECV_LINE_EOF:
            fprintf(stderr, "Server disconnected.\n");
            break;
        case RECV_LINE_TRUNCATED:
            fprintf(stderr, "Server disconnected during a message.\n");
            break;
        case RECV_LINE_BAD_FRAME:
            fprintf(stderr, "Server sent an invalid message frame.\n");
            break;
        case RECV_LINE_TOO_LONG:
            fprintf(stderr, "Server message is too long.\n");
            break;
        case RECV_LINE_TIMEOUT:
            fprintf(stderr, "Receiving from the server timed out.\n");
            break;
        case RECV_LINE_NO_SPACE:
            fprintf(stderr, "Server message does not fit the receive buffer.\n");
            break;
        default:
            perror("Cannot receive from server");
            break;
        }
        return -1;
    }
    if (parse_message(line, message) == -1) {
        fprintf(stderr, "Server sent an invalid protocol message.\n");
        return -1;
    }
    if (message->type == MSG_ERROR) {
        fprintf(stderr, "Server error: %s\n", message->reason);
        return -1;
    }
    return 0;
}

/* Line-input pattern adapted from readline() [3], cited above.
 * Socket monitoring uses the two-descriptor select pattern from [4].
 */
static InputResult read_input(LineReader *reader, char *out, size_t capacity)
{
    fd_set read_fds;
    size_t length = 0;
    int invalid = 0;
    int received = 0;

    if (reader->fd <= STDIN_FILENO || reader->fd >= FD_SETSIZE) {
        fprintf(stderr, "Cannot monitor the input and server descriptors.\n");
        return INPUT_ERROR;
    }
    for (;;) {
        unsigned char ch;
        ssize_t count;

        /* recv_line may already have buffered another server message. */
        if (reader->begin < reader->end) {
            WordleMessage message;
            if (receive_message(reader, &message) == 0) {
                fprintf(stderr, "Unexpected server message during input.\n");
            }
            return INPUT_ERROR;
        }
        /* Descriptor-waiting pattern adapted from daytimed.c [4]. */
        FD_ZERO(&read_fds);
        FD_SET(STDIN_FILENO, &read_fds);
        FD_SET(reader->fd, &read_fds);
        if (select(reader->fd + 1, &read_fds, NULL, NULL, NULL) == -1) {
            if (errno == EINTR) {
                continue;
            }
            perror("Cannot wait for input");
            return INPUT_ERROR;
        }
        if (FD_ISSET(reader->fd, &read_fds)) {
            WordleMessage message;
            if (receive_message(reader, &message) == 0) {
                fprintf(stderr, "Unexpected server message during input.\n");
            }
            return INPUT_ERROR;
        }
        if (!FD_ISSET(STDIN_FILENO, &read_fds)) {
            continue;
        }
        /* End of adapted descriptor-waiting pattern. */
        count = read(STDIN_FILENO, &ch, 1);
        if (count == -1) {
            if (errno == EINTR) {
                continue;
            }
            perror("Cannot read input");
            return INPUT_ERROR;
        }
        if (count == 0) {
            if (!received) {
                return INPUT_EOF;
            }
            break;
        }
        received = 1;
        if (ch == '\n') {
            break;
        }
        if (ch == '\0' || length >= capacity - 1) {
            invalid = 1;
        } else if (!invalid) {
            out[length++] = (char)ch;
        }
    }
    out[length] = '\0';
    return invalid ? INPUT_INVALID : INPUT_OK;
}
/* End of adapted line-input pattern. */

static void score_guess(const char *answer, const char *guess, char *feedback)
{
    int remaining[26] = {0};
    int i;

    /* Exact matches use their copies before any yellow matches. */
    for (i = 0; i < WORD_LENGTH; ++i) {
        if (guess[i] == answer[i]) {
            feedback[i] = 'G';
        } else {
            feedback[i] = '-';
            ++remaining[answer[i] - 'A'];
        }
    }
    for (i = 0; i < WORD_LENGTH; ++i) {
        int letter = guess[i] - 'A';
        if (feedback[i] != 'G' && remaining[letter] > 0) {
            feedback[i] = 'Y';
            --remaining[letter];
        }
    }
    feedback[WORD_LENGTH] = '\0';
}

static int quit_requested(const char *input)
{
    const unsigned char *p = (const unsigned char *)input;

    while (isspace(*p)) {
        ++p;
    }
    if (strncmp((const char *)p, ":quit", 5) != 0) {
        return 0;
    }
    p += 5;
    while (isspace(*p)) {
        ++p;
    }
    return *p == '\0';
}

static void show_board(char guesses[MAX_ATTEMPTS][WORD_LENGTH + 1],
                       char feedback[MAX_ATTEMPTS][WORD_LENGTH + 1],
                       const int guessed[26], int attempts)
{
    int row;
    int column;
    int letters_left = 0;

    printf("\n   Guess       | Feedback\n");
    for (row = 0; row < MAX_ATTEMPTS; ++row) {
        printf("%d. ", row + 1);
        for (column = 0; column < WORD_LENGTH; ++column) {
            printf("%c ", row < attempts ? guesses[row][column] : '_');
        }
        printf("| ");
        for (column = 0; column < WORD_LENGTH; ++column) {
            printf("%c ", row < attempts ? feedback[row][column] : '_');
        }
        putchar('\n');
    }
    printf("Guesses remaining: %d\n", MAX_ATTEMPTS - attempts);
    printf("Unguessed letters:");
    for (column = 0; column < 26; ++column) {
        if (!guessed[column]) {
            printf(" %c", 'A' + column);
            letters_left = 1;
        }
    }
    if (!letters_left) {
        printf(" (none)");
    }
    putchar('\n');
}

/* Return 1 for a finished round, 0 for input EOF, or -1 for an error. */
static int play_round(LineReader *reader, const char *answer)
{
    char guesses[MAX_ATTEMPTS][WORD_LENGTH + 1] = {{0}};
    char feedback[MAX_ATTEMPTS][WORD_LENGTH + 1] = {{0}};
    int guessed[26] = {0};
    int attempts = 0;

    show_board(guesses, feedback, guessed, attempts);
    while (attempts < MAX_ATTEMPTS) {
        char input[INPUT_SIZE];
        InputResult result;
        int i;

        printf("Guess %d of %d: ", attempts + 1, MAX_ATTEMPTS);
        fflush(stdout);
        result = read_input(reader, input, sizeof(input));
        if (result == INPUT_EOF ||
            (result == INPUT_OK && quit_requested(input))) {
            return 0;
        }
        if (result == INPUT_ERROR) {
            return -1;
        }
        if (result == INPUT_INVALID ||
            normalize_word(input, guesses[attempts]) == -1) {
            printf("Enter exactly five letters. This did not use a guess.\n");
            continue;
        }
        score_guess(answer, guesses[attempts], feedback[attempts]);
        for (i = 0; i < WORD_LENGTH; ++i) {
            guessed[guesses[attempts][i] - 'A'] = 1;
        }
        ++attempts;
        show_board(guesses, feedback, guessed, attempts);
        if (strcmp(guesses[attempts - 1], answer) == 0) {
            printf("You won!\n");
            return 1;
        }
    }
    printf("You lost. The word was %s.\n", answer);
    return 1;
}

static int ask_replay(LineReader *reader)
{
    for (;;) {
        char input[INPUT_SIZE];
        const unsigned char *p;
        unsigned char choice;
        InputResult result;

        printf("Play again? (y/n): ");
        fflush(stdout);
        result = read_input(reader, input, sizeof(input));
        if (result == INPUT_EOF ||
            (result == INPUT_OK && quit_requested(input))) {
            return 0;
        }
        if (result == INPUT_ERROR) {
            return -1;
        }
        if (result == INPUT_OK) {
            p = (const unsigned char *)input;
            while (isspace(*p)) {
                ++p;
            }
            choice = *p;
            if (choice != '\0') {
                ++p;
                while (isspace(*p)) {
                    ++p;
                }
                if (*p == '\0') {
                    if (choice == 'y' || choice == 'Y') {
                        return 1;
                    }
                    if (choice == 'n' || choice == 'N') {
                        return 0;
                    }
                }
            }
        }
        printf("Enter y or n.\n");
    }
}

int main(int argc, char *argv[])
{
    uint16_t port = DEFAULT_PORT;
    LineReader reader;
    WordleMessage message;
    int fd;
    int status = EXIT_FAILURE;

    /* CLA structure adapted from CLAs.c [1]; validation uses [5]. */
    if ((argc != 2 && argc != 3) || argv[1][0] == '\0') {
        fprintf(stderr, "Usage: %s hostname [port]\n", argv[0]);
        return EXIT_FAILURE;
    }
    /* End of adapted CLA structure. */
    if (argc == 3 && parse_port(argv[2], &port) == -1) {
        fprintf(stderr, "Port must contain digits only and be 1024-65535.\n");
        return EXIT_FAILURE;
    }
    fd = connect_to_server(argv[1], port);
    if (fd == -1) {
        return EXIT_FAILURE;
    }
    line_reader_init(&reader, fd);
    if (receive_message(&reader, &message) == -1) {
        goto done;
    }
    if (message.type != MSG_HELLO) {
        fprintf(stderr, "Unexpected server response: expected HELLO.\n");
        goto done;
    }
    printf("Connected to %s on port %u.\n", argv[1], (unsigned int)port);
    printf("Guess the five-letter word in at most six guesses.\n");
    printf("G = correct position, Y = wrong position, - = no matching copy.\n");
    printf("Lowercase guesses are accepted. Invalid guesses do not count.\n");
    printf("Enter :quit at a guess or replay prompt to leave.\n");

    for (;;) {
        int result;

        if (send_line(fd, "READY") == -1) {
            perror("Cannot request a word");
            goto done;
        }
        if (receive_message(&reader, &message) == -1) {
            goto done;
        }
        if (message.type != MSG_WORD) {
            fprintf(stderr, "Unexpected server response: expected WORD <word>.\n");
            goto done;
        }
        printf("\nStarting a new round.\n");
        result = play_round(&reader, message.word);
        if (result == -1) {
            goto done;
        }
        if (result == 0) {
            break;
        }
        result = ask_replay(&reader);
        if (result == -1) {
            goto done;
        }
        if (result == 0) {
            break;
        }
    }
    if (send_line(fd, "BYE") == -1) {
        perror("Cannot send BYE");
        goto done;
    }
    printf("\nGoodbye.\n");
    status = EXIT_SUCCESS;

done:
    /* Socket cleanup follows the client example [2]. */
    close(fd);
    return status;
}
