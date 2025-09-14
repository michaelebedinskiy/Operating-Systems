# Operating Systems – Homework Exercise 3 🖥️

## Overview
This project implements a **multi-threaded web server** in C that:

- Handles **concurrent HTTP GET and POST requests**  
- Maintains a **server log** with reader-writer synchronization  
- Collects **detailed usage statistics** for requests and threads  

The server is built on top of the provided **single-threaded starter server**.

---

## Features

### Multi-threaded Server
- Master thread (producer) accepts incoming client connections  
- Fixed-size worker thread pool (consumers) processes requests concurrently  
- **Bounded FIFO request queue** with blocking when full  
- Maintains **FIFO order** for requests  

### Request Handling
- **GET requests**:  
  - Treated as writers to the server log  
  - Append entries to the log  
  - Require exclusive access to the log  

- **POST requests**:  
  - Treated as readers  
  - Return the contents of the server log  
  - Multiple POST requests can read simultaneously  

### Reader-Writer Synchronization
- Implements **writer priority** to avoid starvation  
- Uses **mutexes** and **condition variables** for safe access  
- No busy-waiting or spinlocks  

### Usage Statistics
- **Per-request statistics**:  
  - `Stat-Req-Arrival` — request arrival timestamp  
  - `Stat-Req-Dispatch` — dispatch interval before worker processes request  
- **Per-thread statistics**:  
  - `Stat-Thread-Id` — worker thread ID  
  - `Stat-Thread-Count` — total requests handled  
  - `Stat-Thread-Static` — total static GET requests handled  
  - `Stat-Thread-Dynamic` — total dynamic GET requests handled  
  - `Stat-Thread-Post` — total POST requests handled  

All statistics are returned to the client in HTTP response headers.

---

## Implementation Details

### Thread Pool
- Worker threads are created at server startup  
- Threads wait on a **condition variable** for incoming requests  
- Master thread signals workers when a new request is added to the queue  

### Request Queue
- Fixed size (`queue_size` specified at launch)  
- Master thread blocks when queue is full  
- FIFO order is maintained for request handling  

### Server Log
- Implemented with **reader-writer lock**  
- GET requests (writers) gain exclusive access  
- POST requests (readers) can read concurrently  
- Writers have priority over readers to prevent starvation  

### Synchronization
- **pthread_mutex_t** used for mutual exclusion  
- **pthread_cond_t** used for signaling threads  
- Correct locking ensures no race conditions or deadlocks  

### Error Handling
- Wrapper functions from `segel.c` used for safe system calls  
- Errors in thread creation, locking, or I/O are immediately reported and handled
- 
