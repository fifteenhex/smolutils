// SPDX-License-Identifier: GPL-3.0-or-later

int main (int argc, char **argv, char **envp)
{
	kill(1, SIGUSR1);

	return 0;
}
