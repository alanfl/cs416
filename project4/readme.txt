1. Building the shell
$ make

2. Running the shell
$ ./cshell

3. Usage
# Exit the shell
/ilab/users/afl59 $ exit

# Usage info
/ilab/users/afl59 $ help

# Change directory
/ilab/users/afl59 $ cd ..

# List directories
/ilab/users/afl59 $ ls

# Redirect output
/ilab/users/afl59 $ wc < cshell.c > test

# Pipe output
/ilab/users/afl59 $ ls | head -3 | tail -1

# Multiple commands
/ilab/users/afl59 $ cd .. ; ls

# Putting it all together
/ilab/users/afl59 $ cd .. ; ls | head -3 | tail -1 > test ; wc < test

4. Ctrl + C Support
/ilab/users/afl59 $ tail -f
^C
Ctrl+C caught, type exit to quit or press enter to continue: