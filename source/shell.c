#include "shell.h"

#include <stdio.h>
#include <stddef.h>
#include <dirent.h>

#include "net.h"
#include "transfer.h"
#include "address_dbg.h"

#define STRINGIFY2(X) #X
#define STRINGIFY(X) STRINGIFY2(X)

struct Command {
	char *type;
	char **args;
	int argLen;
};

static inline void InitCommandStruct(struct Command *cmd) {
	cmd->type = NULL;
	cmd->args = NULL;
	cmd->argLen = 0;
}

static char *ParseNextArg(int *i, char *rawCmd) {
	while (rawCmd[*i] == ' ') { // skip whitespace
		++*i;
	}
	char *cmd = NULL;
	int len = 0;
	bool inQuotes = false;
	while ((rawCmd[*i] != ' ' || inQuotes) && rawCmd[*i] != '\n' && rawCmd[*i] != '\0') { // get non whitepace
		if (rawCmd[*i] == '"') {
			inQuotes = !inQuotes;
			if (!inQuotes) {
				++*i;
				continue;
			} else
				++*i;
		}
		cmd = (cmd == NULL) ? malloc(++len) : realloc(cmd, ++len);
		cmd[len - 1] = rawCmd[*i];
		
		++*i;
	}
	cmd = (cmd == NULL) ? NULL : realloc(cmd, ++len);
	if (cmd != NULL)
		cmd[len - 1] = '\0';
	return cmd;
}

#define MAX(a, b) (((a) > (b)) ? a : b)

static void GetNextCommand(struct Command *cmd) {
	char curChr;
	int n = 0;
	char *rawCmd = malloc(0);
#ifdef _WIN32
	while ((curChr = getchar()) != '\r' || (curChr = getchar()) != '\n') {
#else
	while ((curChr = getchar()) != '\n') {
#endif
		rawCmd = realloc(rawCmd, ++n);
		rawCmd[n - 1] = curChr;
	}
	if (n == 0) {
		cmd->type = NULL;
		goto nextCommandExit;
	}
	rawCmd = realloc(rawCmd, ++n);
	rawCmd[n - 1] = '\0';
	
	int i = 0;
	
	cmd->type = ParseNextArg(&i, rawCmd);
	cmd->argLen = 0;
	cmd->args = malloc(0);
	
	char *nextArg = NULL;
	while ((nextArg = ParseNextArg(&i, rawCmd)) != NULL) {
		cmd->args = realloc(cmd->args, (++cmd->argLen)*sizeof(void*));
		cmd->args[cmd->argLen - 1] = nextArg;
	}
nextCommandExit:
	free(rawCmd);
}

static inline void freeCommandBuffer(struct Command  *cmd) {
	free(cmd->type);
	cmd->type = NULL;
	for (int i = 0; i < cmd->argLen; i++) {
		free(cmd->args[i]);
	}
	free(cmd->args);
	cmd->args = NULL;
	cmd->argLen = 0;
}

static char *helpText = "\
This shell is blocks sockets requests. Use it carefully!\n\
Commands:\n\
help: Get command list\n\
exit: Exit shell\n\
cd: Change current working directory\n\
rcd: Change current working directory (remote). Make sure you have a connection!\n\
cwd: Display current working directory\n\
nts: Display/Change number of current transfering sockets (to change, add an argument, max "STRINGIFY(MAX_NTS)" nts)\n\
ls: List the current directory\n\
";

#define REQUIRE_NUM_ARGS(x) \
do {\
	if (command.argLen != x) {\
		printf("Invalid command. Requires %d %s. %d %s given!\n", x, (x == 1) ? "argument" : "arguments", command.argLen, (command.argLen == 1) ? "argument" : "arguments");\
		goto shellOnError;\
	}\
} while(0)

int secretProgress = 0;

void RunShell(void) {
	printf("Welcome to the secret command shell. For more info type help\n");
	
	struct Command command;
	
	InitCommandStruct(&command);
	
	while (true) {
		printf("> ");
		GetNextCommand(&command);
		
		if (command.type == NULL)
			continue;
		
		if (strcmp(command.type, "exit") == 0) {
			puts("Exiting command shell...");
			break;
		} else if (strcmp(command.type, "help") == 0) {
			printf(helpText);
		} else if (strcmp(command.type, "cd") == 0) {
			REQUIRE_NUM_ARGS(1);
			if (chdir(command.args[0]) == -1)
				puts("Directory does not exist.");
		} else if (strcmp(command.type, "cwd") == 0) {
			char *cwd = getcwd(NULL, 0);
			printf("%s\n", cwd);
			free(cwd);
		} else if (strcmp(command.type, "nts") == 0) {
			if (command.argLen == 1) {
				if (atoi(command.args[0]) > MAX_NTS || atoi(command.args[0]) < 1) {
					puts("Invalid transfer socket number property value.");
					goto shellOnError;
				}
				SetConfigNTS(atoi(command.args[0]));
			} else if (command.argLen == 0) {
				printf("%d\n", GetConfigNTS());
			} else {
				puts("Invalid nts command");
			}
		} else if (strcmp(command.type, "rcd") == 0) {
			REQUIRE_NUM_ARGS(1);
			ChangeDirectoryRemote(command.args[0]);
			puts("Successfully changed directories.");
		} else if (strcmp(command.type, "ls") == 0) {
			system("ls");
		} else if (strcmp(command.type, "mew") == 0) {
			puts("🤫🧏");
		} else if (strcmp(command.type, "mog") == 0) {
			puts("😠😠😠 You have angered the sigma...");
		} else if (strcmp(command.type, "fard") == 0) {
			puts("Ok you're not funny lol");
		} else if (strcmp(command.type, "password") == 0) {
			puts("Password is incorrect");
			secretProgress = 1;
		} else if (strcmp(command.type, "incorrect") == 0) {
			if (secretProgress != 1)
				goto unknownCommand;
			puts("Please try again");
			secretProgress = 2;
		} else if (strcmp(command.type, "again") == 0) {
			if (secretProgress == 2 && command.argLen == 0) {
				puts("Please try again later");
				secretProgress = 3;
			} else if (secretProgress == 3 && command.argLen == 1) {
				secretProgress = 0;
				if (strcmp(command.args[0], "later") == 0) {
					puts("You looked at the code didn't you. i hope you're happy with yourself");
				} else {
					goto unknownCommand;
				}
			} else {
				goto unknownCommand;
			}
		} else {
unknownCommand:
			printf("Unkown command: \"%s\"\n", command.type);
		}
shellOnError:
		freeCommandBuffer(&command);
	}
}