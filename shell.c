#include <stdio.h>
#include <stdbool.h> 
#include <signal.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>

#define command_length 128
#define max_tokens (command_length/2 +1) 
#define history_length 5

char *input;

void handle_signal(int);

int min(int a, int b);

bool flag = false;

#define directory_length 256

// Display prompt as [(current_directory)~$ ]
void display_prompt(){
    char current_working_directory[directory_length];
    if(getcwd(current_working_directory,sizeof(current_working_directory)) != NULL){
        printf("%s~$ ", current_working_directory);
    }
}

#define max_num_process 100000

int child_process_history_num = 0;
int child_process_entered[max_num_process];

// display status of all child process entered by the user
void display_ps_history(){
    int j = 0;
    while(j<child_process_history_num){
        printf("%d ", child_process_entered[j]);
        if(kill(child_process_entered[j], 1) < 0){
            printf("STOPPED\n");
        }
        else{
            printf("RUNNING\n");
        }
        j++;
    }
}

// For displaying the Last 5 executed Commands
int cmd_history_num = 0;
char cmd_history[history_length][command_length];

void display_cmd_history(){
    int j = (cmd_history_num - 1)%5;
    int sl = min(5,cmd_history_num);
    while(j > ((cmd_history_num -1)%5 - sl)){
        printf("%s \n", cmd_history[(j+5)%5]);
        --j;
    }
}

struct tokenized_input{
    char *p[command_length];
};

// For breaking the input string into tokens, so as to get it ready for the execvp format 
struct tokenized_input tokenize_command(char *input, char *delimiter){
    int index = 0;
    char *token = strtok(input, delimiter);
    struct tokenized_input params;
    while(token != NULL){
        if(token[0] == '&'){
            ++token;
        }
        params.p[index] = token;
        index++;
        token = strtok(NULL, delimiter);
    }
    params.p[index] = NULL;
    return params;
}

bool read_input(){
    fgets(input, command_length, stdin);
    if(input[0] == '\n'){
        return false;
    }
    input = strsep(&input, "\n");
    return true;
}

// To check if we have to normally execute or pipe
bool normal_exec(char *input){
    for(int i = 0;i<command_length;i++){
        if(input[i] == '\0') return true;
        else if(input[i] == '|') return false;
    }
    return true;
}

// In case of piping, we generate two commands - one before pipe and the other after pipe
void generate_two_commands(char *input, char* commands[command_length]){
    char *before_pipe = strtok(input , "|");
    commands[0] = before_pipe;
    commands[0][strlen(commands[0])-1] = '\0';

    char *after_pipe = strtok(NULL, "|");
    if(after_pipe[0] == ' ' && after_pipe[1] != '\0') ++after_pipe;
    commands[1] = after_pipe;
}

// To check if the given input is for setting environment variables  
bool set_env(char *input){
    int k = strnlen(input,command_length);
    for(int i =  0;i<k;i++){
        if(input[i] == '\0') return false;
        else if(input[i] == '=') return true;
    }
    return false;
}


