#include <string.h>
#include <stdbool.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <errno.h>
#include "linked_list.h"
#include <ctype.h>
#include <limits.h> 

Node* head = NULL;

/************ Helper Functions *************/

// Function to convert a relative path to an absolute path
char *makeFullPath(const char *cmd) {
    char *full_path = NULL;
    char cwd[1024];

    // Already absolute path
    if (cmd[0] == '/') {
        return strdup(cmd);
    } 
    else {
        if (getcwd(cwd, sizeof(cwd)) != NULL) {
            // Concatenate current working directory with the command
            // Adding 2 for the '/' and null terminator
            int len = strlen(cwd) + strlen(cmd) + 2;
            full_path = malloc(len);

            if (full_path == NULL) {
                perror("malloc failed");
                exit(EXIT_FAILURE);
            }
            // Use snprintf to prevent buffer overflow
            snprintf(full_path, len, "%s/%s", cwd, cmd);
        } 
        else {
            perror("getcwd() error");
            return NULL;
        }
    }
    return full_path;
}

/*  Checks if a PID is valid
    Returns 1 if valid otherwise 0
 */
int is_valid_pid(const char *str_pid) {
    char *endpidptr;
    long pid;
    
    // Check if str_pid is NULL or empty
    if (str_pid == NULL || str_pid[0] == '\0') {
        printf("Input is invalid: NULL or empty string\n");
        return 0;
    }
    
    // Check if str_pid is all numeric
    for (int i = 0; str_pid[i] != '\0'; i++) {
        if (!isdigit(str_pid[i])) {
            printf("Input is invalid: Non-numeric characters found\n");
            return 0;
        }
    }
    
    // Converting str_pid to a long
    // Track if the conversion is successfull or not
    errno = 0;
    pid = strtol(str_pid, &endpidptr, 10);
    
    // Check for conversion errors
    if ((errno == ERANGE && (pid == LONG_MAX || pid == LONG_MIN)) 
        || (errno != 0 && pid == 0)) {
        perror("strtol");
        return 0;
    }
    
    /* Check in case no conversion happened as not digits found
       so they both will be pointing at the same thing
     */
    if (endpidptr == str_pid) {
        printf("Input is invalid: No digits found\n");
        return 0;
    }
    
    // Check for any trailing non-numeric characters
    if (*endpidptr != '\0') {
        printf("Input is invalid: Trailing non-numeric characters\n");
        return 0;
    }
    
    // Check pid is within a valid range
    if (pid <= 0 || pid >= INT_MAX) {
        printf("Input is invalid: Process ID out of valid range\n");
        return 0;
    }
    // Valid PID
    return 1;
}

/* Function to monitor any changes made to the
   background processes.
   Example: Any process killed outside the pman
 */
void check_background_jobs() {
    Node *current = head;
    Node *prev = NULL;

    // Traversing the linked list while checking every PID
    while (current != NULL) {
        int p_status;
        pid_t result = waitpid(current->pid, &p_status, WNOHANG);
        
        // Child process is still running
        if (result == 0) {
            prev = current;
            current = current->next;
        }
        // If an error occurs
        else if (result == -1) {
            perror("waitpid: An error is occured");
            prev = current;
            current = current->next;
        }
        else {
            // Child process has terminated or exits
            if (WIFSIGNALED(p_status)) {
                printf("Process %d was killed\n", current->pid);
            }
            if (WIFEXITED(p_status)) {
                printf("Process %d exits\n", current->pid);
            }
            
            // Remove the node from the PID list
            if (prev == NULL) {
                head = current->next;
                free(current->path);
                free(current);
                current = head;
            }
            else {
                prev->next = current->next;
                free(current->path);
                free(current);
                current = prev->next;
            }
        }
    }
}

// Function to get the state of the background processes
char get_p_state(char * str_pid) {
    char proc_stat_path[PATH_MAX];
    char file_buffer[256];
    FILE *file;
    char state = ' ';

    pid_t pid = atoi(str_pid);

    // Construct path for process information
    snprintf(proc_stat_path, sizeof(proc_stat_path), "/proc/%d/stat", pid);

    // Read the stat file for state
    file = fopen(proc_stat_path, "r");
    if (file == NULL) {
        perror("fopen failed");
        return '0';
    }

    // Read contents of stat file
    if (fgets(file_buffer, sizeof(file_buffer), file) == NULL) {
        perror("fgets failed");
        fclose(file);
        return '0';
    }
    fclose(file);

    char *tokens[52];
    int tokenIndex = 0;

    char *token = strtok(file_buffer, " ");
    while (token != NULL && tokenIndex < 52) {
        tokens[tokenIndex++] = token;
        token = strtok(NULL, " ");
    }

    if (tokenIndex > 2) {
        state = tokens[2][0];
    }

    return state;
}

/*****************************************/

/*
    Function to start a background process
    Example as per main() func: bg foo
    `foo` is just an placeholder for
    any executable file.
 */
