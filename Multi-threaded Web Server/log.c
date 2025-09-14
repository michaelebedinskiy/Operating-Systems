#include <stdlib.h>
#include <string.h>
#include "log.h"
#include "segel.h"

typedef struct LogEntry {
    char data[MAXBUF];
    struct LogEntry* next;
} LogEntry;

// Opaque struct definition
struct Server_Log {
    LogEntry *head;
    LogEntry *tail;
    size_t total_length;

    pthread_mutex_t lock;
    pthread_cond_t readers_cond;
    pthread_cond_t writers_cond;
    int readers_active;
    int writers_active;
    int writers_waiting;
};

// Creates a new server log instance (stub)
server_log create_log() {
    server_log log = (server_log)malloc(sizeof(struct Server_Log));
    if (log == NULL) {
        unix_error("Failed to allocate memory for server log");
    }

    log->head = NULL;
    log->tail = NULL;
    log->total_length = 0;

    pthread_mutex_init(&log->lock, NULL);
    pthread_cond_init(&log->readers_cond, NULL);
    pthread_cond_init(&log->writers_cond, NULL);
    log->readers_active = 0;
    log->writers_active = 0;
    log->writers_waiting = 0;

    return log;
}

// Destroys and frees the log (stub)
void destroy_log(server_log log) {
    if (log == NULL) return;

    LogEntry* current = log->head;
    while (current != NULL) {
        LogEntry* next = current->next;
        free(current);
        current = next;
    }

    pthread_mutex_destroy(&log->lock);
    pthread_cond_destroy(&log->readers_cond);
    pthread_cond_destroy(&log->writers_cond);
    free(log);
}

// Returns dummy log content as string (stub)
int get_log(server_log log, char** dst) {
    pthread_mutex_lock(&log->lock);

    while (log->writers_active > 0 || log->writers_waiting > 0) {
        pthread_cond_wait(&log->readers_cond, &log->lock);
    }
    log->readers_active++;

    size_t num_entries = 0;
    LogEntry* counter = log->head;
    while(counter != NULL) {
        num_entries++;
        counter = counter->next;
    }

    //make space for the content + null terminator
    size_t total_alloc_len = log->total_length + num_entries + 1;
    *dst = (char*)malloc(total_alloc_len);
    if (*dst == NULL) {
        pthread_mutex_unlock(&log->lock);
        unix_error("Failed to allocate memory for server log");
    }
    (*dst)[0] = '\0'; //start with empty string

    LogEntry* current = log->head;
    while (current != NULL) {
        strcat(*dst, current->data);
        strcat(*dst, "\n");
        current = current->next;
    }

    log->readers_active--;
    if (log->readers_active == 0 && log->writers_waiting > 0) {
        pthread_cond_signal(&log->writers_cond);
    }

    pthread_mutex_unlock(&log->lock);
    return strlen(*dst);
}

// Appends a new entry to the log (no-op stub)
void add_to_log(server_log log, const char* data, int data_len) {
    // This function should handle concurrent access
    pthread_mutex_lock(&log->lock);
    log->writers_waiting++;
    while (log->readers_active > 0 || log->writers_active > 0) { //wait if there are other active w or r or waiting w
        pthread_cond_wait(&log->writers_cond, &log->lock);
    }
    log->writers_waiting--;
    log->writers_active = 1;
    LogEntry* new_entry = (LogEntry*)malloc(sizeof(LogEntry));
    if (new_entry == NULL) {
        unix_error("Failed to allocate memory for log entry");
    }
    strncpy(new_entry->data, data, MAXBUF - 1);
    new_entry->data[MAXBUF - 1] = '\0';

    int actual_data_len = strlen(new_entry->data);

    new_entry->next = NULL;

    if (log->head == NULL) {
        log->head = new_entry;
        log->tail = new_entry;
    } else {
        log->tail->next = new_entry;
        log->tail = new_entry;
    }
    log->total_length += actual_data_len;

    log->writers_active = 0;
    
    pthread_cond_broadcast(&log->writers_cond);
    pthread_cond_broadcast(&log->readers_cond);

    pthread_mutex_unlock(&log->lock);
}
