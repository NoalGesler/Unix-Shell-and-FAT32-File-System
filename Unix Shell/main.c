#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <time.h>

typedef struct {
	int size;
	char **items;
} tokenlist;

char *get_input(void);
tokenlist *get_tokens(char *input);

tokenlist *new_tokenlist(void);
void add_token(tokenlist *tokens, char *item);
void free_tokens(tokenlist *tokens);
void expand_Env(tokenlist *tokens); //Expands tokens with ~(home) or $(env), no return
char* path_Search(tokenlist *tokens); //Returns string with command location for use in execv(), only use with commands with no "/"
void cmd_execute(tokenlist *tokens, int pipe1, int pipe2, int inpos, int outpos);
void bg_execute(tokenlist *tokens, int pipe1, int pipe2, int inpos, int outpos, tokenlist *tokenscopy, int pid, int queuesize, int* statuspointer);

int main()
{
	time_t runTime;
	time_t finishTime;
	time_t initTime;
	time_t doneTime;
	int timeRan;
	int longest = 1;
	runTime = time(NULL);
	int status = 0;
	int queuesize = 0;
	tokenlist *tokenscopy;
	char cwd[1000];
	strcpy(cwd, getenv("PWD"));
	while (1) {
		int statcopy = waitpid(status, 0, WNOHANG);
		if(status > 0){
			printf("[%d]+ %d\t", queuesize+1, status);
			for(int i = 0; i < tokenscopy->size; i++){
				printf("%s ", tokenscopy->items[i]);
			}
			printf("\n");
		}
		printf("%s@%s:%s>", getenv("USER"), getenv("MACHINE"), cwd);
		/* input contains the whole command
		 * tokens contains substrings from input split by spaces
		 */

		char *input = get_input();
		if(input[0] == '\0'){
			continue;
		}
		if(input[0] == 'j' && input[1] == 'o' && input[2] == 'b' && input[3] == 's' && status > 0){
			printf("[%d]+ %d\t", queuesize+1, status);
                        for(int i = 0; i < tokenscopy->size; i++){
                                printf("%s ", tokenscopy->items[i]);
                        }
                        printf("\n");
			continue;
		}
		if(status > 0){
			status = 0;
		}
		tokenlist *tokens = get_tokens(input);
		tokenscopy = get_tokens(input);
		char exitchk[5] = "exit";
		exitchk[4] = '\0';
		char cdchk[3] = "cd";
		cdchk[2] = '\0';
		char echochk[5] = "echo";
		echochk[4] = '\0';
		int lasttoken = tokens->size;
		lasttoken -= 1;
		if(strstr(tokens->items[0], exitchk) != NULL){
			break;
		}
		expand_Env(tokens);
		if(strstr(tokens->items[0], cdchk) != NULL){
			int cdstatus;
			if (tokens->size > 2){
				printf("Too many arguments. syntax: cd PATH\n");
			} else if (tokens->size == 1){
				cdstatus = chdir(getenv("HOME"));
				getcwd(cwd, sizeof(cwd));
			} else {
			cdstatus = chdir(tokens->items[1]);
			getcwd(cwd, sizeof(cwd));
			}
			if(cdstatus == -1){
				printf("ERROR. syntax: cd PATH\n");
			}
			continue;
		}
		if(strstr(tokens->items[0], echochk) != NULL){
			for(int i = 1; i < tokens->size; i++){
				printf("%s ", tokens->items[i]);
			}
			printf("\n");
			continue;
		}
		int inpos = -1;
		for(int i = 0; i < tokens->size; i++){
			char* redirect = tokens->items[i];
			redirect = strchr(redirect, '<');
			if(redirect != NULL){
				inpos = i;
				break;
			}
		}
		int outpos = -1;
		for(int i = 0; i < tokens->size; i++){
			char* redirect = tokens->items[i];
			redirect = strchr(redirect, '>');
			if(redirect != NULL){
				outpos = i;
				break;
			}
		}
		int pipe1 = -1;
		int pipe2 = -1;
		for(int i = 0; i < tokens->size; i++){
			char* redirect = tokens->items[i];
			redirect = strchr(redirect, '|');
			if(redirect != NULL){
				if(pipe1 == -1){
					pipe1 = i;
				}
				else if(pipe2 == -1 && pipe1 != -1){
					pipe2 = i;
					break;
				}
				else{
					break;
				}
			}
		}
                char* command_check = (char *) malloc(sizeof(tokens->items[0]));
                strcpy(command_check, tokens->items[0]);
                command_check = strchr(command_check, '/');
                char* cmd_Path = (char *) malloc(sizeof(tokens->items[0]));
                if(command_check == NULL){
                        cmd_Path = path_Search(tokens);
                }
                else{
                        strcpy(cmd_Path, tokens->items[0]);
                }
                if(cmd_Path != NULL && tokens->items[tokens->size - 1][0] != '&'){
					initTime = time(NULL);
			cmd_execute(tokens, pipe1, pipe2, inpos, outpos);
                }
		else if(cmd_Path != NULL && tokens->items[lasttoken][0] == '&'){
			initTime = time(NULL);
			int* statuspointer;
			statuspointer = &status;
			bg_execute(tokens, pipe1, pipe2, inpos, outpos, tokenscopy, status, queuesize, statuspointer);
		}
		wait(0);
		free(input);
		free_tokens(tokens);
		free(cmd_Path);
		free(command_check);
	}
	strcpy(cwd, "");
	finishTime = time(NULL);
	timeRan = (int)finishTime - (int)runTime;
	printf("Shell ran for ");
	printf(	"%d", timeRan);
	printf(" seconds and took ");
	printf ("%d", longest);
	printf("%s\n", " second(s) to execute one command");
	fflush(stdout);
	return 0;
}

tokenlist *new_tokenlist(void)
{
	tokenlist *tokens = (tokenlist *) malloc(sizeof(tokenlist));
	tokens->size = 0;
	tokens->items = (char **) malloc(sizeof(char *));
	tokens->items[0] = NULL; /* make NULL terminated */
	return tokens;
}

void add_token(tokenlist *tokens, char *item)
{
	int i = tokens->size;

	tokens->items = (char **) realloc(tokens->items, (i + 2) * sizeof(char *));
	tokens->items[i] = (char *) malloc(strlen(item) + 1);
	tokens->items[i + 1] = NULL;
	strcpy(tokens->items[i], item);

	tokens->size += 1;
}

char *get_input(void)
{
	char *buffer = NULL;
	int bufsize = 0;

	char line[5];
	while (fgets(line, 5, stdin) != NULL) {
		int addby = 0;
		char *newln = strchr(line, '\n');
		if (newln != NULL)
			addby = newln - line;
		else
			addby = 5 - 1;

		buffer = (char *) realloc(buffer, bufsize + addby);
		memcpy(&buffer[bufsize], line, addby);
		bufsize += addby;

		if (newln != NULL)
			break;
	}

	buffer = (char *) realloc(buffer, bufsize + 1);
	buffer[bufsize] = 0;

	return buffer;
}

tokenlist *get_tokens(char *input)
{
	char *buf = (char *) malloc(strlen(input) + 1);
	strcpy(buf, input);

	tokenlist *tokens = new_tokenlist();

	char *tok = strtok(buf, " ");
	while (tok != NULL) {
		add_token(tokens, tok);
		tok = strtok(NULL, " ");
	}

	free(buf);
	return tokens;
}

void free_tokens(tokenlist *tokens)
{
	for (int i = 0; i < tokens->size; i++)
		free(tokens->items[i]);
	free(tokens->items);
	free(tokens);
}

void expand_Env(tokenlist *tokens){
	for(int i = 0; i < tokens->size; i++){
		if(tokens->items[i][0] == '$'){
			char *temp = tokens->items[i] + 1;
			char *expanded = getenv(temp);
			int j = strlen(getenv(temp));
			expanded[j] = '\0';
			tokens->items[i] = (char *) realloc(tokens->items[i], strlen(expanded)*2);
			strcpy(tokens->items[i], expanded);
		}
		else if(tokens->items[i][0] == '~'){
			char *temp = tokens->items[i] + 1;
			char *home = getenv("HOME");
			strcat(home, temp);
			tokens->items[i] = (char *) realloc(tokens->items[i], strlen(home));
			strcpy(tokens->items[i], home);
		}
	}
}

