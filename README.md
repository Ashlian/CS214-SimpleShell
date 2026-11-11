# CS214-SimpleShell
Project 3 of CS214 Systems Programming

By: Jeffrey Lu (jl3925) & Alexander Wong (amw408)

# Run Instructions:
From the project root, run `make` to build and make the program. The executables are placed in the `build directory`. To remove all build outputs, run `make clean`

The program is run directly from the project root through `.build/mysh [file]`
- When given no arguments, the shell runs with interactive mode, which prints the working directory and intro and ending messages
- When given a file argument, the shell runs under batch mode, which reads the commands in the file and runs them sequentially

# Design Notes:
`mysh` creates an array of command structs that consists of the array of arguments, the number of arguments, input and output files if redirected, and a flag if output should be appended.`mysh` reads the users input from the standard input in interactive mode or the given file in batch mode using a character buffer. When the buffer reaches a newline character, the following steps occur:

1. **Tokenize** - The buffer is split into an array of tokens, each one separated by whitespace. 
2. **Setup Command List** - Iterating over the list of tokens, regular tokens are added to the current command argument list. If a token has a wildcard ('*'), then all files that have a name matching the pattern given in either the working directory or a given directory are added to that command's argument list. If a token is a pipe character ('|'), then a new command gets made with the tokens after being added to the new command's argument list. If a token is a redicrection character ('<','>','>>'), then the token after it is used as the file argument inside the command struct, with the append flag being set when necessary.
3. **Run Commands** - The program iterates over the command list and runs each command one by one. If the command is only a built-in command with no redirection or pipes, then it is run in the parent process. Otherwise, every command is ran in a child process, with the program checking for redirections and pipes and setting them up accordingly. When all commands are run, the parent process checks the status of the last child process to determine whether or not the commands succeeded or failed.

# Test Strategy:
