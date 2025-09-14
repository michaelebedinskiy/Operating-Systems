# Operating Systems – Homework Exercise 2 🖥️

## Overview
This project implements **Homework Exercise 2 (“Wet”)** for the Operating Systems course (234123). The focus is on extending the Linux kernel with **new system calls** to support **process clearance**, allowing processes to be assigned one or more of five security clearance types: Sword, Midnight, Clamp, Duty, and Isolate.

## Assignment Goal
The main goal is to implement five system calls in `kernel/hw2.c`: `hello()`, `set_sec()`, `get_sec()`, `check_sec()`, and `flip_sec_branch()`. These calls enable assigning, checking, and manipulating process clearances, including inheritance during fork operations and selective modification of parent process clearances.

## Kernel and User-Side Implementation
The project includes both **kernel- and user-space components**. Kernel modifications are applied on an Ubuntu virtual machine using a custom-built Linux kernel. User-space programs use a provided testing framework to validate the implemented system calls.

## Features
- Adds support for five distinct process clearance types.
- Implements system calls to set, query, and manipulate clearances.
- Ensures clearance inheritance when processes are forked.
- Provides a simple user-space testing framework to verify kernel modifications.