char* path_Search(tokenlist *tokens){
	char *path = getenv("PATH");
	char *command = (char *) malloc(strlen(tokens->items[0]) + 1);
	strcpy(command, tokens->items[0]);
	int max = strlen(path) + strlen(command) + 1;
	int endcheck = 0;
	int success = 0;
	char *pathcopy = path;
	while(endcheck == 0){
		char *current = (char *) malloc(strlen(path));
		strcpy(current, pathcopy);
		current = strtok(current, ":");
		pathcopy = strchr(pathcopy, ':');
		if(pathcopy == NULL){
			endcheck = 1;
		}
		else{
			pathcopy = pathcopy + 1;
		}
		strcat(current, "/");
		strcat(current, command);
		FILE *tester = fopen(current, "r");
		if(tester){
			tokens->items[0] = (char *) realloc(tokens->items[0], strlen(current)*2);
			strcpy(tokens->items[0], current);
			free(command);
			return current;
		}
		free(current);
	}
	printf("Command not found\n");
	char notfound[100] = "DNE";
	strcpy(path, notfound);
	free(command);
	return path;
}

void cmd_execute(tokenlist *tokens, int pipe1, int pipe2, int inpos, int outpos){
	if(inpos != -1 && outpos != -1){
		pid_t pid = fork();
		int fd = open(tokens->items[inpos+1], O_RDONLY, S_IROTH);
		if(pid == 0){
			close(0);
			dup(fd);
			close(fd);
			int fd2 = open(tokens->items[outpos+1], O_RDWR|O_CREAT|O_TRUNC, S_IRWXU);
			close(1);
			dup(fd2);
			close(fd2);
			execv(tokens->items[0], tokens->items);
		}
		else{
			waitpid(pid, NULL, 0);
		}
	}
	else if(inpos != -1 && outpos == -1){
		pid_t pid = fork();
		int fd = open(tokens->items[inpos+1], O_RDONLY, S_IROTH);
		if(pid == 0){
			close(0);
			dup(fd);
			close(fd);
			execv(tokens->items[0], tokens->items);
		}
		else{
			waitpid(pid, NULL, 0);
		}
	}
	else if(inpos == -1 && outpos != -1){
		pid_t pid = fork();
		int fd = open(tokens->items[outpos+1], O_RDWR|O_CREAT|O_TRUNC, S_IRWXU);
		if(pid == 0){
			close(1);
			dup(fd);
			close(fd);
			execv(tokens->items[0], tokens->items);
		}
		else{
			waitpid(pid, NULL, 0);
		}
	}
	else if(pipe1 != -1 && pipe2 != -1){
		int pd[2];
		int pds[2];
		pipe(pd);
		int pid;
		pid = fork();
		if(pid == 0){
			dup2(pd[1], 1);
			close(pd[0]);
			close(pd[1]);
			execv(tokens->items[0], tokens->items);
		}
		pipe(pds);
		pid = fork();
		if(pid == 0){
			dup2(pd[0], 0);
			dup2(pds[1], 1);
			close(pd[0]);
			close(pd[1]);
			close(pds[0]);
			close(pds[1]);
			execv(tokens->items[pipe1+1], tokens->items);
		}
		close(pd[0]);
		close(pd[1]);
		pid = fork();
		if(pid == 0){
			dup2(pds[0], 0);
			close(pds[0]);
			close(pds[1]);
			execv(tokens->items[pipe2+1], tokens->items);
		}
		close(pds[0]);
		close(pds[1]);
		waitpid(-1, NULL, 0);
		waitpid(-1, NULL, 0);
		waitpid(1, NULL, 0);
	}
	else if(pipe1 != -1 && pipe2 == -1){
		int pd[2];
		pipe(pd);
		int pid = fork();
		if(pid == 0){
			dup2(pd[1], 1);
			close(pd[1]);
			close(pd[0]);
			execv(tokens->items[0], tokens->items);
		}
		pid = fork();
		if(pid == 0){
			dup2(pd[0], 0);
			close(pd[0]);
			close(pd[1]);
			execv(tokens->items[pipe1+1], tokens->items);
		}
		close(pd[0]);
		close(pd[1]);
		waitpid(-1, NULL, 0);
		waitpid(1, NULL, 0);
	}
	else{
		pid_t pid = fork();
		if(pid == 0){
			execv(tokens->items[0], tokens->items);
		}
		else{
			waitpid(pid, NULL, 0);
		}
	}
}

