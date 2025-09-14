#include "segel.h"
#include "request.h"
#include "log.h"

//
// server.c: A very, very simple web server
//
// To run:
//  ./server <portnum (above 2000)>
//
// Repeatedly handles HTTP requests sent to this port number.
// Most of the work is done within routines written in request.c
//

typedef struct RequestItem {
    int connfd;
    struct timeval arrival_time;
    struct timeval dispatch_time;
} RequestItem;

typedef struct RequestQueue {
    RequestItem *buffer;
    int capacity;
    int front;
    int rear;
    int count;
    int handledCount;

    pthread_mutex_t lock;
    pthread_cond_t not_empty;
    pthread_cond_t not_full;
} RequestQueue;

RequestQueue *request_queue;
pthread_t *worker_threads;
threads_stats* thread_stats_array;
int num_threads;
server_log my_server_log;

void queue_init(RequestQueue *q, int capacity) {
    q->buffer = (RequestItem *)malloc(sizeof(RequestItem) * capacity);
    if (q->buffer == NULL) {
        unix_error("Could not allocate memory for queue buffer");
    }
    q->capacity = capacity;
    q->front = 0;
    q->rear = -1; //indicates empty queue
    q->count = 0;
    q->handledCount = 0;

    pthread_mutex_init(&q->lock, NULL);
    pthread_cond_init(&q->not_empty, NULL);
    pthread_cond_init(&q->not_full, NULL);
}

void queue_destroy(RequestQueue *q) {
    free(q->buffer);
    pthread_mutex_destroy(&q->lock);
    pthread_cond_destroy(&q->not_empty);
    pthread_cond_destroy(&q->not_full);
}

void queue_enqueue(RequestQueue *q, RequestItem item) {
    pthread_mutex_lock(&q->lock);
    while (q->count + q->handledCount >= q->capacity) {  //if full
        pthread_cond_wait(&q->not_full, &q->lock);
    }
    q->rear = (q->rear + 1) % q->capacity;
    q->buffer[q->rear] = item;
    q->count++;

    pthread_cond_signal(&q->not_empty);
    pthread_mutex_unlock(&q->lock);
}

RequestItem queue_dequeue(RequestQueue *q) {
    pthread_mutex_lock(&q->lock);
    while (q->count == 0) { //if empty
        pthread_cond_wait(&q->not_empty, &q->lock);
    }
    RequestItem item = q->buffer[q->front];
    q->front = (q->front + 1) % q->capacity;
    q->count--;
    q->handledCount++;
    //pthread_cond_signal(&q->not_full);
    pthread_mutex_unlock(&q->lock);
    return item;
}

void *worker(void *arg) {
    long thread_idx = (long)arg; //worker's index in the global stats array
    threads_stats my_stats = thread_stats_array[thread_idx];
    my_stats->id = (int)thread_idx + 1; //id from 1 to N

    while (1) {
        RequestItem item = queue_dequeue(request_queue);
        gettimeofday(&item.dispatch_time, NULL); //record request pick up time
        //request_queue->handledCount++;
        requestHandle(item.connfd, item.arrival_time, item.dispatch_time, my_stats, my_server_log);

        Close(item.connfd); //close connection after handling
        pthread_mutex_lock(&request_queue->lock);
        request_queue->handledCount--;
        pthread_cond_signal(&request_queue->not_full);
        pthread_mutex_unlock(&request_queue->lock);
    }
    return NULL;
}

// Parses command-line arguments
void getargs(int *port, int *threads, int *queue_size, int argc, char *argv[])
{
    if (argc < 4) {
        fprintf(stderr, "Usage: %s <port> <threads> <queue_size>\n", argv[0]);
        exit(1);
    }
    *port = atoi(argv[1]);
    *threads = atoi(argv[2]);
    *queue_size = atoi(argv[3]);

    if (*port < 1024 || *port > 65535) {
        fprintf(stderr, "Port number must be between 1024 and 65535\n");
        exit(1);
    }
    if (*threads <= 0) {
        fprintf(stderr, "Threads must be a positive integer\n");
        exit(1);
    }
    if (*queue_size <= 0) {
        fprintf(stderr, "Queue_size must be a positive integer\n");
        exit(1);
    }
}

int main(int argc, char *argv[])
{
    int listenfd, connfd, port, clientlen, queue_capacity;
    struct sockaddr_in clientaddr;

    getargs(&port, &num_threads, &queue_capacity, argc, argv);

    my_server_log = create_log();
	request_queue = (RequestQueue *)malloc(sizeof(RequestQueue));
	if (request_queue == NULL) {
		unix_error("Could not allocate memory for request queue");
	}
    queue_init(request_queue, queue_capacity);

    thread_stats_array = (threads_stats *)malloc(sizeof(threads_stats) * num_threads);
    if (thread_stats_array == NULL) {
        unix_error("Could not allocate memory for thread stats array");
    }

    worker_threads = (pthread_t *)malloc(sizeof(pthread_t) * num_threads);
    if (worker_threads == NULL) {
        unix_error("Could not allocate memory for worker_threads");
    }

    for (int i = 0; i < num_threads; i++) {
        thread_stats_array[i] = (threads_stats)malloc(sizeof(struct Threads_stats));
        if (thread_stats_array[i] == NULL) {
            unix_error("Could not allocate memory for thread stats struct");
        }
        thread_stats_array[i]->id = 0;
        thread_stats_array[i]->stat_req = 0;
        thread_stats_array[i]->dynm_req = 0;
        thread_stats_array[i]->post_req = 0;
        thread_stats_array[i]->total_req = 0;

        pthread_create(&worker_threads[i], NULL, worker, (void *)(long)i);
    }

    listenfd = Open_listenfd(port);
    while (1) {
        pthread_mutex_lock(&request_queue->lock);
        while (request_queue->count + request_queue->handledCount >= request_queue->capacity) {
            pthread_cond_wait(&request_queue->not_full, &request_queue->lock);
        }
        pthread_mutex_unlock(&request_queue->lock);
        clientlen = sizeof(clientaddr);
        connfd = Accept(listenfd, (SA *)&clientaddr, (socklen_t *) &clientlen);
        pthread_mutex_lock(&request_queue->lock);

        RequestItem new_request;
        new_request.connfd = connfd;
        gettimeofday(&new_request.arrival_time, NULL);
        request_queue->rear = (request_queue->rear + 1) % request_queue->capacity;
        request_queue->buffer[request_queue->rear] = new_request;
        request_queue->count++;
        pthread_cond_signal(&request_queue->not_empty);
        pthread_mutex_unlock(&request_queue->lock);
    }

    // Clean up the server log before exiting
    for (int i = 0; i < num_threads; i++) {
        pthread_join(worker_threads[i], NULL);
        free(thread_stats_array[i]);
    }

    free(worker_threads);
    free(thread_stats_array);

    destroy_log(my_server_log);
    queue_destroy(request_queue);

    return 0;
}