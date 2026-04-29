// SPDX-License-Identifier: GPL-3.0-or-later

static const char *proc_path = "/proc";

static void print_process(const char *pid, const char *comm_path)
{
	char tmp[1024];
	int len;
	int fd;

	fd = open(comm_path, O_RDONLY);

	len = read(fd, tmp, sizeof(tmp));

	tmp[len - 1] = '\0';

	printf("%s\t%s\n", pid, tmp);

	close(fd);
}

int main (int argc, char **argv, char **envp)
{
	struct dirent e, *result;
	char tmp[1024];
	DIR *dir;

	dir = opendir(proc_path);
	if (!dir)
		return 1;

	printf("PID\tCMD\n");

	while ((readdir_r(dir, &e, &result) == 0) && result) {
		const char *name = e.d_name;

                if (strcmp(name, "self") == 0)
                        continue;

                if (strcmp(name, "thread-self") == 0)
                        continue;

		sprintf(tmp, "%s/%s/comm", proc_path, name);

		/* If there isn't a comm file then this isn't a process? */
		if (access(tmp, F_OK)) {
			continue;
		}

		//printf("%s\n", tmp);

		print_process(name, tmp);
	}

	closedir(dir);

	return 0;
}
