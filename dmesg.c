// SPDX-License-Identifier: GPL-3.0-or-later

#include "common.h"

#define MSG_MAX 2048

static int readone(int fd, char *tmp, size_t sz, char **msg)
{
	int ret;
	char *_msg;
	char *_msg_end;

	ret = read(fd, tmp, sz - 1);
	if (ret == 0)
		return -EAGAIN;
	if (ret < 0)
		return ret;

	/* Terminate line before we start using str functions */
	tmp[ret] = '\0';

	/*
	 * There should be a newline at the end of
	 * message and before any continuations if there
	 * are any.
	 */
	_msg_end = strchr(tmp, '\n');
	if (!_msg_end)
		goto bad_record;

	/* Terminate the message part so we can print it */
	*_msg_end = '\0';

	/* Start of the message is indicated by ';' */
	_msg = strchr(tmp, ';');
	if (!_msg)
		goto bad_record;

	/* Terminate the flags and set the pointer to the message */
	*_msg++ = '\0';
	*msg = _msg;

	return 0;

bad_record:
	printf("bad record\n");
	return -EINVAL;
}

static char tmp[MSG_MAX + 1];

int main (int argc, char **argv, char **envp)
{
	int __cleanup_fd fd = -1;
	int len;

	fd = open("/dev/kmsg", O_RDONLY | O_NONBLOCK);
	if (fd < 0)
		return 1;

	while (true) {
		int ret;
		char *msg;

		ret = readone(fd, tmp, sizeof(tmp), &msg);
		if (ret)
			break;

		printf("%s\n", msg);
	}

	return 0;
}
