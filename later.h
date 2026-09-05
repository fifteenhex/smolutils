// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef _SMOLUTILS_LATER_H
#define _SMOLUTILS_LATER_H

/* Poor man's cron, mainly for dhcp renewal */

#define LATER_DIR	SMOL_RUN_PRIVATE_DIR "/later"

#define LATER_ARGS	4
#define LATER_ARG_MAX	64

struct later_job {
	uint32_t when;

	char argv[LATER_ARGS][LATER_ARG_MAX];
};

static inline uint32_t later_now(void)
{
	struct timespec ts;

	if (clock_gettime(CLOCK_MONOTONIC, &ts))
		return 0;

	return ts.tv_sec;
}

static inline int later_ask(const char *name, unsigned int secs,
			    const char * const *argv)
{
	struct later_job job = { 0 };
	int __cleanup_fd fd = -1;
	char path[64];
	unsigned int i;

	for (i = 0; argv[i]; i++) {
		if (i == LATER_ARGS) {
			error("Too many arguments for %s\n", name);
			return -1;
		}

		if (strlen(argv[i]) >= LATER_ARG_MAX) {
			error("Argument too long for %s: %s\n", name, argv[i]);
			return -1;
		}

		strcpy(job.argv[i], argv[i]);
	}

	job.when = later_now() + secs;

	if (snprintf(path, sizeof(path), "%s/%s", LATER_DIR, name)
	    >= (int) sizeof(path)) {
		error("Name too long: %s\n", name);
		return -1;
	}

	fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
	if (fd < 0) {
		error("Failed to open %s: %d\n", path, errno);
		return -1;
	}

	if (write(fd, &job, sizeof(job)) != sizeof(job)) {
		error("Failed to write %s: %d\n", path, errno);
		return -1;
	}

	return 0;
}

static inline int later_read(const char *path, struct later_job *job)
{
	int __cleanup_fd fd = -1;
	struct stat st;
	unsigned int i;

	fd = open(path, O_RDONLY | O_NONBLOCK | O_NOFOLLOW);
	if (fd < 0)
		return -1;

	if (fstat(fd, &st) || !S_ISREG(st.st_mode))
		return -1;

	if (read(fd, job, sizeof(*job)) != sizeof(*job))
		return -1;

	for (i = 0; i < LATER_ARGS; i++)
		job->argv[i][LATER_ARG_MAX - 1] = '\0';

	/* Nothing to run means nothing to do */
	if (!job->argv[0][0])
		return -1;

	/* An absolute path, so it can't be a name found along a PATH */
	if (job->argv[0][0] != '/')
		return -1;

	return 0;
}

#endif /* _SMOLUTILS_LATER_H */
