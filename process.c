// SPDX-License-Identifier: GPL-3.0-or-later

#include "config.h"
#include "common.h"
#include "inc/users.h"

#include "inc/multicall.h"

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

	if (argc > 1) {
		usage("usage: free\n");
		return EX_USAGE;
	}

	if (!read_file(PROC_MEMINFO, procbuf, sizeof(procbuf))) {
		error("Failed to read %s: %d\n", PROC_MEMINFO, errno);
		return EX_IOERR;
	}

	/* Impossible? */
	if (!meminfo_field("MemTotal:", &total) ||
	    !meminfo_field("MemFree:", &free_kb) ||
	    !meminfo_field("MemAvailable:", &avail)) {
		return EX_DATAERR;
	}

	/* Total up buffers, cache,... */
	cache = 0;

	foreach(field, cache_fields) {
		unsigned long val;

		if (!meminfo_field(*field, &val))
			return EX_DATAERR;

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
	char comm[1024];

	if (!read_file(comm_path, comm, sizeof(comm)))
		return;

	print_user(uid);
	printf("\t\t%s\t\t%s\n", pid, comm);
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
	if (argc > 1) {
		usage("usage: ps\n");
		return EX_USAGE;
	}

	printf("USER\t\tPID\t\tCMD\n");

	iterate_dir(proc_path, cb, NULL);

	return 0;
}

static int prog_kill(int argc, char **argv, char **envp)
{
	int sig = SIGTERM;
	int pid;
	int i = 1;

	if (argc > 1 && argv[1][0] == '-') {
		if (!parse_int_range(argv[1] + 1, 10, 0, INT_MAX, &sig)) {
			error("Not a signal: %s\n", argv[1] + 1);
			return EX_USAGE;
		}
		i++;
	}

	if ((argc - i) != 1) {
		usage("usage: kill [-<signal>] <pid>\n");
		return EX_USAGE;
	}

	/* pid_t is an int, so this can't be truncated on the way through */
	if (!parse_int_range(argv[i], 10, 1, INT_MAX, &pid)) {
		error("Not a pid: %s\n", argv[i]);
		return EX_USAGE;
	}

	if (kill(pid, sig)) {
		error("kill() failed: %d\n", errno);
		return ex_errno(errno);
	}

	return 0;
}

#define CHROOT_USAGE "usage: chroot <newroot> [<command> [args...]]\n"

/* Run a command, or a shell, with a different root */
static int prog_chroot(int argc, char **argv, char **envp)
{
	char *shell[] = { "smolsh", NULL };
	const char *path;
	char **cmd;

	if (argc < 2) {
		usage(CHROOT_USAGE);
		return EX_USAGE;
	}

	/* A command to run, or a shell in there if there isn't one */
	if (argc > 2) {
		path = argv[2];
		cmd = argv + 2;
	} else {
		path = "/bin/smolsh";
		cmd = shell;
	}

	if (chroot(argv[1])) {
		error("chroot(%s) failed: %d\n", argv[1], errno);
		return ex_errno(errno);
	}

	if (chdir("/")) {
		error("chdir(/) failed: %d\n", errno);
		return ex_errno(errno);
	}

	execve(path, cmd, envp);
	error("Failed to run %s: %d\n", path, errno);

	/* What the shell says for the same thing */
	return errno == ENOENT ? 127 : 126;
}

static const struct multicall_prog progs[] = {
	{ "chroot", prog_chroot },
	{ "free", prog_free },
	{ "kill", prog_kill },
	{ "ps", prog_ps },
};

MULTICALL_MAIN(progs)
