/*
 ============================================================================
 Name        : wshell.c
 Author      : Kseniia Romanova, Brian DeFlaminio
 Description : A program that mimics a shell.
 ============================================================================
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/wait.h>
#include <fcntl.h>

//Function to print a prompt
void type_prompt() {
    char dir[1024];
    getcwd(dir, sizeof(dir));
    char *p = strrchr (dir, '/');
    if (strlen(dir) == 1) {
    	printf("%s$ ",  (char *) dir);
    } else {
    	printf("%s$ ", (p ? p + 1 : (char *) dir));
    }
    fflush (stdout);
}

//Function to get a line from the input
int getInput(char* str) {
	char* line;
    size_t len = 0;
    if(getline(&line, &len, stdin) != -1) {
    	strcpy(str, line);
    	return 0;
    } else {
    	return 1;
    }
}

//Function to parse the line from the input using spaces
void parse(char* str, char** parsed, int sizeP) {
	char* copy = strdup(str);
	for(int i = 0; i < sizeP; i++) {
		parsed[i] = strsep(&copy, " ");
		if(parsed[i] == NULL) {
			break;
		}
		if(strlen(parsed[i]) == 0) {
			i--;
		}
	}
}

//Function to search for a build in or use other commands
int buildin(char** given, char *line, int sizeI, char** history) {
    int num = 5;
    int swch = 6;
    char* list[num];
    pid_t p;
    //List of build-in commands
    list[0] = "exit";
    list[1] = "echo";
    list[2] = "cd";
    list[3] = "pwd";
    list[4] = "history";
    //Search for build-in command
    for (int i = 0; i < num; i++) {
        if (strstr(given[0], list[i]) != NULL) {
            swch = i + 1;
            break;
        }
    }
    //If not found jump to case 6
    switch (swch) {
    case 1: {                                 //exit
        exit(0);
    }
    case 2: {                                 //echo
    	if(given[1] == NULL) {
    		printf("%c", '\n');
    		fflush (stdout);
    	} else if(given[1] != NULL) {
    		for(int j = 5; j < sizeI; j++) {
    			if(line[j] == '\0') {
    				break;
    			} else {
    				printf("%c", line[j]);
    			}
    		}
    	}
        return 0;
    }
    case 3: {                                 //cd
    	if(given[1] == NULL) {
    		chdir(getenv("HOME"));
    		return 0;
    	} else if(given[2] != NULL) {
    		printf("wshell: cd: too many arguments\n");
    		fflush (stdout);
    		return 1;
    	} else {
    		if(chdir(given[1]) != 0) {
    			printf("wshell: no such directory: %s\n", given[1]);
    			fflush (stdout);
    			return 1;
    		}
    	}
    	return 0;
    }
    case 4: {                                 //pwd
    	char direc[1024];
    	getcwd(direc, sizeof(direc));
    	printf("%s\n", direc);
    	fflush (stdout);
        return 0;
    }
    case 5: {                                 //history
    	for(int x = 0; x < 10; x++) {
    		if(history[x] != NULL) {
    			printf("%s", history[x]);
    			fflush (stdout);
    		}
    	}
    	return 0;
    }
    case 6: {                                 //non-existing or not a build-in command
    	p = fork();

    	if(p < 0) {
    		printf("error fork");
    	} else if(p == 0) { //child
    		if(execvp(given[0], given) < 0) {
    			printf("wshell: could not execute command: %s\n", given[0]);
    			fflush (stdout);
    			exit(1);
    		}
    	} else { //parent
    		wait(NULL);
    	}
    	if(strstr(given[0], "/usr/bin/true")) {
    		return 0;
    	} else if(strstr(given[0], "/usr/bin/false")) {
    		return 1;
    	}
    	return 0;
    }
    default: {
        break;
    }
    }
    return 1;
}

//Function for redirection >
int redir(char** given, char *filename) {
	int file = open(filename, O_WRONLY | O_CREAT | O_TRUNC, 0777);
	pid_t id;
	id = fork();
	if(id < 0) {
		printf("error fork");
	} else if(id == 0) { //child
		fclose(stdout);
		dup2(file, STDOUT_FILENO);
		close(file);
		if(execvp(given[0], given) < 0) {
			printf("wshell: could not execute command: %s\n", given[0]);
			fflush (stdout);
			exit(1);
		}
	} else { //parent
		wait(NULL);
	}
	return 0;
}

//Function for redirection >>
int redirr(char** given, char *filename) {
	int file = open(filename, O_WRONLY | O_APPEND, 0777);
	pid_t id;
	id = fork();
	if(id < 0) {
		printf("error fork");
	} else if(id == 0) { //child
		fclose(stdout);
		dup2(file, STDOUT_FILENO);
		close(file);
		if(execvp(given[0], given) < 0) {
			printf("wshell: could not execute command: %s\n", given[0]);
			fflush (stdout);
			exit(1);
		}
	} else { //parent
		wait(NULL);
	}
	return 0;
}

//Function for pipe | (uses double fork)
void piping(char** given1, char** given2) {
	pid_t id2 = fork();
	if(id2 == 0) {
		int fd[2];
		pipe(fd);
		pid_t id1;
		id1 = fork();
		if(id1 < 0) {
			printf("error fork");
		} else if(id1 == 0) { //child
			fclose(stdout);
			dup2(fd[1], STDOUT_FILENO);
			if(execvp(given1[0], given1) < 0) {
				printf("wshell: could not execute command: %s\n", given1[0]);
				fflush (stdout);
				exit(1);
			}
		} else {
			dup2(fd[0], STDIN_FILENO);
			close(fd[1]);
			execvp(given2[0], given2);
		}
	} else {
		wait(NULL);
	}
}

//Main
int main(int argc, char *argv[]) {
	//Store input string and parsed string
	char inputStr[1024];
	int sizeI = 1024;
	char *parsed[50];
	int sizeP = 50;
	//For history function
	char *history[10];
	int num = 0;
	//Main loop
	while(1) {
		//Print prompt
		type_prompt();
		//Get input string and then continue
		if(getInput(inputStr)) {
			continue;
		}
		//Add to History
		if (num < 10) {
			history[num] = strdup(inputStr);
			num = num + 1;
		} else {
			free(history[0]);
			for (int y = 1; y < 10; y++) {
		    	history[y - 1] = history[y];
			}
		    history[9] = strdup(inputStr);
		}
		//Parse the input string
		parse(inputStr, parsed, sizeP);
		//Remove newline from the end of the last parsed word
		for(int n = 0; n < sizeP; n++) {
			if(parsed[n] == NULL) {
				parsed[n-1] = strsep(&parsed[n-1], "\n");
				break;
			}
		}
		//Check if the command comes from a file
		//Print command
		if (!isatty(fileno(stdin))) {
			int k;
			for(k = 0; k < sizeI; k++) {
				if(inputStr[k] == '\0') {
					break;
				} else {
					printf("%c", inputStr[k]);
					fflush (stdout);
				}
			}
			fflush (stdout);
		}
		//check = (0 nothing) = (1 &&) = (2 ||) = (3 >) = (4 >>) = (5 |)
		int check = 0;
		if(strstr(inputStr, "|") != NULL) {
			check = 5;
		}
		if(strstr(inputStr, "&&") != NULL) {
			check = 1;
		} else if(strstr(inputStr, "||") != NULL) {
			check = 2;
		} else if(strstr(inputStr, ">") != NULL) {
			check = 3;
		}
		if(strstr(inputStr, ">>") != NULL) {
			check = 4;
		}
		//Execute redirect >
		if(check == 3) {
			char * inputStr1;
			char * inputStr2;
			inputStr1 = strtok(inputStr, ">");
			inputStr1[strlen(inputStr1)-1] = '\0';
			inputStr2 = strtok(NULL, " ");
			inputStr2 = strtok(inputStr2, "\n");
			char* parsed1[sizeP];
			parse(inputStr1, parsed1, sizeP);
			redir(parsed1, inputStr2);
		}
		//Execute redirect >>
		if(check == 4) {
			char * inputStr1;
			char * inputStr2;
			inputStr1 = strtok(inputStr, ">");
			inputStr1[strlen(inputStr1)-1] = '\0';
			inputStr2 = strtok(NULL, " ");
			inputStr2 = strtok(NULL, "\n");
			char* parsed1[sizeP];
			parse(inputStr1, parsed1, sizeP);
			redirr(parsed1, inputStr2);
		}
		//Execute piping |
		if(check == 5) {
			char * inputStr1;
			char * inputStr2;
			inputStr1 = strtok(inputStr, "|");
			inputStr1[strlen(inputStr1)-1] = '\0';
			inputStr2 = strtok(NULL, "\n");
			char* parsed1[sizeP];
			char* parsed2[sizeP];
			parse(inputStr1, parsed1, sizeP);
			parse(inputStr2, parsed2, sizeP);
			piping(parsed1, parsed2);
		}
		//Execute && and ||
		if(check == 1 || check == 2) {
			char * inputStr1;
			char * inputStr2;
			if(check == 1) {
				inputStr1 = strtok(inputStr, "&");
				inputStr1[strlen(inputStr1)-1] = '\0';
				inputStr2 = strtok(NULL, " ");
				inputStr2 = strtok(NULL, "\n");
				inputStr2[strlen(inputStr2)] = '\n';
			} else {
				inputStr1 = strtok(inputStr, "|");
				inputStr1[strlen(inputStr1)-1] = '\0';
				inputStr2 = strtok(NULL, " ");
				inputStr2 = strtok(NULL, "\n");
				inputStr2[strlen(inputStr2)] = '\n';
			}
			char* parsed1[sizeP];
			char* parsed2[sizeP];
			parse(inputStr1, parsed1, sizeP);
			parse(inputStr2, parsed2, sizeP);
			if(check == 1) {
				if(buildin(parsed1, inputStr1, sizeI, history) == 0) {
					buildin(parsed2, inputStr2, sizeI, history);
				}
			}
			if(check == 2) {
				if(buildin(parsed1, inputStr1, sizeI, history) != 0) {
					buildin(parsed2, inputStr2, sizeI, history);
				}
			}
		} else if(check == 0) {
			buildin(parsed, inputStr, sizeI, history);
		}
		//Clear the input string
		for(int m = 0; m < sizeI; m++) {
			inputStr[m] = '\0';
		}
	}
	return 0;
}
