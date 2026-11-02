// SPDX-License-Identifier: GPL-3.0-or-later

#include "config.h"

#define TAG "telnetd"

#include "common.h"
#include "inc/nonblockingio.h"
#include "inc/net.h"
#include "inc/pts.h"
#include "inc/users.h"

#define GETTY_PATH	"/sbin/getty"
#define GETTY_NAME	"getty"
#define SHELL_PATH	"/bin/smolsh"

#define DEFAULT_PORT	23
#define MAX_SESSIONS	4
#define BUF_SIZE	512
#define QUEUE_SIZE	512
/*
 * This is the minimum amount of free space in our "nonblockingio"
 * buffer to bother polling for data to read, this should let the output
 * make some space so we can do a bigger read and jam that into the
 * output instead.
 */
#define MIN_READ	16

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
	struct nonblockingio master;
	RINGBUF(to_master, QUEUE_SIZE);
	enum iac state;
};

static struct session sessions[MAX_SESSIONS];

static void setup_signals(void)
{
	if (signal_interrupt(SIGCHLD) || signal_interrupt(SIGPIPE))
		verbose("Failed to setup signals: %d\n", errno);
}

static void session_init(struct session *s)
{
	nonblockingio_init(&s->master, ringbuf_init(&s->to_master));
	s->sock = -1;
}

static void session_end(struct session *s)
{
	close(s->master.fd);
	close(s->sock);

	session_init(s);
}

static void session_start(struct session *s, int *sock)
{
	int __cleanup_fd master = -1;
	char slave[32];
	char * const args[] = {
		GETTY_NAME,
		slave,
		SHELL_PATH,
		NULL
	};

	master = smolutils_pts_open(slave, sizeof(slave));
	if (master < 0)
		return;

	/* TODO: This only works for the one normal user we currently have */
	if (chown(slave, SMOLUTILS_USERS_NORMAL_MIN,
		  SMOLUTILS_USERS_NORMAL_MIN))
		verbose("Failed to chown %s: %d\n", slave, errno);

	if (spawn(GETTY_PATH, args, environ) < 0) {
		error("Failed to run %s\n", GETTY_PATH);
		return;
	}

	if (nonblockingio_attach(&s->master, master)) {
		error("Failed to set up the pty: %d\n", errno);
		return;
	}

	s->master.fd = keep_fd(&master);
	s->sock = keep_fd(sock);
	s->state = IAC_DATA;

	write_full(s->sock, hello, sizeof(hello));
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
	unsigned int want;
	int len;

	if (s->sock < 0)
		return;

	if ((master->revents & POLLOUT) && nonblockingio_drain(&s->master)) {
		session_end(s);
		return;
	}

	/* The client left while its input was still waiting for the pty */
	if (sock->revents & POLLRDHUP) {
		session_end(s);
		return;
	}

	if (sock->revents & POLLIN) {
		want = nonblockingio_room(&s->master);
		if (want > sizeof(buf))
			want = sizeof(buf);
		len = read(s->sock, buf, want);
		if (len <= 0) {
			session_end(s);
			return;
		}

		len = strip_iac(s, buf, len);
		if (len && nonblockingio_write(&s->master, buf, len)) {
			session_end(s);
			return;
		}
	}

	/* We want to send data to the client */
	if (master->revents & POLLIN) {
		len = nonblockingio_read(&s->master, buf, sizeof(buf));
		if (len < 0 || write_escaped(s->sock, buf, len)) {
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
	int __cleanup_fd sock = -1;
	int i;

	sock = accept4(listener, NULL, NULL, SOCK_CLOEXEC);
	if (sock < 0)
		return;

	for (i = 0; i < MAX_SESSIONS; i++) {
		if (sessions[i].sock < 0) {
			session_start(&sessions[i], &sock);
			return;
		}
	}

	verbose("Dropping incoming telnet connection, no room\n");
}

#define POLLFDS_NUM (1 + (MAX_SESSIONS * 2))
#define POLLFDS_SOCK (POLLFDS_NUM - 1)
#define POLLFDS_SESSION_SOCK(_session) (2 * (_session))
#define POLLFDS_SESSION_MASTER(_session) ((2 * (_session)) + 1)

int main (int argc, char **argv, char **envp)
{
	struct pollfd fds[POLLFDS_NUM];
	int __cleanup_fd listener = -1;
	uint16_t port = DEFAULT_PORT;
	int i, c;

	while ((c = getopt(argc, argv, "p:")) != -1) {
		switch (c) {
		case 'p':
			if (parse_port(optarg, &port))
				break;
			/* fall through */
		default:
			usage("usage: telnetd [-p port]\n");
			return EX_USAGE;
		}
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
			/* The socket side */
			fds[POLLFDS_SESSION_SOCK(i)].fd = sessions[i].sock;
			fds[POLLFDS_SESSION_SOCK(i)].events = POLLRDHUP |
				(nonblockingio_room(&sessions[i].master) >=
					MIN_READ ? POLLIN : 0);

			/* The pty master side */
			fds[POLLFDS_SESSION_MASTER(i)].fd =
				sessions[i].master.fd;
			fds[POLLFDS_SESSION_MASTER(i)].events =
				POLLIN |
				nonblockingio_events(&sessions[i].master);
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
