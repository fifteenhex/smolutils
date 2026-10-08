// SPDX-License-Identifier: GPL-3.0-or-later

#include "config.h"
#include "common.h"

#define PROC_MOUNTS "/proc/mounts"

typedef long long df_size_t;

/* Divide to kb first to keep within 32bits */
static df_size_t blocks_to_kb(df_size_t blocks, unsigned long bsize)
{
	if (is_enabled(CONFIG_DF_LARGE))
		return (blocks * bsize) / 1024;

	return (unsigned long) blocks * (bsize / 1024);
}

static unsigned int use_percent(df_size_t total, df_size_t used)
{
	if (is_enabled(CONFIG_DF_LARGE))
		return total ? (unsigned int) (used * 100 / total) : 0;

	if (total < 100)
		return used ? 100 : 0;

	return (unsigned long) used / ((unsigned long) total / 100);
}

static char linebuf[1024];

struct mount {
	char *dev;
	char *mountpoint;
	char *type;
	char *opts;
	char *dump;
	char *pass;
};

static int parse_mount(char *line, struct mount *mount)
{
	char *dev;
	char *mountpoint;
	char *type;
	char *opts;
	char *dump;
	char *pass;

	dev = next_field(&line);
	if (!dev) {
		verbose("Didn't find dev in line\n");
		return -EINVAL;
	}

	/* Mount point should be after the dev */
	mountpoint = next_field(&line);
	if (!mountpoint) {
		verbose("Didn't find mountpoint in line\n");
		return -EINVAL;
	}

	/* type should be after the mount point */
	type = next_field(&line);
	if (!type) {
		verbose("Didn't find type in line\n");
		return -EINVAL;
	}

	/* options should be after the type */
	opts = next_field(&line);
	if (!opts) {
		verbose("Didn't find opts in line\n");
		return -EINVAL;
	}

	/* dump should be after the options */
	dump = next_field(&line);
	if (!dump) {
		verbose("Didn't find dump in line\n");
		return -EINVAL;
	}

	/* pass should be after dump */
	pass = next_field(&line);
	if (!pass) {
		verbose("Didn't find pass in line\n");
		return -EINVAL;
	}

	mount->dev = dev;
	mount->mountpoint = mountpoint;
	mount->type = type;
	mount->opts = opts;
	mount->dump = dump;
	mount->pass = pass;

	return 0;
}

static int process_line(char *line)
{
	df_size_t total = 0;
	df_size_t avail = 0;
	df_size_t used = 0;
	struct mount mount;
	struct statfs buf;
	unsigned int usepercent = 0;
	int ret;

	ret = parse_mount(line, &mount);
	if (ret)
		return ret;

	ret = statfs(mount.mountpoint, &buf);
	if (ret) {
		verbose("stafs(%s, ..) failed: %d\n", mount.mountpoint, errno);
		return -1;
	}

	if (buf.f_bsize) {
		total = blocks_to_kb(buf.f_blocks, buf.f_bsize);
		avail = blocks_to_kb(buf.f_bavail, buf.f_bsize);
		used  = blocks_to_kb(buf.f_blocks - buf.f_bfree, buf.f_bsize);
		usepercent = use_percent(total, used);
	}

	printf("%-20s %12lld %12lld %12lld %4u%% %s\n",
		mount.dev, total, used, avail, usepercent, mount.mountpoint);

	return 0;
}

static void df_line(char *line, void *priv)
{
	int *ret = priv;

	/* Only process the line if there wasn't already an error */
	if (!*ret)
		*ret = process_line(line);
}

int main (int argc, char **argv, char **envp)
{
	int lineret = 0;
	int dropped;

	printf("%-20s %12s %12s %12s %5s %s\n",
		"Filesystem", "1K-blocks", "Used", "Available", "Use%", "Mounted on");

	dropped = read_lines(PROC_MOUNTS, linebuf, sizeof(linebuf),
			     df_line, &lineret);
	if (dropped < 0) {
		verbose("Failed to open %s\n", PROC_MOUNTS);
		return 1;
	}

	if (dropped) {
		verbose("Line too long\n");
		return 1;
	}

	if (lineret) {
		verbose("Failed to process input line\n");
		return 1;
	}

	return 0;
}
