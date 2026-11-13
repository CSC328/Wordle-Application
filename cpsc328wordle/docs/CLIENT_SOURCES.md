# Sources used for the client

Course examples were supplied in P-5.zip and read on November 12, 2026.
Citations use the authors and dates written in those files. Missing author
or date information is identified below rather than guessed.

The archive has duplicate socket examples under examples/sockets/ and
sockets/. The paths below use the examples/ copies.

## Cited sources

1. R. Walther, modified from L. Frye. **CLAs.c**. Date not listed.
   Path: P-5.zip, examples/CLAs.c.
   Retrieved November 12, 2026 from Prof. Reiley Walther's provided
   CPSC 328 class files.
   Used for the main argc/argv checks and usage-message pattern.

2. Dr. Lisa Frye. **client_allAddrs.c**. Last modification listed:
   September 1, 2021. Path: P-5.zip,
   examples/sockets/sum/client_allAddrs.c.
   Retrieved November 12, 2026 from the same class files.
   Used for the getaddrinfo/address-list/socket/connect/close pattern in
   connect_to_server(). The class file also credits Stevens and Matt
   Muggeridge's "Programming with TCP/IP - Best Practices."

3. W. Richard Stevens, Bill Fenner and Andrew M. Rudoff. **utils.c**,
   readline(), from *UNIX Network Programming: The Sockets Networking API*,
   Volume 1, Third Edition. Documentation modified by Dr. Lisa Frye,
   October 2017. Path: P-5.zip, examples/sockets/sum/utils.c.
   Retrieved November 12, 2026 from the same class files.
   Used for the byte-at-a-time line-read, newline and EOF pattern in
   read_input(). The client adds overlong-line draining, NUL rejection and
   server monitoring required for this project.

4. **daytimed.c**. Author and date not listed in the supplied file.
   Path: P-5.zip, examples/sockets/UDPtimed/daytimed.c.
   Retrieved November 12, 2026 from the same class files.
   Used for FD_ZERO, FD_SET, select and FD_ISSET with two descriptors.
   The Wordle client watches stdin and its connected TCP socket.

5. Charanya Ananthula. **Group 17 shared-library handoff**, November 2026.
   Files: wordle_common.h, wordle_common.c and HANDOFF.md.
   These completed tasks 2.1-2.4 supply the shared validation and message
   functions. The client uses them unchanged.

6. **echoServer.py**. Author and date not listed in the supplied file.
   Path: P-5.zip, examples/sockets/echoPython/echoServer.py.
   Retrieved November 12, 2026 from the same class files.
   Used for socket creation, SO_REUSEADDR, bind/listen and connection
   cleanup in the Python test peer. The Wordle protocol script and test
   assertions are specific to this project. This source applies to the
   test fixture; the team's server.c is unchanged.

## Team and design sources

- The existing Makefile and shared API from source [5] are unchanged.
- Group_17_Network_Program_Design_Final(1).pdf, sections 5, 6, 8, 9.1 and 12:
  client behavior, five-letter guesses, six attempts, feedback, replay,
  :quit/EOF cleanup and test expectations.
- CSC328/Wordle-Application at commit
  54cf66f93f3c8c94e24dd85240c94eee58cb04e2: the reviewed project baseline.
  https://github.com/CSC328/Wordle-Application
- DietrichGebert/ponytail: README.md, AGENTS.md and
  skills/ponytail/SKILL.md. Applied its guidance to read the affected code,
  reuse existing helpers and keep the change within the requested tasks.
  https://github.com/DietrichGebert/ponytail

## Adaptation notes

The client retains the project's shared port validation and LF framing.
It initializes its own addrinfo hints, frees the list using freeaddrinfo,
checks connection failures and closes failed sockets before trying another
address. The address lookup accepts both IPv4 and IPv6 with AF_UNSPEC.
Passing the validated numeric service to getaddrinfo also fills in the port
in network byte order.

read_input() checks the LineReader's buffered bytes before waiting on the
socket, because recv_line may already have received a following message.
It resets the descriptor set before each select call, retries EINTR and
guards FD_SETSIZE. Guesses continue to stay in the client.

The assignment uses TCP. No UDP transport, binary sum messages, forked
server implementation or additional runtime dependency was added to the
application. The new client references the existing static library.

The client header lists numbered citations and the adapted code sections
identify their sources inline. The board, alphabet, repeated-letter
scoring and replay implementation follow the team's design.
