# Custom Linux Command Shell

A robust command-line interpreter developed in C, simulating core functionalities of a Unix shell. This project demonstrates comprehensive proficiency in fundamental operating system concepts, including process management, inter-process communication, I/O redirection, and advanced signal handling.

## 🚀 Features

This shell provides a foundational command-line experience with several advanced capabilities, mimicking common Unix shell behaviors:

-   **Basic Command Execution:** Executes standard external Linux commands (e.g., `ls`, `pwd`, `grep`).
-   **Built-in `cd` Command:** Handles directory changes internally within the shell process. Supports navigating to the home directory (`cd` or `cd ~`) and specific paths.
-   **Concurrent Command Execution:** Run multiple commands *in parallel* in the background, allowing the shell to continue processing other inputs without waiting. Commands are separated by the `&&` operator.
    ```bash
    command1 arg1 && command2 arg2 && command3
    ```
-   **Sequential Command Execution:** Execute multiple commands *one after another*, where each command completes before the next one starts. Commands are separated by the `##` operator.
    ```bash
    command1 arg1 ## command2 arg2 ## command3
    ```
-   **Output Redirection:** Redirects the standard output (`STDOUT`) of a command to a specified file. If the file does not exist, it is created; otherwise, its contents are truncated. Uses the `>` operator.
    ```bash
    ls -l > output.txt
    ```
-   **Command Pipelining:** Chains commands together, directing the standard output of one command as the standard input (`STDIN`) to the next. This enables complex data processing workflows using the `|` operator.
    ```bash
    cat file.txt | grep "pattern" | wc -l
    ```
-   **Robust Signal Handling:**
    *   **`SIGINT` (Ctrl+C) & `SIGTSTP` (Ctrl+Z)`:** The shell itself is immune to these signals, preventing accidental termination or suspension. Child processes spawned by the shell still respond to these signals, allowing user control over foreground tasks.
    *   **`SIGCHLD`:** A custom signal handler proactively reaps terminated child processes, effectively preventing the creation of "zombie" processes and ensuring efficient resource cleanup.
-   **Error Handling:** Provides informative error messages for incorrect commands ("Shell: Incorrect command") and specific `cd` failures (`perror("cd")`). Handles edge cases like empty commands and trailing pipe operators.
-   **Dynamic Input Processing:** Utilizes `getline()` for flexible input reading and `strsep()` for efficient tokenization of commands and arguments, accommodating varying input lengths and structures.

## ⚙️ How it Works: In-Depth Technical Breakdown

The shell's functionality is built upon a careful orchestration of low-level Linux system calls and C standard library functions.

### **1. Input & Parsing:**

-   **`getline()`:** Reads an entire line from `stdin` dynamically, allocating memory as needed. This prevents buffer overflows and allows for commands of arbitrary length.
-   **`strsep()`:** A powerful function used for tokenizing the input string. It's employed with various delimiters:
    *   `"&&"` to separate commands for parallel execution.
    *   `"##"` to separate commands for sequential execution.
    *   `"|"` to separate commands for pipelining.
    *   `" "` (space) to parse a single command into its executable name and arguments.
-   **Global Arrays:** `commands[]` stores parsed command strings for batch execution, and `args[]` stores arguments for a single command. `NULL` terminators are crucial for `execvp()`.

### **2. Process Management:**

-   **`fork()`:** The cornerstone of multi-tasking. For every external command or segment in parallel/piped execution, `fork()` creates a new child process. This isolates the execution of commands from the shell itself.
    *   The `pid_t` return value is used to differentiate between the parent and child processes (`pid == 0` for child, `pid > 0` for parent, `pid < 0` for error).
-   **`execvp()`:** In the child process, `execvp()` replaces the current process image with that of the command specified by `args[0]` and `args[]`. This is where the actual Linux command is run. If `execvp()` returns, it indicates an error (e.g., command not found).
-   **`waitpid()`:** The parent process uses `waitpid()` to:
    *   **Synchronize:** Wait for child processes to complete before continuing (e.g., for sequential commands or waiting for all parallel children to finish).
    *   **Prevent Zombies:** With `WNOHANG` option in the `SIGCHLD` handler, it reaps terminated children without blocking the shell.

### **3. Inter-Process Communication (IPC) & I/O Redirection:**

-   **`pipe()`:** Used exclusively for command pipelines (`|`). `pipe(pipes)` creates an anonymous pipe, providing two file descriptors: `pipes[0]` for reading and `pipes[1]` for writing.
    *   The write end of the pipe (`pipes[1]`) from an upstream command's child process is connected to the read end (`pipes[0]`) for a downstream command's child process.
-   **`dup2()`:** A critical system call for redirecting standard file descriptors:
    *   **Output Redirection (`>`):** In the child process, `dup2(fd, STDOUT_FILENO)` redirects `STDOUT` to a newly opened file descriptor (`fd`), ensuring command output goes to the file instead of the terminal.
    *   **Pipelining (`|`):**
        *   For intermediate commands, `dup2(pipes[1], STDOUT_FILENO)` redirects the command's output to the write end of the pipe.
        *   For downstream commands, `dup2(input_fd, STDIN_FILENO)` redirects the command's input from the read end of the previous pipe.
-   **`open()`, `close()`:** `open()` is used to create/access files for I/O redirection, and `close()` is meticulously used to close unused file descriptors (especially pipe ends) to prevent resource leaks.

### **4. Signal Handling:**

-   **`signal()`:** Used to register custom signal handlers.
    *   `signal(SIGINT, SIG_IGN)` and `signal(SIGTSTP, SIG_IGN)` in `main()` make the shell itself ignore `Ctrl+C` and `Ctrl+Z`, ensuring it continues running.
    *   `signal(SIGCHLD, signal_handler)` registers a handler that cleans up child processes.
-   **`signal_handler()`:** This custom function is invoked upon `SIGCHLD`. It uses a `while (waitpid(-1, &status, WNOHANG) > 0);` loop to repeatedly reap any child processes that have terminated without blocking the parent shell.

### **5. Built-in Commands (`cd`):**

-   The `cd` command **must** be executed directly by the shell's parent process (not a child `fork()`ed for `execvp()`). This is because a child process changing its directory would not affect the parent shell's current working directory.
-   **`chdir()`:** The system call used to change the current working directory.
-   **`getenv("HOME")`:** Retrieves the user's home directory path for `cd` without arguments or `cd ~`.

## 📂 Project Structure

-   `myshell.c`: The main source file containing all the shell's logic.
    -   `main()`: The entry point, handles the main shell loop, input, and high-level command routing.
    -   `signal_handler()`: Custom handler for `SIGCHLD`.
    -   `parseInput()`: Parses command lines based on `&&`, `##`, or `|` delimiters.
    -   `parseCommandArgs()`: Parses a single command string into executable and arguments.
    -   `executeCommand()`: Handles single command execution, including built-ins and redirection.
    -   `executeParallelCommands()`: Manages concurrent execution of multiple commands.
    -   `executeSequentialCommands()`: Manages sequential execution of multiple commands.
    -   `executeCommandRedirection()`: A wrapper for `executeCommand` to specifically handle redirection (though integrated into `executeCommand`).
    -   `executePipedCommands()`: Orchestrates complex command pipelines.

## 🛠️ Build and Run

To compile and run the shell:

```bash
gcc myshell.c -o myshell
./myshell
