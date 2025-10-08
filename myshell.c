#include <stdio.h>
#include <string.h>
#include <stdlib.h>			// exit()
#include <unistd.h>			// fork(), getpid(), exec()
#include <sys/wait.h>		// wait()
#include <signal.h>			// signal()
#include <fcntl.h>			// close(), open()

#define COMMAND_LENGTH_MAXIMUM 1000
#define MAX_ARGUMNETS 50

// golbal variables 
char *commands[MAX_ARGUMNETS];
char *args[MAX_ARGUMNETS];
int command_count = 0;
int redirect_flag = 0;
char *output_file = NULL;

void signal_handler(int sig);
void parseInput(char *input_str, const char *delimiter);
void parseCommandArgs(char *command_str);
void executeCommand(char *command_str);
void executeParallelCommands(void);
void executeSequentialCommands(void);
void executeCommandRedirection(char *input_str);
void executePipedCommands(void);

int main()
{
	// Initial declarations
	
	// signal() sets up the signal handler
	signal(SIGCHLD, signal_handler); // registers the handler for SIGCHLD to prevent zombie processes
    signal(SIGINT, SIG_IGN);       // ignores SIGINT (Ctrl+C) to prevent the shell from terminating
    signal(SIGTSTP, SIG_IGN);      // ignores SIGTSTP (Ctrl+Z) to prevent the shell from stopping
	
	while(1)	// This loop will keep your shell running until user exits.
	{
		// Print the prompt in format - currentWorkingDirectory$
		char current_wd[COMMAND_LENGTH_MAXIMUM];
		getcwd(current_wd,sizeof(current_wd));
		printf("%s$",current_wd);
		
		// accept input with 'getline()'
		char *command=NULL;
		size_t len=0;
		getline(&command,&len,stdin);

		//  to remove the trailing newline character from the input
        command[strcspn(command,"\n")]=0;

        // will check for empty command,if there then skip
        if (strlen(command)==0) 
        {
            free(command);
            continue;
        }
		
		// when user uses exit command then to exit from shell
		if(strcmp(command,"exit")==0)	
		{
			printf("Exiting shell...\n");
			free(command);
			break;
		}

        // check for parallel commands
        // this function is invoked when user wants to run multiple commands in parallel (commands separated by &&)
		if(strstr(command,"&&")) 
        {
            parseInput(command,"&&");
			executeParallelCommands();
		}
        // Check for sequential commands
        // this function is invoked when user wants to run multiple commands sequentially (commands separated by ##)
        else if(strstr(command,"##")) 
        {
            parseInput(command,"##");
			executeSequentialCommands();
		} 
        // check for output redirection
        // this function is invoked when user wants redirect output of a single command to and output file specificed by user
        else if(strstr(command,">")) 
        {
			executeCommandRedirection(command);
		} 
        // check for command pipelines
        else if (strstr(command,"|")) 
        {
            if (command[strlen(command)-1]=='|') 
            {
                // check for a trailing pipe without a command
                printf("Shell: Incorrect command\n");
            } 
            else 
            {
                parseInput(command,"|");
                executePipedCommands();
            }
        }
        // single command
        // this function is invoked when user wants to run a single commands
        else 
        {
			executeCommand(command);
        }

        free(command);
	}
	
	return 0;
}

// this is the signal handler for SIGCHLD, done to prevent zombie processes
void signal_handler(int sig) 
{
    int status;
    // use waitpid with WNOHANG to reap all terminated child processes without blocking
    while (waitpid(-1,&status,WNOHANG)>0);
}

// This function will parse the input string into multiple commands or a single command with arguments depending on the delimiter (&&, ##, or spaces)
void parseInput(char *input_str,const char *delimiter)
{
    // reseting global variables for each new command to avoid leftover data from the previous command
    command_count=0;
    
    char *token_ptr;
    char *rest_ptr=input_str;

    // strsep is used to extract tokens(commands) from the input_str based on the delimiter
    while ((token_ptr=strsep(&rest_ptr,delimiter))!=NULL) 
    {
        // skip empty tokens resulting from multiple delimiters (e.g., `cmd1 ## ## cmd2`)
        if (strlen(token_ptr)>0) 
        {
            // This loop trims leading spaces from the start of the command token
            while (*token_ptr==' ') 
            {
                token_ptr++;
            }
            if (strlen(token_ptr)>0) 
            {
                commands[command_count]=token_ptr;//stores the command string in global commands array
                command_count++;//increament the command count
            }
        }
    }
    commands[command_count]=NULL; // adds a NULL terminator to the end of the commands array because it is required for execvp to work properly
}

// this function parses a single command string into its executable and arguments
void parseCommandArgs(char *command_str)
{
    int arg_count=0;
    char *token_ptr;
    char *rest_ptr=command_str;

    // strsep is used here to split the command string by spaces
    while ((token_ptr=strsep(&rest_ptr," "))!=NULL) 
    {
        // to skip empty tokens
        if (strlen(token_ptr)>0) 
        {
            args[arg_count]=token_ptr;// stores argument in the global args array
            arg_count++;//increament the command count
        }
    }
    args[arg_count] = NULL; // Adds a NULL terminator to the end of the args array, a requirement for execvp
}

