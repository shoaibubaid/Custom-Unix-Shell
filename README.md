# Development of a Custom Unix Shell

**Project Type:** Self Project  
**Duration:** May 2025 – Jul 2025  

## 1. Introduction
This project involves the development of a Unix-style command-line shell from scratch. The shell replicates core functionalities of standard shells (like Bash or Zsh), providing a user-friendly interface for executing commands, managing processes, and performing I/O operations. The project demonstrates understanding of system-level programming, process management, and data structures for interactive features.

## 2. Objectives
- Create a functional command-line shell supporting essential Unix commands.
- Implement process control and manage foreground/background tasks.
- Support pipelines, I/O redirection, and background execution.
- Implement tab completion using a Trie data structure.
- Maintain persistent command history across shell sessions.

## 3. Key Features

### 3.1 Built-in Commands
Supports common built-in commands:
- `cd` – Change directory
- `echo` – Display messages or variables
- `exit` – Terminate the shell

Built-in commands execute without creating a new process, enabling efficient state management.

### 3.2 Pipelines and I/O Redirection
- **Pipelines (`|`)**: Chain commands, passing output of one as input to another.
- **I/O Redirection (`>`, `<`)**: Redirect standard input/output to files.

**Implementation Details:**
- Pipes created using `pipe()`
- `dup2()` redirects stdin/stdout for each command in the pipeline
- Commands execute in child processes using `fork()` and `execvp()`

### 3.3 Background Execution (`&`)
- Commands ending with `&` run in the background.
- Shell tracks background process IDs and handles termination using `SIGCHLD`.

### 3.4 Tab Completion Using Trie
- Trie stores all executable names and built-in commands.
- Pressing `Tab` completes partially typed commands.
- Multiple matches show suggestions to the user.

### 3.5 Persistent Command History
- Commands stored in `history.txt` and loaded at shell startup.
- Supports `↑/↓` navigation.
- Maintains a fixed-size history, removing oldest entries when limit is reached.

## 4. Process and Job Control
- **Process creation:** `fork()` spawns child processes.
- **Command execution:** `execvp()` replaces child memory with target program.
- **I/O management:** `dup2()` handles redirection; `pipe()` manages IPC.
- **Signal handling:**
  - `SIGINT`, `SIGTSTP` – Interrupt/suspend foreground processes
  - `SIGCHLD` – Reaps background processes to prevent zombies

This allows robust management of concurrent foreground and background tasks.

## 5. Implementation Workflow
1. **Startup:**
   - Load persistent command history
   - Build Trie for tab completion
   - Initialize signal handlers
2. **Command Input:**
   - Read user input in raw mode
   - Handle backspace, tab completion, and history search
3. **Parsing and Execution:**
   - Tokenize input into arguments
   - Check if command is built-in or external
   - Handle pipes and I/O redirection
   - Execute background/foreground processes
4. **Post-Execution:**
   - Update command history
   - Rebuild Trie if directory changes
   - Return prompt to the user

## 6. Tools and Libraries
- **C++ Standard Library:** `iostream`, `string`, `list`, `map`, `algorithm`
- **POSIX System Calls:** `fork`, `execvp`, `pipe`, `dup2`, `chdir`
- **Signal Handling:** `signal()` for job control and user interrupts
- **Filesystem Access:** `opendir()`, `readdir()`, `getcwd()` for tab completion and `cd`

## 7. Outcome
- Fully functional Unix-like shell capable of handling multiple concurrent tasks, pipelines, redirection, and interactive features.
- Demonstrated system-level programming skills, data structure usage, and process management.
- Serves as a foundation for advanced shell development and understanding operating system internals.
