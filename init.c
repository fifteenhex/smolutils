int main (int argc, char **argv, char **envp)
{
	printf("smolutils init (%s, %s)\n", __DATE__, __TIME__);

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
