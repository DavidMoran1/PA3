/*
 * David Moran
 * CS 3377.0W1
 * Spring 2018
 * Simple Custom Shell
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <iostream>
#include <fstream>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <cerrno>

using namespace std;


// Declare a constant that will be the maximum number of arguments
const int MAX_ARGS = 256;

// Create an enumerator that we will use as the return type of the parsing function
enum PipeOrRedirect {PIPE, REDIRECT, NEITHER, MULTIPLE};

// Function Headers
// Parses user's input into either multiple commands or a command and a filename
PipeOrRedirect parseInput(int, char**, char**, char**);

// Pipes two commands together
void pipeCommand(char**, char**);

// Reads input from the user and returns the number of arguments entered
int readArgs(char**);

// Redirects the output of the command into the given file
void redirectCommand(char**, char**);

// Takes a number of arguments and an array of arguments and will execute those arguments
void runCommand(int, char**);

// Determines if the user wants to quit the program
bool checkQuit(string);


// Takes user input until they enter 'quit' or 'exit' to exit the shell
int main(){
	// Declare variables
	char* argv[MAX_ARGS], *cmd1[MAX_ARGS], *cmd2[MAX_ARGS];
	PipeOrRedirect pipeRedirect;
	int argc;

	// Keep returning to prompt until user enters 'quit' or 'exit' (without quotes)
	while(true){
		// Display prompt
		cout << "David's Shell> ";

		// Read in commands from user
		argc = readArgs(argv);

		// Parse the command into multiple commands if necessary and set pipeRedirect to the return value
		// to determine if we need to either pipe or redirect
		pipeRedirect = parseInput(argc, argv, cmd1, cmd2);

		// Determine how to handle the input
		// Piping
		if(pipeRedirect == PIPE){
			pipeCommand(cmd1, cmd2);
		}
		// Redirecting
		else if(pipeRedirect == REDIRECT){
			redirectCommand(cmd1, cmd2);
		}
		// Multiple commands separeted by ';'
		else if(pipeRedirect == MULTIPLE){
			// Run the two commands sequentially
			runCommand(1, cmd1);
			runCommand(1, cmd2);
		}
		// Neither
		else{
			runCommand(argc, argv);
		}
	}
}

// Given a number of arguments (argc) and a list of arguments (argv), goes through the list of
// arguments and will split the input into multiple commands if there is piping, redirection, or multiple
// commands on one line separated by ';'. It will return whether or not there is piping or redirection or neither
PipeOrRedirect parseInput(int argc, char** argv, char** cmd1, char** cmd2){
	// Assume that there will not be any piping or redirection
	PipeOrRedirect result = NEITHER;

	// Holds the index in argv where the pipe or redirect was found
	int split = -1;

	// Loop through the arguments
	for(int i = 0; i < argc; i++){
		// Tests to see if a pipe was found
		if(strcmp(argv[i], "|") == 0){
			// Set the result to show that we need to pipe the commands
			result = PIPE;
			// Store the index where we found the piping character
			split = i;
		}
		// Tests to see if a pipe was found and there is already a pipe
		else if((strcmp(argv[i], ";") == 0)){
			// The result will already be PIPE so we don't need to change it again
			result = MULTIPLE;
			// Store the index where the found the second piping character
			split = i;
		}
		// Tests to see if a redirection was found
		else if((strcmp(argv[i], "<") == 0) || (strcmp(argv[i], ">") == 0) || (strcmp(argv[i], ">>") == 0)){
			// Set the result to show that we need to redirect the commands
			result = REDIRECT;
			// Store the index where we found the redirection character
			split = i;
		}
	}

	// If a pipe or redirect was found
	if(result != NEITHER){
		// Loop through the array up to the point where the pipe or redirect was found and
		// store the arguments in cmd1
		for(int i = 0; i < split; i++){
			cmd1[i] = argv[i];
		}
		int count = 0;
		for(int i = split + 1; i < argc; i++){
			cmd2[count] = argv[i];
			count++;
		}
		// Terminate the commands with NULL so that execvp will properly execute them
		cmd1[split] = NULL;
		cmd2[count] = NULL;
	}

	// Return the enum
	return result;
}

// Function that pipes the output of the first command into the second command
void pipeCommand(char** cmd1, char** cmd2){
	// File descriptors
	int fds[2];
	pipe(fds);
	pid_t pid;

	// Child process 1
	if(fork() == 0){
		// Reassign stdin to fds[0] end of pipe
		dup2(fds[0], 0);

		// No writing in this child process so we can close this end of the pipe
		close(fds[1]);

		// Execute the second command
		execvp(cmd2[0], cmd2);
		perror("execvp failed");
	}
	// Child process 2
	else if((pid = fork()) == 0){
		// Reassign stdout to fds[1] end of pipe
		dup2(fds[1], 1);

		// No reading in this child process so we can close this end of the pipe
		close(fds[0]);

		// Execute the first command
		execvp(cmd1[0], cmd1);
		perror("execvp failed");
	}
	// Parent process
	else{
		waitpid(pid, NULL, 0);
		close(fds[0]);
		close(fds[1]);
	}
}

// Gets input from the user, split it into arguments, puts the arguments into an array, and returns the number of arguments
int readArgs(char** argv){
	// Declare variables
	char* cstr;
	string arg;
	int argc = 0;

	// Read in the arguments
	while(cin >> arg){
		// Exits if the user inputs an exit command
		if(checkQuit(arg)){
			cout << "Goodbye" << endl;
			exit(0);
		}

		// Convert the string into a c-string
		cstr = new char(arg.size() + 1);
		strcpy(cstr, arg.c_str());
		argv[argc] = cstr;

		// Increment the counter for the number of arguments
		argc++;

		// If the user hit enter, break
		if(cin.get() == '\n'){
			break;
		}
	}

	// Make the last character NULL so that exexvp will work properly
	argv[argc] = NULL;

	// Return the number of arguments
	return argc;
}

// Function to run the command for redirection
void redirectCommand(char** cmd, char** file){
	int fds[2]; // File descriptors
	int count; // Used for reading from stdout
	int fd; // Single file descriptor
	char c; // Used to read and write a character at a time
	pid_t pid; // Holds process ID

	pipe(fds);

	// Child process 1
	if(fork() == 0){
		// Open the file into the file descriptor
		fd = open(file[0], O_RDWR | O_CREAT, 0666);

		// If fd is negative, there was an error opening the file
		if(fd < 0){
			// Output the error and return
			printf("Error %s\n", strerror(errno));
			return;
		}

		dup2(fds[0], 0);

		// Don't need stdout end of pipe
		close(fds[1]);

		// Read from stdout
		while((count = read(0, &c, 1)) > 0){
			// Write to the file
			write(fd, &c, 1);
		}

		// A useless exec call that will stop the loop and allow user to input a command
		execlp("echo", "echo", NULL);
	}
	// Child Process 2
	else if((pid = fork()) == 0){
		dup2(fds[1], 1);

		// Don't need stdin end of pipe
		close(fds[0]);

		// Output contents of command to the file
		execvp(cmd[0], cmd);
		perror("Error: execvp failed");
	}
	// Parent process
	else{
		waitpid(pid, NULL, 0);
		close(fds[0]);
		close(fds[1]);
	}
}

// Function that will fork a process and run a command given a number of arguments(argc) and a list of
// arguments(argv)
void runCommand(int argc, char** argv){
	// Declare variables
	pid_t pid;

	// Fork our process
	pid = fork();

	// If pid is negative, there was an error forking
	if(pid < 0){
		perror("Error: there was an error forking");
	}
	// Child process
	else if(pid == 0){
		// Execute the command
		execvp(argv[0], argv);
		perror("Error: execvp error");
	}
	// Parent Process
	else{
		waitpid(pid, NULL, 0);
	}
}

bool checkQuit(string input){
	// Make the user input lowercase
	for(int i = 0; i < input.length(); i++){
		input[i] = tolower(input[i]);
	}

	return(input == "exit" || input == "quit");
}
