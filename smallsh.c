#define _GNU_SOURCE
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stddef.h>
#include <stdint.h>
#include <fcntl.h>

#define arrlen(a) (sizeof(a)/sizeof *(a))

int exitStatus = 0;
char bgPid[12] = "";

void handle_SIGINT(int signo) {
}

//str search and replace function 
char *str_gsub(char *restrict *restrict haystack, char const *restrict needle, char const *restrict sub){
  char *str = *haystack;
  size_t haystack_len = strlen(str);
  size_t const needle_len = strlen(needle),
               sub_len = strlen(sub);
  
  for (; (str = strstr(str, needle));) {
    ptrdiff_t off = str - *haystack; 
    if (sub_len > needle_len) {
      str = realloc(*haystack, sizeof **haystack * (haystack_len + sub_len - needle_len + 1));
      if (!str) goto exit; 
      *haystack = str;
      str = *haystack + off;

    }

    memmove(str + sub_len, str + needle_len, haystack_len + 1 - off - needle_len); 
    memcpy(str, sub, sub_len); 
    haystack_len = haystack_len + sub_len - needle_len; 
    str += sub_len; 
  }
  str = *haystack; 
  if (sub_len < needle_len) {
    str = realloc(*haystack, sizeof **haystack * (haystack_len + 1));
    if (!str) goto exit;
    *haystack = str; 
  }
  exit: 
  return str;
}

//parses command line input into semantic tokens 
int getWords(char *input, char *words[]){
    int i = 0;
    char *delim = getenv("IFS");
    if (delim == NULL){
      delim = " \t\n";
    }

    char *token = strtok(input, delim);
    if (token == NULL) {
      return -1; 
    }
    words[i] = strdup(token);
    i++;

    while(token != NULL) {
      //printf("%s\n", tokens[i]);
      token = strtok(NULL, delim);
      if (token != NULL){
      words[i] = strdup(token);}
      i++;
    }
    return i-1;
}

// main shell fn
void execute(char *args[], int nArgs, struct sigaction old_sigint, struct sigaction old_sigtstp){

  // built-in cd
  if (args[0] == NULL) goto exit;

  if (strcmp(args[0], "cd") == 0) {
      if (nArgs > 2) {
        fprintf(stderr, "%s", "Error: too many arguments\n");
        goto exit; 
      }
       if (nArgs == 1) {
        chdir(getenv("HOME"));
        goto exit; 
       }
      else {
        if (chdir(args[1]) != 0){
          perror("Error");
          goto exit; 
          } 
      }
      goto exit;
   }
  
  // built-in exit
  if (strcmp(args[0], "exit") == 0) {
    if (nArgs > 2) {
      fprintf(stderr, "%s", "Error: too many arguments");
      goto exit; 
  }
   if (nArgs == 1) {
     fprintf(stderr, "%s", "\nexit\n");
     exit(exitStatus);
     goto exit;
   } else {
      fprintf(stderr, "%s", "\nexit\n");
      int exitValue = atoi(args[1]); 
      exit(exitValue);
      goto exit;
   }
  } 
   else {
    int bg = 0;
    int outFlag = 0;
    int inFlag = 0;
    char *output;
    char *input;
    int i = 0;
    while (i < nArgs) {
      if (strcmp(args[i], "#")== 0){
        args[i] = NULL; 
        nArgs = i; 
        break; 
      }
      i++;
    }
    i = nArgs - 1;
    if (strcmp(args[i], "&") == 0) {
      bg = 1;
      args[i] = NULL;
      nArgs = i;
    }
   if (nArgs < 3) {
    goto exc;
   }
   i = nArgs - 1; 
   if (strcmp(args[i - 1], ">") == 0 || strcmp(args[i-1], "<") == 0) {
      if (strcmp(args[i - 1], ">") == 0) {
         // output file
         outFlag = 1;
         output = strdup(args[i]);
         args[i-1] = NULL; 
         nArgs = i-1; 
      } else {
        inFlag = 1;
        input = strdup(args[i]);
        args[i-1] = NULL;
        nArgs = i-1; 
      }
   }
   if (nArgs < 3) {
      goto exc;
   }
   i = nArgs - 1; 
   if (strcmp(args[i - 1], ">") == 0 || strcmp(args[i-1], "<") == 0) {
      if (strcmp(args[i - 1], ">") == 0) {
         // output file
         if (outFlag == 0) {
         outFlag = 1;
         output = strdup(args[i]);
         args[i-1] = NULL; 
         nArgs = i-1;
        }
      } else {
        if (inFlag == 0) {
        inFlag = 1;
        input = strdup(args[i]);
        args[i-1] = NULL;
        nArgs = i-1; 
       }
      }
   } 

exc:
   for (int i = 0; i < 0; i++) {
   }
   pid_t spawnpid = -5; 
   int childStatus;
   int childPid;
   char spidStr[12]; 
   args[nArgs] = NULL;
   int outputFD;
   int inputFD;

   spawnpid = fork();
   switch(spawnpid) {
   case -1:
     perror("fork() failed");
     goto exit;

   case 0:
    //redirection 
    if (outFlag == 1) {
      outputFD = open(output, O_WRONLY | O_CREAT | O_TRUNC, 0777);
      if (outputFD == - 1) {
        perror("outfile open()");
        exit(1);
      }
      int res = dup2(outputFD, 1);
      if (res == -1) {
        perror("output dup2");
        exit(1);
      }
    }
    if (inFlag == 1) {
      inputFD = open(input, O_RDONLY);
      if (inputFD == -1) {
        perror("infile open()");
        exit(1);
      }
      int res = dup2(inputFD, 0);
      if (res == -1) {
        perror("input dup2");
        exit(1);
      }
    }
    sigaction(SIGINT, &old_sigint, NULL);
    sigaction(SIGTSTP, &old_sigtstp, NULL);
    if (execvp(args[0], args) == -1) {
    if (outFlag == 1) {
      free(output);
      close(outputFD);
    }
    if (inFlag == 1) {
      free(input);
      close(inputFD);
    }
    perror("exec fail");
    exit(1);
    }
    if(outFlag == 1) {
      free(output);
      close(outputFD);
    }
    if (inFlag == 1) {
      free(input);
      close(inputFD);
    }
    break;

   default:
    if (bg == 0) {
      childPid = waitpid(spawnpid, &childStatus, 0);
      if (WIFEXITED(childStatus)) {
        exitStatus = WEXITSTATUS(childStatus);
       }
      if (WIFSIGNALED(childStatus)) {
        exitStatus = 128 + WTERMSIG(childStatus);
      }
    } else { 
      sprintf(spidStr, "%jd", (intmax_t) spawnpid); 
      strcpy(bgPid, spidStr); 
    }
    break;
   }
   }
  
  
exit:
   return; 
  }

int main(){
  char *line = NULL;
  size_t n = 1024;
  char *words[512];
  int numWords;
  char *home = strcat(getenv("HOME"), "/");
  char pidStr[12];
  char exitStatusStr[4];
  int bgStatus;
  int bgChild;

  struct sigaction SIGINT_action = {0}, ignore_action = {0}, SIGINT_oldact = {0}, SIGTSTP_oldact = {0}; 

  ignore_action.sa_handler = SIG_IGN;
  ignore_action.sa_flags = 0;
  sigaction(SIGTSTP, &ignore_action, &SIGTSTP_oldact);
  
  SIGINT_action.sa_handler = handle_SIGINT;
  sigfillset(&SIGINT_action.sa_mask);
  SIGINT_action.sa_flags = 0;
  sigaction(SIGINT, &ignore_action, &SIGINT_oldact);

  for (;;){
start:
    sigaction(SIGTSTP, &ignore_action, NULL);

    while((bgChild = waitpid(0, &bgStatus, WUNTRACED | WNOHANG)) > 0){
      if (WIFEXITED(bgStatus)) {
        fprintf(stderr, "Child process %jd done. Exit status %d.\n", (intmax_t) bgChild, WEXITSTATUS(bgStatus));
      }
      if (WIFSIGNALED(bgStatus)) {
        fprintf(stderr, "Child process %jd done. Signaled %d.\n", (intmax_t) bgChild, WTERMSIG(bgStatus)); 
      }
      if (WIFSTOPPED(bgStatus)) {
        kill(bgChild, SIGCONT);
        fprintf(stderr, "Child process %jd stopped. Continuing.\n", (intmax_t) bgChild);
      }
    }
    if (getenv("PS1") == NULL) {
      fprintf(stderr, "%s", "");
    } else {
      fprintf(stderr, "%s", getenv("PS1"));
    }
    sigaction(SIGINT, &SIGINT_action, NULL);
    ssize_t linelength = getline(&line, &n, stdin);
    if (feof(stdin) != 0) {
      exit(exitStatus);
    }
    if (linelength == -1) {
     fprintf(stderr, "\n");
     clearerr(stdin);
     errno = 0;
     goto start;
    }
    sigaction(SIGINT, &ignore_action, NULL);
    numWords = getWords(line, words); 
    if (numWords < 0) {
      continue; 
    }
    sprintf(pidStr, "%d", getpid());
    sprintf(exitStatusStr, "%d", exitStatus);
     
    // expansion 
    for (int i = 0; i < numWords; i++) {
      if (strcmp(words[i], "~/") == 0 || (strcmp(&words[i][0], "~") && strcmp(&words[i][1], "/"))){
       words[i] = str_gsub(&words[i], "~/", home);
      }
      words[i] = str_gsub(&words[i], "$$", pidStr);
      
      words[i] = str_gsub(&words[i], "$!", bgPid);
      
      words[i] = str_gsub(&words[i], "$?", exitStatusStr);
    }

    // parse 
    execute(words, numWords, SIGINT_oldact, SIGTSTP_oldact);
    for (int i = 0; i < numWords; i++) {
      free(words[i]);
    }
    
  }
}

