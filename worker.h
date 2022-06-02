// File:	worker_t.h

// List all group member's name: Abhilash Kolluri(ak2048), Srikar Gona(gs943)
// username of iLab: gs943
// iLab Server: ilab3.cs.rutgers.edu

#ifndef WORKER_T_H
#define WORKER_T_H

#define _GNU_SOURCE


/* include lib header files that you need here: */
#include <unistd.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <ucontext.h>
#include <signal.h>
#include <sys/time.h>
#include <string.h>
#include <time.h>

/* To use Linux pthread Library in Benchmark, you have to comment the USE_WORKERS macro */
#define USE_WORKERS 1

// defines used for knowing the status of threads.
#define READY 0
#define SCHEDULED 1
#define BLOCKED 2
#define DONE 3

//Using a timeslice of 4ms
#define TIMESLICE 4

typedef uint worker_t;

/* define your data structures here: */
// Feel free to add your own auxiliary data structures (linked list or queue etc...)

// YOUR CODE HERE

typedef struct TCB {
	/* add important states in a thread control block */
	// thread Id
	// thread status
	// thread context
	// thread stack
	// thread priority
	// And more ...

	// YOUR CODE HERE

	// thread id
	worker_t tid;

	//thread status, ie if its ready, running or blocked etc...
	int status;

	//Thread context, use ucontext provided by linux here
	ucontext_t cntx;

	//Thread Priority, 
	//For this assignment, 4 priority levels will be used.
	// 4 is the highest priority and 1 being the lowest.
	int priority;

} tcb;


// A structure that encapsulates a thread, it holds a pointer
// to the TCB structure which is unique to that thread
typedef struct worker_thread{

	tcb *tcb_block;

} worker_thread;

// We will be using a linked list structure.
// The below structure is a node that points
// to a thread structure and has next pointer to point to next node.
typedef struct node{
	worker_thread *thrd;
	struct node *next;
}node;


// Runqueue structur so is holds list of all jobs
typedef struct runqueue{
	//head of runqueue
	node *head;

	//tail of runqueue
	node *tail;
} runqueue;

/* mutex struct definition */
typedef struct worker_mutex_t {
	/* add something here */

	// YOUR CODE HERE

	// Flag to know if mutex ie lock is available or not
	// When this value is 0 it is available else it is not
	int flag;

	// Thread pointer for which mutex has been assigned to
	worker_thread *t;
} worker_mutex_t;


/* Function Declarations: */

/* create a new thread */
int worker_create(worker_t * thread, pthread_attr_t * attr, void
    *(*function)(void*), void * arg);

/* give CPU pocession to other user level worker threads voluntarily */
int worker_yield();

/* terminate a thread */
void worker_exit(void *value_ptr);

/* wait for thread termination */
int worker_join(worker_t thread, void **value_ptr);

/* initial the mutex lock */
int worker_mutex_init(worker_mutex_t *mutex, const pthread_mutexattr_t
    *mutexattr);

/* aquire the mutex lock */
int worker_mutex_lock(worker_mutex_t *mutex);

/* release the mutex lock */
int worker_mutex_unlock(worker_mutex_t *mutex);

/* destroy the mutex */
int worker_mutex_destroy(worker_mutex_t *mutex);

#ifdef USE_WORKERS
#define pthread_t worker_t
#define pthread_mutex_t worker_mutex_t
#define pthread_create worker_create
#define pthread_exit worker_exit
#define pthread_join worker_join
#define pthread_mutex_init worker_mutex_init
#define pthread_mutex_lock worker_mutex_lock
#define pthread_mutex_unlock worker_mutex_unlock
#define pthread_mutex_destroy worker_mutex_destroy
#endif

#endif
