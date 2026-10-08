// SPDX-License-Identifier: GPL-3.0-or-later

#include "config.h"
#include "common.h"
#include "users.h"

#include "multicall.h"

static const char *proc_path = "/proc";

#define PROC_MEMINFO	"/proc/meminfo"
#define PROC_FILE_MAX	4096

static char procbuf[PROC_FILE_MAX];

static const char *next_line(const char *p)
{
	p = strchr(p, '\n');

	return p ? p + 1 : NULL;
}

static bool meminfo_field(const char *key, unsigned long *out)
{
	size_t keylen = strlen(key);
	const char *p;

	for (p = procbuf; p && *p; p = next_line(p)) {
		char *end;

		if (strncmp(p, key, keylen))
			continue;

		*out = strtoul(p + keylen, &end, 10);

		return end != p + keylen;
	}

	return false;
}

static const char * const cache_fields[] = {
	"Buffers:",
	"Cached:",
	"SReclaimable:",
};

static int prog_free(int argc, char **argv, char **envp)
{
	unsigned long free_kb;
	unsigned long total;
	unsigned long avail;
	unsigned long cache;
	unsigned long used;

	if (!read_file(PROC_MEMINFO, procbuf, sizeof(procbuf))) {
		error("Failed to read %s: %d\n", PROC_MEMINFO, errno);
		return 1;
	}

	/* Impossible? */
	if (!meminfo_field("MemTotal:", &total) ||
	    !meminfo_field("MemFree:", &free_kb) ||
	    !meminfo_field("MemAvailable:", &avail)) {
		return 1;
	}

	/* Total up buffers, cache,... */
	cache = 0;

	foreach(field, cache_fields) {
		unsigned long val;

		if (!meminfo_field(*field, &val))
			return 1;

		cache += val;
	}

	used = total - avail;

	printf("%18s %12s %12s %12s\n", "total", "used", "free", "buff/cache");
	printf("Mem: %13lu %12lu %12lu %12lu\n", total, used, free_kb, cache);

	return 0;
}

static void print_user(uid_t uid)
{
	const char *user = users_map_user(uid);

	if (user)
		printf("%s", user);
	else
		printf("%u", (unsigned int) uid);
}

static void print_process(const char *pid, const char *comm_path, uid_t uid)
{
	char tmp[1024];
	int len;
	int __cleanup_fd fd = -1;

	fd = open(comm_path, O_RDONLY);
	if (fd < 0)
		return;

	len = read(fd, tmp, sizeof(tmp) - 1);
	if (len <= 0)
		return;

	/* Drop the trailing newline */
	if (tmp[len - 1] == '\n')
		len--;
	tmp[len] = '\0';

	print_user(uid);
	printf("\t\t%s\t\t%s\n", pid, tmp);
}

static int cb(const char *name, int dir, void *priv)
{
	char tmp[1024];
	struct stat st;

	if (strcmp(name, "self") == 0)
		return 0;

	if (strcmp(name, "thread-self") == 0)
		return 0;

	sprintf(tmp, "%s/%s/comm", proc_path, name);

	/* If there isn't a comm file then this isn't a process? */
	if (access(tmp, F_OK))
		return 0;

	if (fstatat(dir, name, &st, 0))
		return 0;

	print_process(name, tmp, st.st_uid);

	return 0;
}

static int prog_ps(int argc, char **argv, char **envp)
{
	printf("USER\t\tPID\t\tCMD\n");

	iterate_dir(proc_path, cb, NULL);

	return 0;
}

static int prog_kill(int argc, char **argv, char **envp)
{
	int sig = SIGTERM;
	char *end;
	long pid;
	int i = 1;

	if (argc > 1 && argv[1][0] == '-') {
		sig = strtol(argv[1] + 1, &end, 10);
		if (*end != '\0')
			return 1;
		i++;
	}

	if ((argc - i) != 1) {
		usage("usage: kill [-<signal>] <pid>\n");
		return 1;
	}

	pid = strtol(argv[i], &end, 10);

	/*
         * pid_t is 32 bits but the value we just parsed might get
         * truncated resulting in badness so we do a cast to check
         * for truncation.
         */
	if (*end != '\0' || pid < 1 || (long) (pid_t) pid != pid) {
		error("Not a pid: %s\n", argv[i]);
		return 1;
	}

	if (kill(pid, sig)) {
		error("kill() failed: %d\n", errno);
		return 1;
	}

	return 0;
}

static const struct multicall_prog progs[] = {
	{ "free", prog_free },
	{ "kill", prog_kill },
	{ "ps", prog_ps },
};

int main (int argc, char **argv, char **envp)
{
	MULTICALL_DISPATCH(argv[0], progs);

	return 1;
}
