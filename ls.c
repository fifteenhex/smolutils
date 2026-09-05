// SPDX-License-Identifier: GPL-3.0-or-later

#include "config.h"
#include "common.h"
#include "colour.h"
#include "users.h"
#include "nolibc_extensions/unistd.h"

static const char *colour_for(const struct stat *st)
{
	if (S_ISDIR(st->st_mode))
		return COLOUR_BLUE;

	if (S_ISLNK(st->st_mode))
		return COLOUR_CYAN;

	if (S_ISCHR(st->st_mode) || S_ISBLK(st->st_mode))
		return COLOUR_YELLOW;

	if (st->st_mode & (S_IXUSR | S_IXGRP | S_IXOTH))
		return COLOUR_GREEN;

	return "";
}

static const char *colour_of(int dir, const char *name)
{
	struct stat st;

	if (!colour_on())
		return "";

	if (fstatat(dir, name, &st, AT_SYMLINK_NOFOLLOW) == -1)
		return "";

	return colour_for(&st);
}

static int cb_short(const char *name, int dir, void *priv)
{
	const char *with = colour_of(dir, name);

	printf("%s%s%s\t", with, name, colour_end(with));

	return 0;
}

static void print_perms(bool r, bool w, bool x)
{
	char _r = '-', _w = '-', _x = '-';

	if (r)
		_r = 'r';
	if (w)
		_w = 'w';
	if (x)
		_x = 'x';

	printf("%c%c%c", _r, _w, _x);
}

static void print_user(uid_t uid)
{
	const char *user = users_map_user(uid);

	if (user)
		printf("%10s", users_map_user(uid));
	else
		printf("%10u", (unsigned int) uid);
}

static void print_group(uid_t gid)
{
	if (gid == 0)
		printf("%10s", "root");
	else
		printf("%10u", gid);
}

/*
 * Path max is a whole page, but if you are making paths that are a page
 * long you are not using this ls.
 */
#define TARGET_MAX	256

static void print_target(int dir, const char *name, const struct stat *st)
{
	char target[TARGET_MAX];
	ssize_t got;

	if (!is_enabled(CONFIG_LS_DETAIL))
		return;

	if (!S_ISLNK(st->st_mode))
		return;

	got = readlinkat(dir, name, target, sizeof(target) - 1);
	if (got < 0)
		return;

	target[got] = '\0';

	printf(" -> %s", target);
}

static int cb_long(const char *name, int dir, void *priv)
{
	const char *with;
	char type = '-';
	struct stat st;

	if (fstatat(dir, name, &st, AT_SYMLINK_NOFOLLOW) == -1)
		return -1;

	/* Type */
	if (S_ISDIR(st.st_mode))
		type = 'd';
	else if (S_ISLNK(st.st_mode))
		type = 'l';
	else if (S_ISCHR(st.st_mode))
		type = 'c';
	else if (S_ISBLK(st.st_mode))
		type = 'b';

	printf("%c", type);

	/* User permissions */
	print_perms(!!(st.st_mode & S_IRUSR),
		    !!(st.st_mode & S_IWUSR),
		    !!(st.st_mode & S_IXUSR));

	/* Group permissions */
	print_perms(!!(st.st_mode & S_IRGRP),
		    !!(st.st_mode & S_IWGRP),
		    !!(st.st_mode & S_IXGRP));

	/* Others permissions */
	print_perms(!!(st.st_mode & S_IROTH),
		    !!(st.st_mode & S_IWOTH),
		    !!(st.st_mode & S_IXOTH));

	/* hard links */
	printf(" %5lu", (unsigned long)st.st_nlink);

	/* user */
	print_user(st.st_uid);

	/* gid */
	print_group(st.st_gid);

	/* size */
	printf(" %10lld", (long long)st.st_size);

	with = colour(colour_for(&st));
	printf(" %s%s%s", with, name, colour_end(with));

	/* target if symlink */
	print_target(dir, name, &st);

	printf("\n");

	return 0;
}

int main(int argc, char **argv, char **envp)
{
	bool long_format = false;
	char *path;
	int c;

	colour_setup();

	while ((c = getopt(argc, argv, "l")) != -1) {
		switch (c) {
		case 'l':
			long_format = true;
			break;
		}
	}

	path = (optind < argc) ? argv[optind] : ".";

	if (iterate_dir(path, long_format ? cb_long : cb_short, NULL) < 0) {
		error("Failed to list %s: %d\n", path, errno);
		return 1;
	}

	if (!long_format)
		printf("\n");

	return 0;
}