int main(){
    signal(SIGINT, handle_signal);
    input = malloc(sizeof(char)*command_length);
    struct tokenized_input params;
    char copy_inp[command_length], copy_command1[command_length], copy_command2[command_length];
    
    while(true){
        int _pid;
        while((_pid = waitpid(-1, NULL, WNOHANG))>0){
            continue;
        };
        display_prompt();
        bool fl = read_input();
        if(!fl){
            printf("error...empty command.. \n");
            continue;
        }

        if(normal_exec(input)){
            // Need to make a copy of the input, in order to preserve the original input,
            // since strtok() used breaks the input into tokens
            strcpy(copy_inp,input);
            params = tokenize_command(copy_inp, " "); 
            
            if(set_env(input)){
                strcpy(copy_inp,input);
                params = tokenize_command(copy_inp, "=");
                setenv(params.p[0],params.p[1],1);
                flag = true;
            }
            else{
                pid_t pid = fork();
                if(pid < 0){
                    printf("error in forking.. \n");
                }
                else if(pid == 0){
                    if(strcmp(params.p[0],"cmd_history") == 0){
                        display_cmd_history();
                    }
                    else if(strcmp(params.p[0],"ps_history") == 0){
                        display_ps_history();
                    }
                    else{
                        for(int i = 0; params.p[i] != NULL; i++){
                            if(params.p[i][0] == '$'){
                                char env_token[command_length];
                                strcpy(env_token,params.p[i]);
                                char env_token_temp[command_length];
                                memcpy(env_token_temp, &env_token[1], (command_length-1)*sizeof(char));
                                params.p[i] = getenv(env_token_temp);
                            }
                        }
                        execvp(params.p[0],params.p);
                        printf("error occured.. \n");
                        exit(EXIT_FAILURE);
                    }
                    exit(0);
                }
                else{
                    if(input[0] == '&'){
                        child_process_entered[child_process_history_num] = pid;
                    }
                    else if(input[0] != '&'){
                        // int status_child;
                        child_process_entered[child_process_history_num] = pid;
                        wait(NULL);
                        // waitpid(pid, &status_child,0);
                    }
                }
                flag = false;
            }
            if(!flag){
                ++child_process_history_num;
            }
        }
        else{
            char *commands[command_length];
            struct tokenized_input params_before_pipe, params_after_pipe;

            strcpy(copy_inp,input);
            generate_two_commands(copy_inp, commands);
            
            strcpy(copy_command1,commands[0]);
            strcpy(copy_command2,commands[1]);

            params_before_pipe = tokenize_command(copy_command1, " ");
            params_after_pipe = tokenize_command(copy_command2, " ");


            int fd[2];
            pipe(fd);
            pid_t pid = fork();
            if(pid < 0){
                printf("error in forking... \n");
            }
            else if(pid == 0){
                dup2(fd[1],1);
                for(int i = 0;i<2;i++){
                    close(fd[i]);
                }
                if(strcmp(commands[0],"cmd_history") == 0){
                    display_cmd_history();
                }
                else if(strcmp(commands[0],"ps_history") == 0){
                    display_ps_history();
                }
                else{
                    for(int i = 0; params_before_pipe.p[i] != NULL; i++){
                        if(params_before_pipe.p[i][0] == '$'){
                            char env_token[command_length];
                            strcpy(env_token,params_before_pipe.p[i]);
                            char env_token_temp[command_length];
                            memcpy(env_token_temp, &env_token[1], (command_length-1)*sizeof(char));
                            params_before_pipe.p[i] = getenv(env_token_temp);
                        }
                    }
                    execvp(params_before_pipe.p[0],params_before_pipe.p);
                    printf("error occured.. \n");
                    exit(EXIT_FAILURE);
                }
                exit(0);
            }
            else{
                child_process_entered[child_process_history_num] = pid;
                ++child_process_history_num;
                wait(NULL);
                // waitpid(pid, NULL, 0);

                pid_t pid_sub = fork();
                if(pid_sub < 0){
                    printf("error in forking.. \n");
                }
                else if(pid_sub == 0){
                    dup2(fd[0],0);
                    for(int i = 0;i<2;i++){
                        close(fd[i]);
                    }
                    for(int i = 0; params_after_pipe.p[i] != NULL; i++){
                        if(params_after_pipe.p[i][0] == '$'){
                            char env_token[command_length];
                            strcpy(env_token,params_after_pipe.p[i]);
                            char env_token_temp[command_length];
                            memcpy(env_token_temp, &env_token[1], (command_length-1)*sizeof(char));
                            params_after_pipe.p[i] = getenv(env_token_temp);
                        }
                    }
                    execvp(params_after_pipe.p[0],params_after_pipe.p);
                    printf("error occured.. \n");
                    exit(EXIT_FAILURE);
                }
                else{
                    for(int i = 0;i<2;i++){
                        close(fd[i]);
                    }
                    child_process_entered[child_process_history_num] = pid_sub;
                    ++child_process_history_num;
                    // waitpid(pid, NULL, 0);
                    wait(NULL);
                    wait(NULL);
                    // waitpid(pid_sub, NULL, 0);

                }
            }
        }
        strcpy(cmd_history[cmd_history_num%5],input);
        cmd_history_num++;
    }
}

void handle_signal(int signal){
    exit(0);
}

int min(int a, int b){
    return (a<b)?a:b;
}