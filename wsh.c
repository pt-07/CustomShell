#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <ctype.h>
#define MAX_ARGS 64

int max_commands = 5;
#define MAX_COMMAND_LENGTH 100
#define MAX_PATH 100


// A simple structure to hold a shell variable
typedef struct ShellVar {
    char *name;
    char *value;
    struct ShellVar *next;
} ShellVar;

// The head of the linked list of shell variables
ShellVar *shellVars = NULL;
char *trim(char *str) {
    while (isspace((unsigned char)*str)) str++;

    if (*str == 0)  // All spaces?
        return str;

    char *end = str + strlen(str) - 1;
    while (end > str && isspace((unsigned char)*end)) end--;

    // Write new null terminator character
    end[1] = '\0';

    return str;
}
// A function to set a shell variable
void setShellVar(char *name, char *value) {
    // Check if the variable already exists
    ///trim the value
    value = trim(value);
    //trim the name
    name = trim(name);
    ShellVar *var = shellVars;
    ShellVar *prev = NULL;
    while (var != NULL) {
        if (strcmp(var->name, name) == 0) {
            if (value != NULL && *value != '\0' && strcmp(value, " ") != 0) {
                // If the variable exists and the new value is not empty, update its value
                free(var->value);
                var->value = strdup(value);
            } else {
                // If the new value is empty, remove the variable from the list
                if (prev != NULL) {
                    prev->next = var->next;
                } else {
                    shellVars = var->next;
                }
                free(var->name);
                free(var->value);
                free(var);
            }
            return;
        }
        prev = var;
        var = var->next;
    }

    // If the variable doesn't exist and the value is not empty, add it to the end of the list
    if (value != NULL && *value != '\0' && strcmp(value, " ") != 0) {
        var = malloc(sizeof(ShellVar));
        var->name = strdup(name);
        var->value = strdup(value);
        var->next = NULL;

        if (shellVars == NULL) {
            // If the list is empty, set the new variable as the first element
            shellVars = var;
        } else {
            // If the list is not empty, find the last element
            ShellVar *lastVar = shellVars;
            while (lastVar->next != NULL) {
                lastVar = lastVar->next;
            }
            // Add the new variable to the end of the list
            lastVar->next = var;
        }
    }
}

// A function to get a shell variable
char *getShellVar(char *name) {
    // If that fails, get the shell variable
    // Find the variable in the list
    ShellVar *var = shellVars;
    while (var != NULL) {
        if (strcmp(var->name, name) == 0) {
            // If the variable exists, return its value
            return var->value;
        }
        var = var->next;
    }

    // If the variable doesn't exist, return an empty string
    return "";
}
char* replaceShellVars(char* token) {
    // Try to replace the token with an environment variable
    char *dollar = strchr(token, '$');
    char *var = dollar + 1;
    char *value = getenv(var);

    // If the environment variable is not set or empty, try to replace with a local variable
    //printf("value: %s\n", value);
    if (value == NULL || value[0] == '\0') {
        value = getShellVar(var);
        //printf("value: %s\n", value);

        if (value == NULL || value[0] == '\0') {
            // If local variable also fails, return the original token
            return "";
        }
    }
    if(value== NULL || value[0] == '\0') {
        //printf("token: %s\n", token);
        return "";
    }
    return value;
}
// A function to print all shell variables
void printShellVars() {
    ShellVar *var = shellVars;
    while (var != NULL) {
        printf("%s=%s\n", var->name, var->value);
        var = var->next;
    }
}

typedef struct Node {
    char command[MAX_COMMAND_LENGTH];
    struct Node* next;
} Node;

Node* head = NULL;
int count = 0;


