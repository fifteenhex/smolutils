// SPDX-License-Identifier: GPL-3.0-or-later

#include "config.h"
#include "common.h"
#include "net.h"
#include "pts.h"
#include "users.h"

#include "nolibc_extensions/signal.h"

#define GETTY_PATH	"/sbin/getty"
#define GETTY_NAME	"getty"
#define SHELL_PATH	"/bin/smolsh"

#define DEFAULT_PORT	23
#define MAX_SESSIONS	4
#define BUF_SIZE	512

/* RFC 854: https://datatracker.ietf.org/doc/html/rfc854 */
#define IAC		255
#define SE		240
#define SB		250
#define WILL		251
#define WONT		252
#define DO		253
#define DONT		254

#define OPT_ECHO	1
#define OPT_SGA		3

static const uint8_t hello[] = {
	IAC, WILL, OPT_ECHO,
	IAC, WILL, OPT_SGA,
	IAC, DO, OPT_SGA,
};

enum iac {
	IAC_DATA,
	IAC_CR,
	IAC_CMD,
	IAC_OPT,
	IAC_SUB,
	IAC_SUB_IAC,
};

struct session {
	int sock;
	int master;
	enum iac state;
};

static struct session sessions[MAX_SESSIONS];

static void handle_sig(int sig)
{
}

static void setup_signals(void)
{
	struct sigaction act = {
		.sa_handler = handle_sig,
	};

	if (sigaction(SIGCHLD, &act, NULL) || sigaction(SIGPIPE, &act, NULL))
		verbose("Failed to setup signals: %d\n", errno);
}

static void session_init(struct session *s)
{
	s->master = -1;
	s->sock = -1;
}

static void session_end(struct session *s)
{
	close(s->master);
	close(s->sock);

	session_init(s);
}

static void session_start(struct session *s, int sock)
{
	char slave[32];
	char * const args[] = {
		GETTY_NAME,
		slave,
		SHELL_PATH,
		NULL
	};

	s->master = smolutils_pts_open(slave, sizeof(slave));
	if (s->master < 0) {
		close(sock);
		return;
	}

	/* TODO: This only works for the one normal user we currently have */
	if (chown(slave, SMOLUTILS_USERS_NORMAL_MIN,
		  SMOLUTILS_USERS_NORMAL_MIN))
		verbose("Failed to chown %s: %d\n", slave, errno);

	if (spawn(GETTY_PATH, args, environ) < 0) {
		error("Failed to run %s\n", GETTY_PATH);
		close(s->master);
		close(sock);
		s->master = -1;
		return;
	}

	s->sock = sock;
	s->state = IAC_DATA;

	write_full(sock, hello, sizeof(hello));
}

/*
 * Parse telnet commands but don't actually use them.
 * Just enough to extract to extract the data stream.
 */
static int strip_iac(struct session *s, char *buf, int len)
{
	int out = 0;
	int i;

	for (i = 0; i < len; i++) {
		uint8_t ch = buf[i];

		switch (s->state) {
		case IAC_CR:
			s->state = IAC_DATA;

			/* Handle encoding of a new line */
			if (ch == '\0' || ch == '\n')
				break;

			/* fall through */
		case IAC_DATA:
			if (ch == IAC) {
				s->state = IAC_CMD;
				break;
			}

			buf[out++] = ch;

			if (ch == '\r')
				s->state = IAC_CR;
			break;

		case IAC_CMD:
			/* 255 is encoded as two IACs */
			if (ch == IAC) {
				buf[out++] = ch;
				s->state = IAC_DATA;
			} else if (ch == SB)
				s->state = IAC_SUB;
			else if (ch >= WILL && ch <= DONT)
				s->state = IAC_OPT;
			else
				s->state = IAC_DATA;
			break;

		case IAC_OPT:
			s->state = IAC_DATA;
			break;

		case IAC_SUB:
			if (ch == IAC)
				s->state = IAC_SUB_IAC;
			break;

		case IAC_SUB_IAC:
			s->state = (ch == SE) ? IAC_DATA : IAC_SUB;
			break;
		}
	}

	return out;
}

