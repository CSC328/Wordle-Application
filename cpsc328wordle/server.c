/* Fork-per-client Wordle server, based on the supplied class example. */
#include "wordle_common.h"

#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#define MAX_WORDS 1000

static char words[MAX_WORDS][WORD_LENGTH + 1];
static int word_count = 0;

typedef struct Child {
    pid_t pid;
    struct Child *next;
} Child;

static volatile sig_atomic_t stopping = 0;

/* Handlers only set a flag; cleanup runs safely in the parent. */
static void catcher(int sig)
{
    if (sig == SIGINT || sig == SIGTERM)
        stopping = 1;
}

static void remove_child(Child **children, pid_t pid)
{
    Child **link = children;

    while (*link != NULL && (*link)->pid != pid)
        link = &(*link)->next;
    if (*link != NULL) {
        Child *finished = *link;
        *link = finished->next;
        free(finished);
    }
}

/* Do not pause accepting clients while other children are still playing. */
static void reap_children(Child **children)
{
    pid_t pid;

    for (;;) {
        pid = waitpid(-1, NULL, WNOHANG);
        if (pid > 0)
            remove_child(children, pid);
        else if (pid == -1 && errno == EINTR)
            continue;
        else
            break;
    }
}

static void stop_children(Child **children)
{
    Child *child;

    reap_children(children);
    for (child = *children; child != NULL; child = child->next)
        (void)kill(child->pid, SIGTERM);

    while (*children != NULL) {
        pid_t pid = (*children)->pid;
        while (waitpid(pid, NULL, 0) == -1 && errno == EINTR)
            ;
        remove_child(children, pid);
    }
}

static int load_words(void)
{
    FILE *file = fopen("words.txt", "r");
    char *line = NULL;
    size_t capacity = 0;
    ssize_t length;
    int read_failed;

    if (file == NULL) {
        perror("words.txt");
        return -1;
    }

    while ((length = getline(&line, &capacity, file)) != -1) {
        char word[WORD_LENGTH + 1];

        if (memchr(line, '\0', (size_t)length) != NULL ||
            normalize_word(line, word) == -1)
            continue;

        if (word_count == MAX_WORDS) {
            fprintf(stderr, "words.txt can contain at most %d valid words.\n",
                    MAX_WORDS);
            free(line);
            fclose(file);
            return -1;
        }

        strcpy(words[word_count], word);
        word_count++;
    }

    read_failed = !feof(file);
    free(line);

    if (fclose(file) == EOF || read_failed) {
        fprintf(stderr, "Could not finish reading words.txt.\n");
        return -1;
    }
    if (word_count == 0) {
        fprintf(stderr, "words.txt needs at least one five-letter word.\n");
        return -1;
    }
    return 0;
}

static void serve_client(int fd)
{
    LineReader reader;
    WordleMessage message;
    char line[MAX_FRAME];
    LineResult result;

    line_reader_init(&reader, fd);

    if (send_line(fd, "HELLO") == -1)
        return;

    printf("[client %ld] HELLO sent\n", (long)getpid());

    while ((result = recv_line(&reader, line, sizeof(line))) == RECV_LINE_OK) {
        if (parse_message(line, &message) == -1) {
            if (send_line(fd, "ERROR BAD_COMMAND") == -1)
                return;
            continue;
        }

        switch (message.type) {
        case MSG_READY:
        case MSG_WORD_REQUEST: {
            char response[MAX_FRAME];
            int index = rand() % word_count;
            int length = snprintf(response, sizeof(response),
                                  "WORD %s", words[index]);

            if (length < 0 || (size_t)length >= sizeof(response))
                return;
            if (send_line(fd, response) == -1)
                return;

            printf("[client %ld] Word requested and sent\n", (long)getpid());
            break;
        }

        case MSG_BYE:
        case MSG_QUIT:
            printf("[client %ld] Quit received\n", (long)getpid());
            return;

        default:
            if (send_line(fd, "ERROR BAD_COMMAND") == -1)
                return;
            break;
        }
    }

    /* A half-closed client may still be able to receive this final reply. */
    if (result == RECV_LINE_BAD_FRAME || result == RECV_LINE_TOO_LONG ||
        result == RECV_LINE_TRUNCATED)
        (void)send_line(fd, "ERROR BAD_FRAME");
}

