/* Compile with: g++ -Wall –Werror -o myshell myshell.c */
#include <sys/wait.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

//constant variables
const int TOKEN_BUFFER_SIZE=32;
const char *TOK_DELIM=" \t\r\n\a";
const int CMD_BUFFER_SIZE=1024;

//global variables
static char* args[CMD_BUFFER_SIZE];
char *cmdBuffer;
char *cmdExe[CMD_BUFFER_SIZE];
int pipeCount;
char cwd[CMD_BUFFER_SIZE];
pid_t pid;
char *promptLine=nullptr;

void clearVar()
{
    cmdBuffer=nullptr;
    pipeCount = 0;
    cwd[0]='\0';
    pid=0;
}

void loadConfig()
{
    return;
}


void outputPrompt()
{
    char shell[1000];
    char cwd[1000];
    if (getcwd(cwd, sizeof(cwd)) != NULL)
    {
        strcpy(shell, "Msh:");
        strcat(shell, cwd);
        strcat(shell, "$ ");
        
        printf("%s", shell);
    }
    else
        perror("getcwd() error");
}

char *shReadLine()
{
    char *line=nullptr;
    size_t bufsize=0;
    getline(&line, &bufsize,stdin);
    char *p=line;
    while(*p)
    {
        if(*p=='\n')
        {
            *p='\0';
            break;
        }
        p++;
    }
    return line;
}

char **tokenizeLine(char* line)
{
    size_t bufsize = TOKEN_BUFFER_SIZE, position=0;
    char **tokens = (char **)malloc(bufsize * sizeof(char*));
    char *token;
    
    if(!tokens)
    {
        fprintf(stderr, "sh: allocation error\n");
        exit(EXIT_FAILURE);
    }
    
    token = strtok(line, TOK_DELIM);
    
    while(token!=nullptr)
    {
        tokens[position] = token;
        position++;
        
        if(position>=bufsize)
        {
            bufsize *= 2;
            tokens = (char **)realloc(tokens, bufsize * sizeof(char*));
            if(!tokens)
            {
                fprintf(stderr, "sh: allocation error\n");
                exit(EXIT_FAILURE);
            }
        }
        
        token = strtok(NULL,TOK_DELIM);
    }
    
    tokens[position]=nullptr;
    return tokens;
}

char **processDirectives(char **args)
{
    return args;
}


int runCommand(char **args)
{
    pid_t pid, wpid;
    int status;
    
    pid = fork();
    if (pid == 0)
    {
        if (execvp(args[0], args) == -1)
        {
            perror("lsh");
        }
        exit(EXIT_FAILURE);
    }
    else if (pid < 0)
    {
        perror("lsh");
    }
    else
    {
        do
        {
            wpid = waitpid(pid, &status, WUNTRACED);
        } while (!WIFEXITED(status) && !WIFSIGNALED(status));
    }
    
    return 1;
}

char* skipcomma(char* str)
{
    int i=0, j=0;
    char temp[1000];
    while(str[i++]!='\0')
    {
        if(str[i-1]!='"')
            temp[j++]=str[i-1];
    }
    temp[j]='\0';
    str = strdup(temp);
    
    return str;
}

static int command(int input, int first, int last, char *cmd_exec)
{
    int mypipefd[2], ret, input_fd, output_fd;
    ret = pipe(mypipefd);
    if(ret == -1)
    {
        perror("pipe");
        return 1;
    }
    pid = fork();
    
    if (pid == 0)
    {
        if (first==1 && last==0 && input==0)
        {
            dup2( mypipefd[1], 1 );
        }
        else if (first==0 && last==0 && input!=0)
        {
            dup2(input, 0);
            dup2(mypipefd[1], 1);
        }
        else
        {
            dup2(input, 0);
        }
        
        //printf("%s",args[0]);
        
        if(execvp(args[0], args)<0) printf("%s: command not found\n", args[0]);
        exit(0);
    }
    else
    {
        waitpid(pid, 0, 0);
    }
    
    if (last == 1)
        close(mypipefd[0]);
    if (input != 0)
        close(input);
    close(mypipefd[1]);
    return mypipefd[0];
    
}


void changeDirectory()
{
    char *h="/home";
    if(args[1]==NULL)
        chdir(h);
    else if ((strcmp(args[1], "~")==0) || (strcmp(args[1], "~/")==0))
        chdir(h);
    else if(chdir(args[1])<0)
        printf("bash: cd: %s: No such file or directory\n", args[1]);
}

void presentDirectory()
{
    if (getcwd(cwd, sizeof(cwd)) != NULL)
    {
        printf("%s\n", cwd );
    }
    else
        perror("getcwd() error");
}

int split(char *cmdExec, int input, int first, int last)
{
    char *newCmd;
    newCmd=strdup(cmdExec);
    {
        int m=1;
        args[0]=strtok(cmdExec," ");
        while((args[m]=strtok(NULL," "))!=NULL)
            m++;
        args[m]=NULL;
        if (args[0] != NULL)
        {
            
            if (strcmp(args[0], "exit") == 0)
                exit(0);
            if (strcmp(args[0], "echo") != 0)
            {
                cmdExec = skipcomma(newCmd);
                int m=1;
                args[0]=strtok(cmdExec," ");
                while((args[m]=strtok(NULL," "))!=NULL)
                    m++;
                args[m]=NULL;
                
            }
            if(strcmp("cd",args[0])==0)
            {
                changeDirectory();
                return 1;
            }
            else if(strcmp("pwd",args[0])==0)
            {
                presentDirectory();
                return 1;
            }
            
        }
    }
    return command(input, first, last, newCmd);
}

void pipeExec()
{
    int i, n=1, input, first;
    
    input=0;
    first= 1;
    
    cmdExe[0]=strtok(cmdBuffer,"|");
    
    while ((cmdExe[n]=strtok(NULL,"|"))!=NULL)
        n++;
    cmdExe[n]=NULL;
    pipeCount=n-1;
    
    for(i=0; i<n-1; i++)
    {
        input = split(cmdExe[i], input, first, 0);
        first=0;
    }
    input=split(cmdExe[i], input, first, 1);
    input=0;
    return;
}

void shellLoop()
{
    //char **unprocessedArgs;
    char ch[2]="\n";
    //char **args;
    int status=1;
    
    
    do
    {
        clearVar();
        outputPrompt();
        cmdBuffer = shReadLine();
        if(strcmp(cmdBuffer,ch)==0)
            continue;
        if(strcmp(cmdBuffer,"exit")==0)
        {
            exit(0);
            return;
        }
        //unprocessedArgs = tokenizeLine(cmdBuffer);
        //args = processDirectives(unprocessedArgs);
        //status = runCommand(unprocessedArgs);
        pipeExec();
        free(cmdBuffer);
        
        //free(unprocessedArgs);
        //free(args);
        
    }while(status);
}


int main()
{
    //load configuration file
    loadConfig();
    
    //run until end of file
    shellLoop();
    
    //exit
    return EXIT_SUCCESS;
}
