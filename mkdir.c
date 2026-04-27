// SPDX-License-Identifier: GPL-3.0-or-later

int main (int argc, char **argv, char **envp)
{
	const char *path;
	int ret;

	if (argc != 2)
		return 1;

	path = argv[1];

	ret = mkdir(path, 0755);
	if (!ret)
		return 1;

	return 0;
}
