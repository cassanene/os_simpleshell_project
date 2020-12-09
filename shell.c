#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <signal.h>
#include <limits.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <setjmp.h>

#define MAX_COMMAND_LINE_LEN 1024
#define MAX_COMMAND_LINE_ARGS 128

char prompt_end[] = "> ";
char delimiters[] = " \t\r\n";
extern char **environ;
struct stat sb;

void echoCmd (char *args[MAX_COMMAND_LINE_ARGS]);
/*
void cpCmd (char* src, char* dest);
void lsCmd ();
*/

char* FGProc (char* process);
void addBGProc (char* process);
void cdCmd (char *args[MAX_COMMAND_LINE_ARGS]);
void envCmd (char *findVar);
void setenvCmd (char *newVar);
void killFG(int sigNum);

static sigjmp_buf jmpBuf;
static volatile sig_atomic_t jump = 0;
void SIGINT_handler(int sig_num);

int main() {
    // Stores the string typed into the command line.
    char command_line[MAX_COMMAND_LINE_LEN];
    char cmd_bak[MAX_COMMAND_LINE_LEN];
    
    //https://stackoverflow.com/a/298518
    char cwd[PATH_MAX];
    if(getcwd(cwd, sizeof(cwd)) == NULL) {
       perror("getcwd() error");
       exit(1);
    }
  
    //https://stackoverflow.com/a/11043336
    char prompt_dir[1024];
  
    // Stores the tokenized command line input.
    char *arguments[MAX_COMMAND_LINE_ARGS];
  
    char *cmd;
  
    pid_t pid;
    int status;
  
    struct stat sb;

    struct sigaction sigAct;
    sigAct.sa_handler = SIGINT_handler;
    sigemptyset(&sigAct.sa_mask);
    sigAct.sa_flags = SA_RESTART;
    sigaction(SIGINT, &sigAct, NULL);
    	
    while (1) {
      
        do{
            
            if (sigsetjmp(jmpBuf, 1) == 42) {
              printf("\n");
              continue;
            }
      
            jump = 1;
            
            strcpy(prompt_dir, getcwd(cwd, sizeof(cwd)));
          
            // Print the shell prompt.
            printf("%s%s", prompt_dir, prompt_end);
            fflush(stdout);

            // Read input from stdin and store it in command_line. If there's an
            // error, exit immediately. (If you want to learn more about this line,
            // you can Google "man fgets")
            
         
            if ((fgets(command_line, MAX_COMMAND_LINE_LEN, stdin) == NULL) && ferror(stdin)) {
                
              fprintf(stderr, "fgets error");
              exit(0);
             
            }
            
            //tokenize: https://stackoverflow.com/questions/28502305/writing-a-simple-shell-in-c-using-fork-execvp
            //helped better understand strtok: https://www.tutorialspoint.com/c_standard_library/c_function_strtok.htm
            int j = 0;
            char* token;
            token = strtok(command_line, " ");
            while (token != NULL) {
    
              arguments[j] = token;
              token = strtok(NULL, " ");
              j++;
            }
            arguments[j] = NULL;
            arguments[j-1] = strtok(arguments[j-1], "\n");
          
            if (j == 1) {
              cmd = strtok(command_line, "\n");
            } else {
              cmd = arguments[0];
            }
          
            for (j = 1; arguments[j] != NULL; j++) {
              if (arguments[j][0] == '$') {
                memmove(arguments[j],arguments[j]+1,strlen(arguments[j]));
                arguments[j] = getenv(arguments[j]);
              }
            }
            
            if (strcmp(cmd, "exit") == 0) { exit(0); }
            else if (strcmp(cmd, "cd") == 0 && arguments[1] != NULL) { cdCmd(arguments); }
            else if (strcmp(cmd, "env") == 0) { envCmd(arguments[1]); }
            else if (strcmp(cmd, "setenv") == 0) { setenvCmd(arguments[1]); }
            else if (stat(cmd, &sb) == 0 && sb.st_mode & S_IXUSR && arguments[1] != NULL) {
              
              //https://stackoverflow.com/a/13098645  ^check if executable
              addBGProc(cmd);
            } 
            else {
              
              pid = fork();
              if (pid == 0) {
                
                //solution to signal handling: https://indradhanush.github.io/blog/writing-a-unix-shell-part-3/
                struct sigaction childAct;
                childAct.sa_handler = SIGINT_handler;
                sigemptyset(&childAct.sa_mask);
                childAct.sa_flags = SA_RESTART;
                sigaction(SIGINT, &childAct, NULL);
                
                signal(SIGALRM,killFG);
                int child_pid = getpid();
                alarm(10);
                
                if(stat(cmd, &sb) == 0 && sb.st_mode & S_IXUSR){
                  strcpy(cmd,FGProc(cmd));
                }
                
                if((execvp(cmd, arguments)) < 0) {
                  perror(cmd);
                  exit(1);
                }
                
              } else {
                
                wait(&status);
              }
            }
            
            /*if (strcmp(cmd, "exit") == 0) {
              
              exit(0);
              
            } else if (strcmp(cmd, "pwd") == 0) {
              
              printf("%s\n", prompt_dir);
              
            } else if (strcmp(cmd, "cd") == 0 && arguments[1] != NULL) {
              
              //https://www.geeksforgeeks.org/chdir-in-c-language-with-examples/
              char* path;
              path = strtok(arguments[1], "\n");
              if(chdir(path) != 0){
                perror("No such directory");
              }
              
              
            } else if (strcmp(cmd, "env") == 0) {
              
              if(arguments[1] == NULL){
                
                //https://stackoverflow.com/a/12059006
                char* curr_env = *environ;
                int a;
                for (a = 1; curr_env; a++) {
                  printf("%s\n", curr_env);
                  curr_env = *(environ + a);
                }
                
              } else {
                
                char* var;
                var = strtok(arguments[1], "\n");
                printf("%s\n", getenv(var));
                
              }
              
              
            } else if (strcmp(cmd, "echo") == 0) {
              
              echoCmd(arguments);
              
            } else if (strcmp(cmd, "setenv") == 0) {
              
              char* key;
              key = strtok(arguments[1], "=");
              char* value;
              value = strtok(NULL, "\n");
              
              setenv(key, value, 1);
              
            } else if (strcmp(cmd, "ls") == 0) {
              
              lsCmd();
              
            } else if (strcmp(cmd, "cp") == 0) {
              
              cpCmd(arguments[1], arguments[2]);
              
            } else if (stat(cmd, &sb) == 0 && sb.st_mode & S_IXUSR) {
              //https://stackoverflow.com/a/13098645  ^check if executable
              
              if(arguments[1] == NULL){
                
                FGProc(cmd);
                
              } else if (strcmp(arguments[1], "&\n") == 0){
                
                addBGProc(cmd);
                
              } else {
                //do nothing
              }
              
            } else {
              
              printf("No such file or directory\n");
            }*/
 
        }while(command_line[0] == 0x0A);  // while just ENTER pressed

      
        // If the user input was EOF (ctrl+d), exit the shell.
        if (feof(stdin)) {
            printf("\n");
            fflush(stdout);
            fflush(stderr);
            return 0;
        }
      
      
        // Hints (put these into Google):
        // man fork
        // man execvp
        // man wait
        // man strtok
        // man environ
        // man signals
        
        // Extra Credit
        // man dup2
        // man open
        // man pipes
    }
  return -1;
}