void pipemode(char* command) {
    //need to loop through each command one by one, using dup2() to execute and redirect outptu to the next command
    char *args[MAX_ARGS];
    char *commands[MAX_COMMAND_LENGTH];
    int n = 0;

    // Split the command into parts around the pipe symbol
    char *token = strtok(command, "|");
    while (token != NULL) {
        // Trim the command
        token = trim(token);
        commands[n] = token;
        n++;
        token = strtok(NULL, "|");
    }

    int fd[n][2];
    for (int i = 0; i < n; i++) {
        if (pipe(fd[i]) == -1) {
            perror("pipe");
            exit(EXIT_FAILURE);
        }
    }

    for (int i = 0; i < n; i++) {
        char *arg = strtok(commands[i], " ");
        int j = 0;
        while (arg != NULL) {
            args[j] = arg;
            j++;
            arg = strtok(NULL, " ");

        }
        args[j] = NULL;

        int pid = fork();
        if (pid == 0){
            //child process
            if (i > 0){
                //error?
                dup2(fd[i-1][0], 0);
            }
            if(i < n-1){
                dup2(fd[i][1], 1);
            }
            for (int k = 0; k < n; k++) {
                close(fd[k][0]);
                close(fd[k][1]);
            }
            if (execvp(args[0], args) == -1) {
                perror("execvp");
                exit(EXIT_FAILURE);
            }
        }
        else if (pid < 0) {
            perror("fork");
            exit(EXIT_FAILURE);
        }

    }
    for (int k = 0; k < n; k++) {
            close(fd[k][0]);
            close(fd[k][1]);
    }
    for (int k = 0; k < n; k++) {
        wait(NULL);
    }
}

void set_max_commands(int n) {
    max_commands = n;
    //need to remove the newer commands that are larger than n
    //if the size is 0 clear the list
    if(n == 0) {
        Node* current = head;
        Node* prev = NULL;
        while (current != NULL) {
            prev = current;
            current = current->next;
            free(prev);
        }
        head = NULL;
        count = 0;
        return;
    }
    while (count > max_commands) {
        Node* current = head;
        Node* prev = NULL;
        while (current->next != NULL) {
            prev = current;
            current = current->next;
        }
        if (prev != NULL) {
            prev->next = NULL;
        } else {
            head = NULL;
        }
        free(current);
        count--;
    }
}
char* get_command(int n) {
    Node* current = head;
    int i = 1;
    while (current != NULL && i < n) {
        current = current->next;
        i++;
    }
    if (current != NULL) {
        return current->command;
    } else {
        return NULL;
    }
}
void add_command(char** command) {
    Node* node = (Node*) malloc(sizeof(Node));
    strncpy(node->command, *command, MAX_COMMAND_LENGTH);
    node->next = head;
    head = node;
    count++;

    if (count > max_commands) {
        Node* current = head;
        Node* prev = NULL;
        int i = 1;
        while (current->next != NULL && i <= max_commands) {
            prev = current;
            current = current->next;
            i++;
        }
        prev->next = NULL;
        free(current);
        count--;
    }
}

void print_commands() {
    Node* current = head;
    int i = 1;
    while (current != NULL) {
        printf("%d) %s\n", i, current->command);
        current = current->next;
        i++;
    }
}
//define a linked list to store the commands

void execute_command(char *cmd) {
    char *args[MAX_ARGS];
    char *token = strsep(&cmd, " ");
    int i = 0;

    while (token != NULL) {
        args[i] = token;
        args[i] = trim(args[i]);
        i++;
        if (cmd == NULL) {
            break;
        }
        else {
            token = strsep(&cmd, " ");
        }
    }

    args[i] = NULL;

    if (fork() == 0) {
        execvp(args[0], args);
        perror("execvp");
        exit(1);
    } else {
        wait(NULL);
    }
}