void func_BG(char **cmd) {
  	// Checks if nothing is passed as command or executable
    if (cmd[1] == NULL) {
        printf("Invalid input -> %s for executable\n",cmd[1]);
        return;
    }
    char *full_path = NULL;

    // Check if the command is already an absolute path
    if (cmd[1][0] == '/') {
        full_path = strdup(cmd[1]);
    } 
    else {
        // Check if the command starts with "./" which indicates a relative path from the current directory
        if (strncmp(cmd[1], "./", 2) == 0) {
            full_path = makeFullPath(cmd[1] + 2);
        }
        else {
            // If just the name from the same directory is passed or by traversing a dir
            full_path = makeFullPath(cmd[1]);
        }
    }

    if (full_path == NULL) {
        fprintf(stderr, "Error making full path for %s\n", cmd[1]);
        return;
    }

    // Check if the file exists and is executable
    if (access(full_path, X_OK) != 0) {
        if (errno == ENOENT) {
            fprintf(stderr, "Executable file %s not found\n", full_path);
        
        } 
        else {
            fprintf(stderr, "Executable file %s is not executable\n", full_path);
        }
        free(full_path);
        return;
    }

    // After validation, creating a child process
    pid_t pid = fork();

    // Fork operation failed
    if (pid < 0) {
        perror("fork failed");
        free(full_path);
        return;
    } 
    // Current process is child
    else if (pid == 0) {
        // Adding current directory to PATH just to be safe
        char *path = ".";
        setenv("PATH", path, 1);

        execvp(full_path, cmd + 1);
        // If execvp fails
        perror("execvp failed");
        exit(EXIT_FAILURE);
    }
    // Current process is parent
    else {
        // Adding the process to the list of PID
        head = add_newNode(head, pid, full_path);
        printf("Process with PID %d started in background\n", pid);
    }

    free(full_path);
}

/*
    Function to list all the background processes
    Example as per main() func: bglist
 */
void func_BGlist(char **cmd) {
	// Check the background process first in case killed outside terminal
    check_background_jobs();

    Node *current = head;
    
    // Check if the user input is right
    if (cmd[1] != NULL) {
        printf("bglist takes no arguments\n");
        return;
    }

    int count = 0;

    // In case no background jobs
    if (current == NULL) {
        printf("No background jobs\n");
        return;
    }

    // Printing the jobs as well tracking the count
    while (current != NULL) {
        printf("%d: %s\n", current->pid, current->path);
        count++;
        current = current->next;
    }
    printf("Total background jobs: %d\n", count);
}

/*
    Function to kill a background process
    Example as per main() func: bgkill {pid}
    where `pid` is the process ID for process
    to be killed.
 */
void func_BGkill(char * str_pid) {
	// Check first if the PID is valid
    if (is_valid_pid(str_pid)) {
        // Converting the PID to pid_t
        pid_t pid_for_p_kill = (pid_t) strtol(str_pid, NULL, 10);

        // Kill the process
        if (kill(pid_for_p_kill, SIGKILL) != 0) {
            perror("kill the process is failed");
            return;
        }

        // Wait for the process to terminate
        int p_status;
        pid_t result = waitpid(pid_for_p_kill, &p_status, 0);

        if (result == -1) {
            perror("waitpid: An error is occured");
        } else {
            printf("Process with PID %d has been killed\n", pid_for_p_kill);
            head = deleteNode(head, pid_for_p_kill);
        }
    } else {
        printf("PID %s is not valid\n", str_pid);
    }

}

/*
    Function to stop a background process
    Example as per main() func: bgstop {pid}
    where `pid` is the process ID for process
    to be stopped.
 */
void func_BGstop(char * str_pid) {
	// Check first if the PID is valid
    if (is_valid_pid(str_pid)) {
        pid_t pid_for_p_stop = (pid_t) strtol(str_pid, NULL, 10);

        // Check if the process exists and in the list
        if(get_p_state(str_pid) != 'T') {
            if(PifExist(head, pid_for_p_stop)) {
                kill(pid_for_p_stop, SIGSTOP);
                printf("PID %s has been stopped \n", str_pid);
            }
        }
        else {
            printf("PID %s is already in stopped state \n", str_pid);
        }
    }
    else {
        printf("PID %s is not valid\n", str_pid);
    }
}

/*
    Function to start a background process
    Example as per main() func: bgstart {pid}
    where `pid` is the process ID for process
    to be started.
 */
void func_BGstart(char * str_pid) {
	// Check first if the PID is valid
    if (is_valid_pid(str_pid)) {
        pid_t pid_for_p_start = (pid_t) strtol(str_pid, NULL, 10);

        // Check if the process exists and in the list
        if(get_p_state(str_pid) == 'T') {
            if(PifExist(head, pid_for_p_start)) {
                kill(pid_for_p_start, SIGCONT);
                printf("PID %s has been started from stopped state \n", str_pid);
            }
        }
        else {
            printf("PID %s was not in stopped state \n", str_pid);
        }
    }
    else {
        printf("PID %s is not valid\n", str_pid);
    }
}

/*
    Function to print stats for a background process
    Example as per main() func: pstat {pid}
    where `pid` is the process ID for process.
 */