void bg_execute(tokenlist *tokens, int pipe1, int pipe2, int inpos, int outpos, tokenlist *tokenscopy, int pid, int queuesize, int* statuspointer){
	queuesize++;
	if(inpos != -1 && outpos != -1){
		pid = fork();
		int fd = open(tokens->items[inpos+1], O_RDONLY, S_IROTH);
		if(pid == 0){
			close(0);
			dup(fd);
			close(fd);
			int fd2 = open(tokens->items[outpos+1], O_RDWR|O_CREAT|O_TRUNC, S_IRWXU);
			close(1);
			dup(fd2);
			close(fd2);
			execv(tokens->items[0], tokens->items);
		}
		else{
			waitpid(pid, NULL, WNOHANG);
		}
	}
	else if(inpos != -1 && outpos == -1){
		pid = fork();
		int fd = open(tokens->items[inpos+1], O_RDONLY, S_IROTH);
		if(pid == 0){
			close(0);
			dup(fd);
			close(fd);
			execv(tokens->items[0], tokens->items);
		}
		else{
			waitpid(pid, NULL, WNOHANG);
		}
	}
	else if(inpos == -1 && outpos != -1){
		pid = fork();
		int fd = open(tokens->items[outpos+1], O_RDWR|O_CREAT|O_TRUNC, S_IRWXU);
		if(pid == 0){
			close(1);
			dup(fd);
			close(fd);
			execv(tokens->items[0], tokens->items);
		}
		else{
			waitpid(pid, NULL, WNOHANG);
		}
	}
	else if(pipe1 != -1 && pipe2 != -1){
		int pd[2];
		int pds[2];
		pipe(pd);
		pid;
		pid = fork();
		if(pid == 0){
			dup2(pd[1], 1);
			close(pd[0]);
			close(pd[1]);
			execv(tokens->items[0], tokens->items);
		}
		pipe(pds);
		pid = fork();
		if(pid == 0){
			dup2(pd[0], 0);
			dup2(pds[1], 1);
			close(pd[0]);
			close(pd[1]);
			close(pds[0]);
			close(pds[1]);
			execv(tokens->items[pipe1+1], tokens->items);
		}
		close(pd[0]);
		close(pd[1]);
		pid = fork();
		if(pid == 0){
			dup2(pds[0], 0);
			close(pds[0]);
			close(pds[1]);
			execv(tokens->items[pipe2+1], tokens->items);
		}
		close(pds[0]);
		close(pds[1]);
		waitpid(pid, NULL, WNOHANG);
		waitpid(pid, NULL, WNOHANG);
		waitpid(pid, NULL, WNOHANG);
	}
	else if(pipe1 != -1 && pipe2 == -1){
		int pd[2];
		pipe(pd);
		pid = fork();
		if(pid == 0){
			dup2(pd[1], 1);
			close(pd[1]);
			close(pd[0]);
			execv(tokens->items[0], tokens->items);
		}
		pid = fork();
		if(pid == 0){
			dup2(pd[0], 0);
			close(pd[0]);
			close(pd[1]);
			execv(tokens->items[pipe1+1], tokens->items);
		}
		close(pd[0]);
		close(pd[1]);
		waitpid(pid, NULL, WNOHANG);
		waitpid(pid, NULL, WNOHANG);
	}
	else{
		pid = fork();
		if(pid == 0){
			execv(tokens->items[0], tokens->items);
		}
		else{
			waitpid(-1, NULL, WNOHANG);
		}
	}
	printf("[%d] %d\n", queuesize, pid);
	*statuspointer = pid;
}