int main(int argc, char **argv)
{
    int sockfd;
    int yes = 1;
    int flags;
    int status = EXIT_SUCCESS;
    uint16_t port = DEFAULT_PORT;
    struct sockaddr_in server = {0};
    struct sigaction action = {0};
    sigset_t blocked, previous, wait_mask;
    Child *children = NULL;

    if (argc > 2 ||
        (argc == 2 && parse_port(argv[1], &port) == -1)) {
        fprintf(stderr, "Usage: %s [port: 1024-65535]\n", argv[0]);
        return EXIT_FAILURE;
    }

    setvbuf(stdout, NULL, _IOLBF, 0);

    if (load_words() == -1)
        return EXIT_FAILURE;

    /* Block these except inside pselect, preventing signal/fork races. */
    sigemptyset(&blocked);
    sigaddset(&blocked, SIGINT);
    sigaddset(&blocked, SIGTERM);
    sigaddset(&blocked, SIGCHLD);
    if (sigprocmask(SIG_BLOCK, &blocked, &previous) == -1) {
        perror("sigprocmask");
        return EXIT_FAILURE;
    }
    wait_mask = previous;
    sigdelset(&wait_mask, SIGINT);
    sigdelset(&wait_mask, SIGTERM);
    sigdelset(&wait_mask, SIGCHLD);
    action.sa_handler = catcher;
    sigemptyset(&action.sa_mask);
    action.sa_flags = SA_NOCLDSTOP;
    if (sigaction(SIGINT, &action, NULL) == -1 ||
        sigaction(SIGTERM, &action, NULL) == -1 ||
        sigaction(SIGCHLD, &action, NULL) == -1) {
        perror("sigaction");
        return EXIT_FAILURE;
    }

    server.sin_family = AF_INET;
    server.sin_port = htons(port);
    server.sin_addr.s_addr = htonl(INADDR_ANY);

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == -1) {
        perror("socket");
        return EXIT_FAILURE;
    }

    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR,
                   &yes, sizeof(yes)) == -1 ||
        bind(sockfd, (struct sockaddr *)&server, sizeof(server)) == -1 ||
        listen(sockfd, 5) == -1) {
        perror("server setup");
        close(sockfd);
        return EXIT_FAILURE;
    }

    /* Only the listener is nonblocking; client sockets stay blocking. */
    flags = fcntl(sockfd, F_GETFL, 0);
    if (sockfd >= FD_SETSIZE || flags == -1 ||
        fcntl(sockfd, F_SETFL, flags | O_NONBLOCK) == -1) {
        fprintf(stderr, "Cannot prepare the listening socket.\n");
        close(sockfd);
        return EXIT_FAILURE;
    }

    printf("Loaded %d words. Server listening on port %u\n",
           word_count, (unsigned)port);

    while (!stopping) {
        fd_set ready;
        int newsockfd;
        pid_t pid;
        Child *child;

        reap_children(&children);
        FD_ZERO(&ready);
        FD_SET(sockfd, &ready);
        /* Atomically unblock signals while waiting for a connection. */
        if (pselect(sockfd + 1, &ready, NULL, NULL, NULL, &wait_mask) == -1) {
            if (errno == EINTR)
                continue;
            perror("pselect");
            status = EXIT_FAILURE;
            break;
        }
        if (stopping)
            break;

        newsockfd = accept(sockfd, NULL, NULL);

        if (newsockfd == -1) {
            if (errno == EINTR || errno == ECONNABORTED ||
                errno == EAGAIN || errno == EWOULDBLOCK)
                continue;
            perror("accept");
            status = EXIT_FAILURE;
            break;
        }

        /* Explicitly keep the shared line reader's blocking-I/O contract. */
        flags = fcntl(newsockfd, F_GETFL, 0);
        if (flags == -1 ||
            fcntl(newsockfd, F_SETFL, flags & ~O_NONBLOCK) == -1) {
            perror("client socket flags");
            close(newsockfd);
            continue;
        }

        child = malloc(sizeof(*child));
        if (child == NULL) {
            perror("malloc");
            close(newsockfd);
            continue;
        }

        pid = fork();

        if (pid == 0) {
            struct sigaction normal = {0};

            close(sockfd);
            free(child);
            /* A child must terminate on the parent's shutdown signal. */
            normal.sa_handler = SIG_DFL;
            sigemptyset(&normal.sa_mask);
            if (sigaction(SIGINT, &normal, NULL) == -1 ||
                sigaction(SIGTERM, &normal, NULL) == -1 ||
                sigaction(SIGCHLD, &normal, NULL) == -1 ||
                sigprocmask(SIG_SETMASK, &wait_mask, NULL) == -1)
                _exit(EXIT_FAILURE);
            srand((unsigned)time(NULL) ^ (unsigned)getpid());

            printf("[client %ld] Connected\n", (long)getpid());
            serve_client(newsockfd);
            close(newsockfd);
            printf("[client %ld] Disconnected\n", (long)getpid());

            _exit(EXIT_SUCCESS);
        }

        if (pid == -1) {
            perror("fork");
            free(child);
        } else {
            child->pid = pid;
            child->next = children;
            children = child;
        }

        close(newsockfd);
    }

    close(sockfd);
    stop_children(&children);
    printf("Server stopped; all client children reaped.\n");
    return status;
}