void func_pstat(char * str_pid) {
	// Check first if the PID is valid
    if (is_valid_pid(str_pid)) {
        FILE *file;
        char proc_stat_path[4096];
        char proc_status_path[4096];
        char file_buffer[512];
        char comm[512];
        char state;
        long rss;
        unsigned long utime_clock_ticks, stime_clock_ticks;
        unsigned long voluntary_ctxt_switches, nonvoluntary_ctxt_switches;

        long clock_ticks_per_second = sysconf(_SC_CLK_TCK);
        pid_t pid = atoi(str_pid);

        if(PifExist(head, pid) == 0) {
            printf("Process is not in the list\n");
            return;
        }

        // Construct paths for extract process stats
        snprintf(proc_stat_path, sizeof(proc_stat_path), "/proc/%d/stat", pid);
        snprintf(proc_status_path, sizeof(proc_status_path), "/proc/%d/status", pid);

        // Read the stat file for comm, state, utime, stime and rss
        file = fopen(proc_stat_path, "r");
        if (file == NULL) {
            perror("fopen failed");
            return;
        }

        // Read contents of stat file
        if (fgets(file_buffer, sizeof(file_buffer), file) == NULL) {
            perror("fgets failed");
            fclose(file);
            return;
        }
        fclose(file);

        // As we have this a single line file with space separated value, so we tokenize
        char *tokens[52];
        int tokenIndex = 0;

        char *token = strtok(file_buffer, " ");
        while (token != NULL && tokenIndex < 52) {
            tokens[tokenIndex++] = token;
            token = strtok(NULL, " ");
        }
        strcpy(comm, tokens[1]);
        state = tokens[2][0];
        utime_clock_ticks = strtoul(tokens[13], NULL, 10);
        stime_clock_ticks = strtoul(tokens[14], NULL, 10);
        rss = strtol(tokens[23], NULL, 10);

        // Read the status file for voluntary ctxt switches and nonvoluntary ctxt switches
        file = fopen(proc_status_path, "r");
        if (file == NULL) {
            perror("fopen failed");
            return;
        }

        char key[128], value[300];

        // Read the status file
        // Since this is multi-line with key value pairs
        while (fgets(file_buffer, sizeof(file_buffer), file)) {
            if (sscanf(file_buffer, "%127s %299s", key, value) != 2) {
                continue;
            }
            if (strcmp(key, "voluntary_ctxt_switches:") == 0) {
                sscanf(value, "%ld", &voluntary_ctxt_switches);
                continue;
            }
            if (strcmp(key, "nonvoluntary_ctxt_switches:") == 0) {
                sscanf(value, "%ld", &nonvoluntary_ctxt_switches);
                continue;
            }
        }
        fclose(file);

        // Clock ticks to seconds
        double utime_seconds = (double) utime_clock_ticks / clock_ticks_per_second;
        double stime_seconds = (double) stime_clock_ticks / clock_ticks_per_second;

        printf("<<--- Process %d (PID: %d) Stats--->>\n", pid, pid);
        printf("     %-30s: {%s}\n", "comm", comm);
        printf("     %-30s: %c\n", "state", state);
        printf("     %-30s: %.2f s\n", "utime", utime_seconds);
        printf("     %-30s: %.2f s\n", "stime", stime_seconds);
        printf("     %-30s: %ld pages\n", "rss", rss);
        printf("     %-30s: %lu\n", "voluntary context switches", voluntary_ctxt_switches);
        printf("     %-30s: %lu\n", "nonvoluntary context switches", nonvoluntary_ctxt_switches);
    }
    else {
        printf("PID %s is not valid\n", str_pid);
    }
}

 
int main() {
    char user_input_str[50];
    char user_input_for_error[50];
    while (true) {
        // Check background process every time prompted for input
        check_background_jobs();
        printf("Pman: > ");
        fgets(user_input_str, 50, stdin);
        strcpy(user_input_for_error, user_input_str);
        //printf("User input: %s \n", user_input_str);
        char * ptr = strtok(user_input_str, " \n");
        if(ptr == NULL) {
            continue;
        }
        char * lst[50];
        int index = 0;
        lst[index] = ptr;
        index++;
        while(ptr != NULL) {
            ptr = strtok(NULL, " \n");
            lst[index]=ptr;
            index++;
        }
        if (strcmp("bg",lst[0]) == 0) {
            func_BG(lst);
        }
        else if (strcmp("bglist",lst[0]) == 0) {
            func_BGlist(lst);
        } 
        else if (strcmp("bgkill",lst[0]) == 0) {
            func_BGkill(lst[1]);
        }
        else if (strcmp("bgstop",lst[0]) == 0) {
            func_BGstop(lst[1]);
        }
        else if (strcmp("bgstart",lst[0]) == 0) {
            func_BGstart(lst[1]);
        }
        else if (strcmp("pstat",lst[0]) == 0) {
            func_pstat(lst[1]);
        }
        else if (strcmp("q",lst[0]) == 0) {
            printf("Bye Bye \n");
            exit(0);
        }
        // Invalid or unknown command
        else {
            user_input_for_error[strcspn(user_input_for_error, "\n")] = '\0';
            printf("%s: command not found \n", user_input_for_error);
        }
    }
    return 0;
}

