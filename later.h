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

	if (!path_join(path, sizeof(path), LATER_DIR, name)) {
		error("Name too long: %s\n", name);
		return -1;
	}

	return write_file(path, &job, sizeof(job));
}

static inline int later_read(const char *path, struct later_job *job)
{
	unsigned int i;

	if (read_struct(path, job))
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