void cdCmd (char *args[MAX_COMMAND_LINE_ARGS]) {
  
  //https://www.geeksforgeeks.org/chdir-in-c-language-with-examples/
  char* path;
  path = strtok(args[1], "\n");
  if(chdir(path) != 0){
    perror("No such directory.\n");
  }
}

void envCmd (char *findVar) {
  
  if(findVar == NULL){
                
    //https://stackoverflow.com/a/12059006
    char* curr_env = *environ;
    int a;
    for (a = 1; curr_env; a++) {
      printf("%s\n", curr_env);
      curr_env = *(environ + a);
    }
    
  } else {
    
    char* var;
    var = strtok(findVar, "\n");
    printf("%s\n", getenv(var));
              
  }
                   
}

void setenvCmd (char *newVar) {
  
  char* key;
  key = strtok(newVar, "=");
  char* value;
  value = strtok(NULL, "\n");
              
  setenv(key, value, 1);
}

void SIGINT_handler (int sig_num) {
  
  if (!jump) {
    return;
  }
  siglongjmp(jmpBuf, 42);
}

//https://stackoverflow.com/a/35899248
void killFG (int sigNum) {
  kill(getpid(), SIGKILL);
}

/*
void echoEnv (char *env_variable) {
  
  char *cmdEcho = "printenv ";
  
  memmove(env_variable,env_variable+1,strlen(env_variable));
  char *new_env = malloc(strlen(cmdEcho) + strlen(env_variable) + 1);
  strcpy(new_env,cmdEcho);
  strcpy(new_env+strlen(cmdEcho),env_variable);
  
  system(new_env);
  free(new_env);
}
void echoCmd (char *args[MAX_COMMAND_LINE_ARGS]) {
  
  int k;
  if (args[1] == NULL) {
    printf(" ");
  } else {
    
    for (k = 1; k < MAX_COMMAND_LINE_ARGS; k++) {
      if (args[k] == NULL) {
        break;
      } else {
        
        if(k != 1) {
          printf(" ");
        }
        
        if(args[k][0] == '$') {
          //echoEnv(args[k]);
          printf("%s",getenv(args[k]));
        } else {
          printf("%s", args[k]);
        }
      }
    }
  }
  printf("\n");
}
*/
/*
void cpCmd (char* src, char* dest) {
  
  //https://www.geeksforgeeks.org/c-program-copy-contents-one-file-another-file/
  char* src_file;
  src_file = strtok(src, " ");
  
  char* dest_file;
  dest_file = strtok(dest, "\n");
  
  char content;
  
  FILE *fptr1, *fptr2;
              
  fptr1 = fopen(src_file, "r");
  if(fptr1 == NULL){
    printf("cant copy\n");
    exit(0);
  }
  
  fptr2 = fopen(dest_file, "w");
  if(fptr2 == NULL){
    fptr2 = fopen(dest_file, "ab+");
  }
  
  content = fgetc(fptr1);
  while (content != EOF) {
    
    fputc(content, fptr2);
    content = fgetc(fptr1);
  }
  
  fclose(fptr1);
  fclose(fptr2);
  
}
void lsCmd (void) {
  
  //https://www.geeksforgeeks.org/c-program-list-files-sub-directories-directory/
  struct dirent *de;
  DIR *dr = opendir(".");
  
  if(dr == NULL){
    
    printf("is broken");
    
  } else {
    
    while ((de = readdir(dr)) != NULL) {
      printf("%s\n", de->d_name);
    }
    
    closedir(dr);
  }
}*/

char* FGProc (char* process) {
  
  char* exec = "./";
  char* new_exec = malloc(strlen(exec) + strlen(process) + 1);
  
  strcpy(new_exec, exec);
  strcpy(new_exec + strlen(exec), process);
  
  system(new_exec);
  
  return new_exec;
}

void addBGProc (char* process) {
  
  //https://stackoverflow.com/a/8821378
  char* exec = "./";
  char* bgRun = "&";
  char* new_exec = malloc(strlen(exec) + strlen(process) + strlen(bgRun) + 1);
  
  strcpy(new_exec, exec);
  strcpy(new_exec + strlen(exec), process);
  strcpy(new_exec + strlen(exec) + strlen(process), bgRun);
  
  //https://stackoverflow.com/a/26817195
  //run executable
  system(new_exec);
}
