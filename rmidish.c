/*
 * This program gives an example on
 * how to create a front-end to midish
 *
 * first we create a pair of pipes corresponding to
 * the stdin/stdout of midish and we start it.
 *
 * then, to send commands to midish we write to its standard input 
 * (though the first pipe) and to receive the results we read its 
 * standart output (trough the second pipe).
 */


#include <signal.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <readline/readline.h>
#include <readline/history.h>

#define MIDISHBIN "midish"

int midish_pid;
FILE *midish_stdout, *midish_stdin;

#define LINELENGTH 10000
char linebuf[LINELENGTH + 1];
unsigned linenum;
	
void
sighandler(int s) {
	kill(midish_pid, s);
}

	/*
	 * send a line to stdin of midish
	 * just write it to the pipe
	 */
	 
void
sendline(char *buf) {
	fprintf(midish_stdin, "%s\n", buf);
	fflush(midish_stdin);
	linenum++;
}

	/*
	 * wait for midish to become ready
	 * read the stdout of midish and print everything
	 * to the real stdout, stop reading when midish issues "+ready\n"
	 */

void
waitready(void) {
	for (;;) {
		if (fgets(linebuf, LINELENGTH, midish_stdout) == NULL) {
			/* midish terminanted */
			exit(0);
		}
		if (strcmp(linebuf, "+ready\n") == 0) {
			/* midish is now waiting for new commands */
			break;
		}
		fprintf(stdout, "%s", linebuf);
	}
	fflush(stdout);
}

	/*
	 * setup a pair of pipes, (stdin and stdout of midish)
	 * and start midish.
	 *
	 * since midish uses SIGINT, it is also necessary
	 * to catch SIGINT
	 */


void
startmidish(void) {
	int ipipe[2], opipe[2];
	struct sigaction sa;
	
	/*
	 * allocate pipes and fork
	 */
	if (pipe(ipipe) < 0 || pipe(opipe) < 0 ) {
		perror("pipe");
		exit(1);
	}
	midish_pid = fork();
	if (midish_pid < 0) {
		perror("fork");
		exit(1);
	}
	if (midish_pid == 0) {
		close(ipipe[1]);
		close(opipe[0]);
		if (dup2(ipipe[0], STDIN_FILENO) < 0) {
			perror("midish_pid/dup2(stdin)");
			exit(1);
		}
		if (dup2(opipe[1], STDOUT_FILENO) < 0) {
			perror("midish_pid/dup2(stdout)");
			exit(1);
		}
		close(ipipe[0]);
		close(opipe[1]);
		if (execlp(MIDISHBIN, MIDISHBIN, "-v", (char *)NULL) < 0) {
			perror(MIDISHBIN);
		}
		exit(1);
	}
	close(ipipe[0]);
	close(opipe[1]);
	midish_stdout = fdopen(opipe[0], "r");
	if (midish_stdout == NULL) {
		perror("fdopen(midish_stdout)");
		exit(1);
	}
	midish_stdin = fdopen(ipipe[1], "w");
	if (midish_stdin == NULL) {
		perror("fdopen(midish_stdin)");
		exit(1);
	}
	sa.sa_handler = sighandler;
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = SA_RESTART;
	if (sigaction(SIGINT, &sa, NULL) < 0) {
		perror("sigaction");
		exit(1);
	}
	linenum = 1;
}

int
main() {
#define PROMPTLENGTH 20
	char *rl, prompt[PROMPTLENGTH];

	/*
	 * if stdin or stdout is not a tty, then dont start the front end
	 * execute midish
	 */

	if (!isatty(STDIN_FILENO) || !isatty(STDOUT_FILENO)) {
		if (execlp(MIDISHBIN, MIDISHBIN, (char *)NULL) < 0) {
			perror(MIDISHBIN);
		}
		exit(1);
	}		
	
	startmidish();
	
	for (;;) {
		waitready();
		snprintf(prompt, PROMPTLENGTH, "%d> ", linenum);
		rl = readline(prompt);
		if (rl == NULL) {
			 break;
		}
		if (rl[0] != '\0') {
			add_history(rl);
		}
		sendline(rl);
		free(rl);
	}
	return 0;
}
