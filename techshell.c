// Name(s): Austin J Brown
// Description: A simple shell that can execute commands, change directories, and redirect input and output.
// It handles errors, and it supports quoted arguments and escaped characters.

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <sys/wait.h>
#include <errno.h>
#include <fcntl.h>

char **parseInput(char *input, bool *errorStatus);
void executeCommand(char **command);
char *tokenize(char *input, int *lastChar, bool *errorStatus);
void freeCommands(char **command);

int main() // MAIN
{		
	// repeatedly prompt the user for input
	for (;;)
	{
        char cwd[1024]; 
        char inputString[1024];
        char* input = inputString;
        bool ERROR_STATUS = false;

        // print the current directory
        getcwd(cwd, sizeof(cwd));
        printf("%s$ ", cwd);

        // get the user input
        fgets(inputString, sizeof(inputString), stdin);
        if (inputString[0] == '\n'){  // don't continue if the user didn't type anything
            continue;
        }
	    
	    // parse the command line
	    char **command = parseInput(input, &ERROR_STATUS);
        // check if an error occured during parsing
        if (ERROR_STATUS || command == NULL || command[0] == NULL){
            freeCommands(command);
            continue;
        }
	    
        // check if the command was to exit the program
        if (strcmp(command[0], "exit") == 0){
            freeCommands(command);
            exit(0);
        }

	    // execute the command
	    executeCommand(command);
	}
}

char **parseInput(char *input, bool *errorStatus){

    // replace the newline character with \0
    input[strcspn(input, "\n")] = '\0';  

    // allocate space for the array of strings
    int MAX_ARGS = 32;
    char **args = calloc(MAX_ARGS, sizeof(char *));

    int i = 0;  // this will be used to access each space of args (max 32)
    int lastChar = 0;  // used to keep track of the last char checked when tokenizing
    char *token = tokenize(input, &lastChar, errorStatus);  // create the first token 

    // add each token to an array args
    while (i < MAX_ARGS - 1 && token != NULL){

        args[i] = token;
        token = tokenize(input, &lastChar, errorStatus);
        i++;

    }

    // check if any errors occured after tokenization
    if (token != NULL){  
        printf("Error: too many arguments\n");
        *errorStatus = true;
        free(token);
        freeCommands(args);
        return NULL;
    } else{
        args[i] = NULL;  // last arg needs to be null for execvp
        return args;
    }
}

void executeCommand(char **command){
    
    // check for a change directory command
    if (strcmp(command[0], "cd") == 0){
        if (command[1] == NULL){
            fprintf(stderr, "Usage: cd <path>\n");
            freeCommands(command);
            return;
        }
        if (chdir(command[1]) != 0){ // change directories and check if it worked
            fprintf(stderr, "Error %d (%s)\n", errno, strerror(errno));
            freeCommands(command);
            return;
        } else{
            freeCommands(command);
            return;
        }
    }

    char *input = NULL;
    char *output = NULL;

    // scan for redirects
    int i = 0;
    int newIndex = 0;
    char *actualCommand[16];
    while (command[i] != NULL){
        if (strcmp(command[i], "<") == 0){  // check for input
            if (i == 0){  // if the user wrote " < file.txt"
                fprintf(stderr, "Error %d (%s)\n", errno, strerror(errno));
                freeCommands(command);
                return;
            } else if (command[i+1] == NULL){  // if the user wrote "cmd < "
                fprintf(stderr, "Error %d (%s)\n", errno, strerror(errno));
                freeCommands(command);
                return;
            }
            input = command[i+1];  // store the input file
            i += 2;
        } else if (strcmp(command[i], ">") == 0){  // check for output
            if (i == 0){  // if the user wrote " > file.txt"
                fprintf(stderr, "Error %d (%s)\n", errno, strerror(errno));
                freeCommands(command);
                return;
            } else if (command[i+1] == NULL){  // if the user wrote "file1.txt > "
                fprintf(stderr, "Error %d (%s)\n", errno, strerror(errno));
                freeCommands(command);
                return;
            }
            output = command[i+1];  // store the output file
            i += 2;
        } else{
            actualCommand[newIndex++] = command[i++];
        }
    }

    // last command has to be NULL
    actualCommand[newIndex] = NULL;

    // set command equal to the actual commands
    for (int i = 0; i <= newIndex; i++){
        command[i] = actualCommand[i];
    }

    // get it done
    pid_t pid = fork();
    if (pid < 0){
        fprintf(stderr, "Error %d (%s)\n", errno, strerror(errno));
        exit(3);
    }
    if (pid == 0){
        // child
        if (input != NULL){
            int fd = open(input, O_RDONLY);  // open the input file
            if (fd < 0){
                fprintf(stderr, "Error %d (%s)\n", errno, strerror(errno));
                exit(4);
            }
            dup2(fd, STDIN_FILENO);  // redirect standard in to the file
            close(fd);
        }
        if (output != NULL){
            int fd = open(output, O_WRONLY | O_CREAT | O_TRUNC, 0644);
            if (fd < 0){
                fprintf(stderr, "Error %d (%s)\n", errno, strerror(errno));
                exit(4);
            }
            dup2(fd, STDOUT_FILENO);  // redirect standard out to the file
            close(fd);
        }

        // execute the command
        execvp(command[0], command);
        fprintf(stderr, "Error %d (%s)\n", errno, strerror(errno));
        exit(1);
    } else{
        // parent 
        wait(NULL);
        freeCommands(command);
    }
}

char *tokenize(char *input, int *lastChar, bool *errorStatus){
    bool inQuotes = false;
    char *result = calloc(1024, sizeof(char));  // this will get returned at the end
    int i = 0;
    while (true){  
        if (input[*lastChar] == ' '){  // take away leading spaces
            (*lastChar)++;
        } else{break;}
    }
    while (i < 1024){  // main loop
        if (input[*lastChar] == '\0'){
            if (i == 0){  // if the last token has already been made
                free(result);
                return NULL;
            } else{
                break;
            }
        }else if (input[*lastChar] == '"'){  // check for quotes
            if (inQuotes){
                inQuotes = false;
                (*lastChar)++;
            } else{
                inQuotes = true;
                (*lastChar)++;
            }
        }else if (input[*lastChar] == '\\'){  // if a backslash was typed
            if (input[(*lastChar)+1] != '\0'){  // checking to make sure the backslash is not the last thing typed
                result[i] = input[(*lastChar)+1];
                (*lastChar) += 2;
                i++;
            } else{
                printf("We do not use line continuation in this house >:(\n");
                free(result);
                *errorStatus = true;
                return NULL;
            }
        }else if (input[*lastChar] == ' '){  // checking spaces
            if (inQuotes){
                result[i] = input[*lastChar];
                (*lastChar)++;
                i++;
            } else{  // else stop tokenizing
                (*lastChar)++;
                break;
            }
        } else{  // if nothing else, append the char to the resulting token
            result[i] = input[*lastChar];
            (*lastChar)++;
            i++;
        }
    }
    if (inQuotes){  // if an odd number of quotes were written
        printf("failed to close quotation\n");
        *errorStatus = true;
        free(result);
        return NULL;
    }
    result[i] = '\0';
    return result;
}

// function to empty the command array
void freeCommands(char **command){
    if (command != NULL){
        for (int i = 0; i < 32; i++){
            free(command[i]);
        }
        free(command);
    }
}