/* Write with handling for 255 */
static int write_escaped(int fd, const char *buf, int len)
{
	int start = 0;
	int i;

	for (i = 0; i < len; i++) {
		/* Is this byte the same value as IAC? */
		if ((uint8_t) buf[i] != IAC)
			continue;

		/* Yes, write from the buffer head to this byte inclusive */
		if (write_full(fd, buf + start, i - start + 1))
			return -1;

		/* And another IAC to make this into 255 */
		if (write_full(fd, buf + i, 1))
			return -1;

		/* Move buffer head to after the IAC */
		start = i + 1;
	}

	/* Write the remaining tail */
	return write_full(fd, buf + start, len - start);
}

static void service(struct session *s, struct pollfd *sock,
		    struct pollfd *master)
{
	char buf[BUF_SIZE];
	int len;

	if (s->sock < 0)
		return;

	/* We got data from the client */
	if (sock->revents & POLLIN) {
		len = read(s->sock, buf, sizeof(buf));
		if (len <= 0) {
			session_end(s);
			return;
		}

		len = strip_iac(s, buf, len);
		if (len && write_full(s->master, buf, len)) {
			session_end(s);
			return;
		}
	}

	/* We want to send data to the client */
	if (master->revents & POLLIN) {
		len = read(s->master, buf, sizeof(buf));
		if (len <= 0 || write_escaped(s->sock, buf, len)) {
			session_end(s);
			return;
		}
	}

	/* We lost one end of the connection */
	if (!((sock->revents | master->revents) & POLLIN) &&
	    ((sock->revents | master->revents) & (POLLHUP | POLLERR)))
		session_end(s);
}

static void accept_one(int listener)
{
	int sock = accept4(listener, NULL, NULL, SOCK_CLOEXEC);
	int i;

	if (sock < 0)
		return;

	for (i = 0; i < MAX_SESSIONS; i++) {
		if (sessions[i].sock < 0) {
			session_start(&sessions[i], sock);
			return;
		}
	}

	verbose("Dropping incoming telnet connection, no room\n");
	close(sock);
}

#define POLLFDS_NUM (1 + (MAX_SESSIONS * 2))
#define POLLFDS_SOCK (POLLFDS_NUM - 1)
#define POLLFDS_SESSION_SOCK(_session) (2 * _session)
#define POLLFDS_SESSION_MASTER(_session) ((2 * _session) + 1)

int main (int argc, char **argv, char **envp)
{
	struct pollfd fds[POLLFDS_NUM];
	int __cleanup_fd listener = -1;
	int port = DEFAULT_PORT;
	int i, c;

	while ((c = getopt(argc, argv, "p:")) != -1) {
		switch (c) {
		case 'p':
			port = atoi(optarg);
			break;
		}
	}

	if (port <= 0 || port > 65535) {
		usage("usage: telnetd [-p port]\n");
		return 1;
	}

	for (i = 0; i < MAX_SESSIONS; i++)
		session_init(&sessions[i]);

	setup_signals();

	listener = smolutils_net_listen_tcp(port, MAX_SESSIONS);
	if (listener < 0)
		return 1;

	debug("telnetd listening on port %d\n", port);

	while (true) {
		while (waitpid(-1, NULL, WNOHANG) > 0)
			;

		for (i = 0; i < MAX_SESSIONS; i++) {
			fds[POLLFDS_SESSION_SOCK(i)].fd = sessions[i].sock;
			fds[POLLFDS_SESSION_SOCK(i)].events = POLLIN;
			fds[POLLFDS_SESSION_MASTER(i)].fd = sessions[i].master;
			fds[POLLFDS_SESSION_MASTER(i)].events = POLLIN;
		}

		fds[POLLFDS_SOCK].fd = listener;
		fds[POLLFDS_SOCK].events = POLLIN;

		if (poll(fds, ARRAY_SIZE(fds), -1) < 0) {
			if (errno == EINTR)
				continue;

			error("poll() failed: %d\n", errno);
			return 1;
		}

		if (fds[POLLFDS_SOCK].revents & POLLIN)
			accept_one(listener);

		for (i = 0; i < MAX_SESSIONS; i++)
			service(&sessions[i], &fds[POLLFDS_SESSION_SOCK(i)],
				              &fds[POLLFDS_SESSION_MASTER(i)]);
	}

	return 0;
}