void is_builtin_command(char *cmd) {
    if (strcmp(cmd, "exit") == 0 ||
        strcmp(cmd, "cd") == 0 ||
        strcmp(cmd, "export") == 0 ||
        strcmp(cmd, "local") == 0 ||
        strcmp(cmd, "vars") == 0 ||
        strcmp(cmd, "history") == 0) {
        //do nothing
    } else {
        add_command(&cmd);
    }
}
void interactive_mode(FILE *batchfile) {
    char *cmd = NULL;
    size_t cmd_len = 0;

    while (1) {
        if(batchfile == NULL){
            printf("wsh> ");
            getline(&cmd, &cmd_len, stdin);
        }
        else{
            if(getline(&cmd, &cmd_len, batchfile) == -1) {
                break;
            }

        }
        //Need to check for end of file
        if(cmd == NULL) {
            break;
        }
        if(feof(stdin)) {
            break;
        }
        //take out the new line character from the command
        if (cmd[strlen(cmd) - 1] == '\n') {
            cmd[strlen(cmd) - 1] = '\0';
        }
        //check if the command has a $, if it does then get the variable name and replace it with the value
        //tokenize the command and check if the first character is a $, if it is then replace it with the value
        //then scan each argument and replace
        //tokenize the commands
        //check if the command contains a $ and has more than one argument
        if(strchr(cmd, '$') != NULL){
            char *token = strtok(cmd, " ");
        char *newcmd = calloc(cmd_len, sizeof(char));
        while(token != NULL) {
            //check if the token has a $, if it does then replace it with the value
           //check if the token begins with a $, if it does then replace it with the value
            if(token[0] == '$') {
                char *value = replaceShellVars(token);
                if(value == NULL || value[0] == '\0') {
                    token = strtok(NULL, " ");
                    continue;
                }
                newcmd = strcat(newcmd, value);
                newcmd = strcat(newcmd, " ");
            }
            else {
                newcmd = strcat(newcmd, token);
                newcmd = strcat(newcmd, " ");
            }
            token = strtok(NULL, " ");
        }
        newcmd = trim(newcmd);

        cmd = newcmd;
        //printf("cmd: %s\n", cmd);

        }

        //check the commands for pipes
        if (strchr(cmd, '|') != NULL) {
            pipemode(cmd);
            continue;
        }
        else if (strncmp(cmd, "exit", 4) == 0) {
            free(cmd);
            break;
        }
        else if (strncmp(cmd, "cd", 2) == 0) {
            char *dir = cmd + 2;
            while (*dir == ' ' || *dir == '\t') dir++; // Skip whitespace characters
      
            // Trim newline character at the end if present

            // Change directory
            if (chdir(dir) != 0) {
                perror("chdir failed");
            }
            continue;
        }
        else if (strncmp(cmd, "history set", 11) == 0) {
            int n = atoi(cmd + 11);
            set_max_commands(n);
            continue;
        }
        else if (strncmp(cmd, "history ", 8) == 0) {
            int n = atoi(cmd + 8);
            char* command = get_command(n);
            if (command != NULL) {
                // Execute the command without adding it to the history
                execute_command(command);
            }
            continue;
        }
        else if(strncmp(cmd, "history", 7) == 0) {
            print_commands();
            continue;
        }
        else if (strncmp(cmd, "vars",4) == 0) {
            printShellVars();
            continue;
        }
        else if (strncmp(cmd, "local ", 6) == 0) {
            char *var = strtok(cmd + 6, "=");  // Skip the "local " part
            char *value = strtok(NULL, "=");
            if (value == NULL) {
                value = "";  // If no value is provided, use an empty string
            }
            setShellVar(var, value);
            continue;
        }
        else if (strncmp(cmd, "export", 6) == 0) {
            char *var = strtok(cmd + 7, "=");  // Skip the "export " part
            char *value = strtok(NULL, "=");
            if (value == NULL || *value == '\0') {
                unsetenv(var);
            }
            else if (setenv(var, value, 1) != 0) {
                perror("setenv failed");
            }
            continue;
        }
        else{
            //cmd[strlen(cmd) - 1] = '\0';  // Remove newline character
            if(max_commands > 0) {
                is_builtin_command(cmd);
            }
            //printf("cmd: %s\n", cmd);
            execute_command(cmd);
            continue;
        }
    }
}

void batch_mode(char *filename) {
    FILE *file = fopen(filename, "r");
    char *cmd = NULL;

    if (file == NULL) {
        perror("fopen failed");
        exit(1);
    }

    while (fgets(cmd, sizeof(cmd), file) != NULL) {
        cmd[strlen(cmd) - 1] = '\0';  // Remove newline character
        execute_command(cmd);
    }

    fclose(file);
}

int main(int argc, char *argv[]) {
    //need to trim the spaces and tabs from the beginning and end
    FILE *batchfile = NULL;
    if (argc == 1) {
        interactive_mode(batchfile);
    } else if (argc == 2) {
        batchfile = fopen(argv[1], "r");
        if(batchfile == NULL) {
            perror("fopen failed");
            return 1;
        }
        interactive_mode(batchfile);
    } else {
        fprintf(stderr, "Usage: %s [script.wsh]\n", argv[0]);
        return 1;
    }

    //close the batchfile
    return 0;
