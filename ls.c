// SPDX-License-Identifier: GPL-3.0-or-later

int main (int argc, char **argv, char **envp)
{
	struct dirent e, *result;
	char *path = ".";
	DIR *dir;

	if (argc == 2)
		path = argv[1];

	dir = opendir(path);
	if (!dir)
		return 1;

	while ((readdir_r(dir, &e, &result) == 0) && result) {
		const char *name = e.d_name;

		if (strcmp(name, ".") == 0)
			continue;

		if (strcmp(name, "..") == 0)
			continue;

		printf("%s\n", name);
	}

	closedir(dir);

	return 0;
}
