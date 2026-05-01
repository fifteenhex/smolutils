// SPDX-License-Identifier: GPL-3.0-or-later

#include "config.h"
#include "common.h"

static int cb_short(const char *name, int dir, void *priv)
{
	printf("%s\t", name);

	return 0;
}

static int cb_long(const char *name, int dir, void *priv)
{
        struct stat st;
	char type = '-';
	char u_r = '-', u_w = '-', u_x = '-';
	char g_r = '-', g_w = '-', g_x = '-';
	char o_r = '-', o_w = '-', o_x = '-';

        if (fstatat(dir, name, &st, AT_SYMLINK_NOFOLLOW) == -1)
                return -1;

	/* Type */
	if (st.st_mode & S_IFDIR)
		type = 'd';

	/* User permissions */
	if (st.st_mode & S_IRUSR)
		u_r = 'r';
	if (st.st_mode & S_IWUSR)
		u_w = 'w';
	if (st.st_mode & S_IXUSR)
		u_x = 'x';

	/* Group permissions */
	if (st.st_mode & S_IRGRP)
		g_r = 'r';
	if (st.st_mode & S_IWGRP)
		g_w = 'w';
	if (st.st_mode & S_IXGRP)
		g_x = 'x';

	/* Others permissions */
	if (st.st_mode & S_IROTH)
		o_r = 'r';
	if (st.st_mode & S_IWOTH)
		o_w = 'w';
	if (st.st_mode & S_IXOTH)
		o_x = 'x';

	printf("%c%c%c%c%c%c%c%c%c%c %s\n",
		type,
		u_r, u_w, u_x,
		g_r, g_w, g_x,
		o_r, o_w, o_x,
		name);

	return 0;
}

int main(int argc, char **argv, char **envp)
{
	bool long_format = false;
	char *path;
	char c;

	while ((c = getopt(argc, argv, "l")) != -1) {
		switch (c) {
		case 'l':
			long_format = true;
			break;
		}
	}

	path = (optind < argc) ? argv[optind] : ".";

	iterate_dir(path, long_format ? cb_long : cb_short, NULL);

	return 0;
}
