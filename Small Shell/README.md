# Operating Systems – Homework Exercise 1 🖥️

## Overview
This project implements a small Unix shell, referred to as **smash**, that mimics the behavior of a Linux shell but supports a limited subset of commands. The shell handles both **built-in commands** and **external commands**, and includes signal handling, job control, and optional bonus features like I/O redirection and piping.

## Assignment Goal
The main goal is to implement a functional shell that supports:
- Running built-in commands like `cd`, `pwd`, `showpid`, `jobs`, `fg`, `quit`, `kill`, `alias`, `unalias`, `unsetenv`, `watchproc`, and others.
- Executing external commands, both simple and complex (with wildcards or special characters), in the foreground or background.
- Proper signal handling for Ctrl+C (SIGINT) to terminate foreground processes.
- Features like I/O redirection (`>` and `>>`), pipes (`|` and `|&`), and network or system inspection commands (`netinfo`, `du`, `whoami`).

## Kernel and User-Side Implementation
The project is implemented entirely in **C/C++**. User-space programs are executed via the shell using `fork`, `execv`, and `waitpid` system calls. Signal handling is implemented to manage foreground and background jobs correctly. The shell tracks running jobs, assigns job IDs, and ensures proper cleanup of finished jobs.

## Features
- Command prompt customization (`chprompt`).
- Built-in command execution within the shell process.
- Background and foreground job management.
- Job listing and process ID management (`jobs`, `fg`, `kill`).
- Alias creation and removal (`alias`, `unalias`).
- Environment variable management (`unsetenv`).
- Monitoring CPU and memory usage (`watchproc`).
- Optional advanced features: I/O redirection, pipes, disk usage (`du`), user info (`whoami`), network info (`netinfo`).

## Assumptions
- Maximum of 100 simultaneous processes.
- Maximum command length: 200 characters.
- Maximum of 20 arguments per command.
- File and folder names in lowercase, without special characters.
- `new`/`malloc` are assumed to succeed; memory leaks are not checked.
- Commands with I/O redirection or pipes are not executed in the background.

## Error Handling
- Invalid arguments or command formats produce standardized error messages.
- System call failures are reported using `perror` with a descriptive message.
- Finished background jobs are removed from the jobs list automatically before printing or adding new jobs.

## Notes
- Use of external libraries that bypass system calls is prohibited.