// This function will fork a new process to execute a command
void executeCommand(char *command_str)
{
    // Reset redirection flags for each command
    redirect_flag=0;
    output_file=NULL;//to handle ouput redirection
    
    // Checks if the command string contains the output redirection symbol '>'
    char *redirect_check=strstr(command_str,">");

    if (redirect_check) 
    {
        redirect_flag=1;//redirection is active

        // strtok is used to split the string at '>' to split the command and the filename
        char *command_part=strtok(command_str,">");
        char *file_part=strtok(NULL,"");
        if (file_part) 
        {
            // this block trim leading and trailing spaces from the filename

            while (*file_part==' ') 
            {
                file_part++;
            }
            size_t len=strlen(file_part);
            while (len>0 && file_part[len-1]==' ') 
            {
                file_part[--len]='\0';
            }
            output_file=file_part;// stores the cleaned filename
        }
        command_str=command_part;//update as to conatin only command part
    }

    // parse the command string to get arguments
    parseCommandArgs(command_str);

    //If args[0] is NULL after parsing, it means there was no command, just a redirection symbol
    if (args[0]==NULL) 
    {
        return;
    }

    // The `cd` command is a built-in shell command so it must be executed by the parent process
    // because a child process cannot change the parent's working directory
    if (strcmp(args[0],"cd")==0) 
    {
        if (args[1]==NULL || strcmp(args[1],"~")==0) 
        {
            // changes the directory to the user's home directory
            chdir(getenv("HOME"));
        } 
        else 
        {
            // here chdir attempts to change the current working directory
            if (chdir(args[1])!=0) 
            {
                perror("cd"); // if chdir fails then error printing
            }
        }
        return;//as cd is executed
    }

    // `fork()` creates a new child process to execute the command
    pid_t pid_process=fork();

    if (pid_process<0) 
    {   //fork failed
        perror("fork failed");
        exit(1);
    } 
    else if (pid_process==0) 
    {
        // child process
        
        // handle output redirection.
        if (redirect_flag && output_file!=NULL) 
        {
            // open creates or opens the file for writing
            int fd=open(output_file,O_WRONLY | O_CREAT | O_TRUNC,0644);
            if (fd<0) 
            {
                perror("open failed");
                exit(1);
            }
            // dup2 redirects standard output to the file descriptor fd
            dup2(fd,STDOUT_FILENO);
            close(fd);//descriptor is no longer needed
        }

        // execvp replaces the current child process with the new command
        execvp(args[0],args);

        // if execvp returns, it means the command failed
        printf("Shell: Incorrect command");
        printf("\n");
        exit(1);
    } 
    else 
    {
        // in parent process
        // variable to hold the child's exit status
        int status;
        // waitpid waits for the child process identified by `pid` to finish
        waitpid(pid_process,&status,0);
    }
}

// This function will run multiple commands in parallel
void executeParallelCommands()
{
    //  array to store the process IDs of all child process
    pid_t pids[command_count];
    int i;

    // this loop will fork and execute each command in the commands array
    for (i=0;i<command_count;i++) 
    {
        pid_t pid=fork();//forks for each command

        if (pid<0) 
        {
            perror("fork failed");
            exit(1);
        } 
        else if (pid==0) 
        {
            //  in child process
            parseCommandArgs(commands[i]);

            // if  the command is 'cd', handle it directly
            if (args[0] && strcmp(args[0],"cd")==0) 
            {
                if (args[1]==NULL || strcmp(args[1],"~")==0) 
                {
                    chdir(getenv("HOME"));
                } 
                else if (chdir(args[1])!=0) 
                {
                    perror("cd");
                }
                exit(0); // cd is handled, so the child can now exit
            } 
            else 
            {
                execvp(args[0],args);
                // if execvp returns, it means the command failed.
                fprintf(stderr,"Shell: Incorrect command");
                exit(1);
            }
        } 
        else 
        {
            // in parent process
            pids[i]=pid;// Stores the child PID number
        }
    }

    // the parent process waits for all the child processes to finish
    for (i=0;i<command_count;i++) 
    {
        int status;
        waitpid(pids[i],&status,0);
    }
}

// This function will run multiple commands sequentially
void executeSequentialCommands()
{
    int i;
    //  loop that calls `executeCommand` for each command in the commands array
    for (i=0;i<command_count;i++) 
    {
        executeCommand(commands[i]);
    }
}

// This function will run a single command with output redirected to an output file specificed by user
void executeCommandRedirection(char *input_str)
{
    // calls the main executeCommand function, which already has the logic to handle output redirection.
    executeCommand(input_str);
}

// This function handles commands chained with pipes
void executePipedCommands()
{
    int pipes[2]; // array to store file descriptors for the read and write ends of the pipe
    pid_t pid_process;
    int input_fd=0; // standard input file descriptor 

    for (int i=0;i<command_count;i++) 
    {
        // pipe() creates a new pipe with a read and a write end
        pipe(pipes);

        pid_process=fork();
        if (pid_process<0) {
            perror("fork failed");
            exit(1);
        }

        if (pid_process==0) 
        {   // in child process
            //not the first one
            if (i>0) 
            {   // dup2() duplicates the file descriptor input_fd to STDIN_FILENO
                // this then redirects the output from the previous command to the current command input
                dup2(input_fd,STDIN_FILENO);
                close(input_fd); // Close the old input fd
            }
            //not the last one
            if (i<command_count-1) 
            {
                // dup2() duplicates the write end of the current pipe (pipes[1]) to STDOUT_FILENO
                // this then redirects the current command's output to the next command's input
                dup2(pipes[1],STDOUT_FILENO);
                close(pipes[1]); // close the write end of the pipe
            }
            close(pipes[0]); // child doesn't need the read end of the pipe

            parseCommandArgs(commands[i]);
            execvp(args[0],args);
            // If execvp returns, the command failed
            printf("Shell: Incorrect command\n");
            exit(1);
        } 
        else 
        {    // in parent process
            close(pipes[1]); // parent doesn't need the write end
            input_fd=pipes[0]; // read end becomes the input for the next command
            waitpid(pid_process,NULL,0); // Wait for the child to finish
        }
    }
    // after the loop, the parent closes the final pipe read end to clean up resources
    close(input_fd);
}