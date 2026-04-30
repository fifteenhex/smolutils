// SPDX-License-Identifier: GPL-3.0-or-later

int main (int argc, char **argv, char **envp)
{
	struct utsname u;

	if (uname(&u))
		return 1;

	printf("%s %s %s %s %s nolibc/Linux\n",
		u.sysname, u.nodename, u.release, u.version, u.machine);

	return 0;
}
