// SPDX-License-Identifier: GPL-3.0-or-later

/* This avoids having to use a mount command, fstab etc */
static int do_mount(const char *source, const char *target, const char *type)
{
	int ret;

	ret = mount(source, target, type, 0, NULL);
	if (ret)
		printf("mount(%s) failed: %d\n", target, ret);

	return ret;
}

static int mount_filesystems(void)
{
	int ret;

	printf("mounting filesystems...\n");

	ret = do_mount("sysfs", "/sys", "sysfs");
	if (ret)
		goto err;

	ret = do_mount("proc", "/proc", "proc");
	if (ret)
		goto err;

	return 0;

err:
	return ret;
}

int main (int argc, char **argv, char **envp)
{
	printf("smolutils init (%s, %s)\n", __DATE__, __TIME__);

	mount_filesystems();

	while (true) {
		pid_t pid = vfork();

		/* We are init */
		if (pid) {
			wait(NULL);
		}
		/* We are the new process */
		else {
			static char *newargv[] = { "sh", NULL };
			static char *newenviron[] = { NULL };

			execve("/bin/sh", newargv, newenviron);
			printf("execve failed\n");
			break;
		}
	}

	return 0;
}
