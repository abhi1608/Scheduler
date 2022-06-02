// File:	worker.c

// List all group member's name: Abhilash Kolluri(ak2048), Srikar Gona(gs943)
// username of iLab: gs943
// iLab Server: ilab3.cs.rutgers.edu

#include "worker.h"
// INITAILIZE ALL YOUR VARIABLES HERE
// YOUR CODE HERE

// Stack size being used
#define STACK_SIZE SIGSTKSZ


// Maximum number of threads, for this project we have defined it to be 150
#define MAX_THREADS 250


//Structure to compute timestamp of day
struct timeval tv;

//Threads start time
unsigned long thread_start_time[MAX_THREADS];

//Time when threads get scheduled
unsigned long thread_schedule_time[MAX_THREADS];

//Time when threads are done
unsigned long thread_completion_time[MAX_THREADS];

// Total response time of all threads, this should be divided by number of threads to
// compute the actual response time
long long int response_time = 0;

// Total turnaround time of all threads, this should be divided by number of threads to
// compute the actual turnaround time
long long int turnaround_time = 0;

// Global used to uniquely assign a thread id to all the threads created.
worker_t id = 1;

//int j = 0;


// To be used so that initialized function is called only once
bool did_intialize = false;

// Use to store the address of thread that is currently being run by the scheduler/
worker_thread *running_thread = NULL;

//Scheduler Context
ucontext_t scheduler_context;

// Required structures for signal handler implementation
struct sigaction sa;
struct itimerval timer;


// Run Queue for scheduler, All threads will be added
// to the scheduler runqueue that needs to be executed.
// Also, this will act as one of levels with higher priority
// for RR scheduling policy. 
struct runqueue *RR; //highest priority in case of MLFQ

// For a 4 level MLFQ, the 2 other 3 levels of runqueues are defined here:
struct runqueue *mlfq_level_3;
struct runqueue *mlfq_level_2;
struct runqueue *mlfq_level_1;


// This list holds all threads that require the lock
struct runqueue *blocked_thread_list = NULL;

//Array to store if thread exited and the exit values of all the threads
int thread_exit_status[MAX_THREADS];
void *thread_exit_value[MAX_THREADS];


//For the scheduler to know if the current thread has been blocked
int is_current_thread_blocked = 0;

//For the scheduler to know if the current thread yielded within its time slice
int did_thread_yield = 0;

//To check if the scheduler has started its function
bool scheduler_started = false;

ucontext_t mainContext;
worker_thread *main_thread;

//to check if the response and turn_around time is printed
int isPrinted = 0;

void timer_handler(int signum);

void insert_queue(worker_thread *new_thread, runqueue *queue);

void unblock_threads();

void free_thread_memory(worker_thread *thread);

static void sched_rr(runqueue *queue);

static void sched_mlfq();

static void schedule();

void printTimes();



void initialize()
{
	did_intialize = true;

	//Initializing and registering signal handler for timer interrupt
	memset(&sa, 0, sizeof(sa));
	sa.sa_handler = &timer_handler;
	sigaction(SIGPROF, &sa, NULL);


	//Creating Run Queue for Scheduler to hold list of jobs.
	RR = (runqueue *)malloc(sizeof(runqueue));

	//Allocate memory for 3 other levels of MLFQ scheduling

	mlfq_level_3 = (runqueue *)malloc(sizeof(runqueue));
	mlfq_level_2 = (runqueue *)malloc(sizeof(runqueue));
	mlfq_level_1 = (runqueue *)malloc(sizeof(runqueue));

	// if a thread requires a mutex and mutex is not available
	// those threads have to be put into blocked list.

	blocked_thread_list = (runqueue *)malloc(sizeof(runqueue));

	//Initializing arrays which will hold exit information of threads.
	for (int i = 0; i < MAX_THREADS; i++) {
		thread_exit_status[i] = 0;
		thread_exit_value[i] = NULL;
	}

	//now set the global context so that program goes to schedule function
	//so , we can use sch later whenever we want to go to schedule function
	if (getcontext(&scheduler_context) < 0) {
		perror("getcontext fails for scheduler");
		exit(1);
	}

	//Allocating stack and other parameters for scheduler context
	void *stack = malloc(STACK_SIZE);
	scheduler_context.uc_link = NULL;
	scheduler_context.uc_stack.ss_sp = stack;
	scheduler_context.uc_stack.ss_size = STACK_SIZE;
	scheduler_context.uc_stack.ss_flags = 0;

	// Setup the scheduler context to start running schedule function
	makecontext(&scheduler_context, schedule, 0);

}


/* create a new thread */
int worker_create(worker_t * thread, pthread_attr_t * attr, 
                      void *(*function)(void*), void * arg) {

       // - create Thread Control Block (TCB)
       // - create and initialize the context of this worker thread
       // - allocate space of stack for this thread to run
       // after everything is set, push this thread into run queue and 
       // - make it ready for the execution.

       // YOUR CODE HERE

	if (did_intialize == false) {
		initialize();
	}
	
	//Create Main thread and add to the scheduler
	if (scheduler_started == false) {
		getcontext(&mainContext);
		if(scheduler_started == false) {
			main_thread = (worker_thread *)malloc(sizeof(worker_thread));
			main_thread->tcb_block = (tcb *)malloc(sizeof(tcb));
			main_thread->tcb_block->tid = id;
			id++;
			// Main thread is given a id of 1, all other threads
			// are given an id starting from 2.
	        main_thread->tcb_block->status = READY;
			main_thread->tcb_block->cntx = mainContext;
			//Set highest priority for main thread
			main_thread->tcb_block->priority = 4;

			//Push the main thread into the job queue
			insert_queue(main_thread, RR);
			//Update that the scheduler started
			scheduler_started = true;
			//Transfer the control to the scheduler
			setcontext(&scheduler_context);
		}
	}

	// Creating the thread by allocating memory to store its pointer
	worker_thread *new_thread = (worker_thread *) malloc(sizeof (worker_thread));

	// Allocating memory for control block and populating it in new_thread
	new_thread->tcb_block = (tcb *) malloc(sizeof(tcb));

       
	//Assigning unique thread id to this newly created thread
	*thread = id;
	(new_thread->tcb_block)->tid = *thread;
	id++;

	// Update the status of thread to Ready
	(new_thread->tcb_block)->status = READY;

	// Update the priority of thread
	(new_thread->tcb_block)->priority = 4;

	//Create the context for this thread and stack for that context
	ucontext_t ctx;
	if (getcontext(&ctx) < 0){
		perror("getcontext for new thread failed");
		exit(1);
	}

	void *stack = malloc(STACK_SIZE);
	ctx.uc_link = NULL;
	ctx.uc_stack.ss_sp = stack;
	ctx.uc_stack.ss_size = STACK_SIZE;
	ctx.uc_stack.ss_flags = 0;
	
	// Alter the path of context ot run the function provided
	// as arguments. Also, pass the arguments that user has 
	// provided while creating the thread.

	if (arg == NULL) {
		makecontext(&ctx, (void *)function, 0);
	}
	else {
		makecontext(&ctx, (void *)function, 1,arg);
	}
	
	//Update the tcb context to this context which we created above
	(new_thread->tcb_block)->cntx = ctx;

	//Finally, Add this thread to the runqueue
	insert_queue(new_thread, RR);

	//Calculating the timestamp.
	gettimeofday(&tv, NULL);
	unsigned long time = 1000000 * tv.tv_sec + tv.tv_usec;

	thread_start_time[*thread] = time;
	//printf("for thread %d start time is %ld\n",*thread, thread_start_time[*thread]);
	
    return 0;
};

/* give CPU possession to other user-level worker threads voluntarily */
int worker_yield() {
	
	// - change worker thread's state from Running to Ready
	// - save context of this thread to its thread control block
	// - switch from thread context to scheduler context

	// YOUR CODE HERE

	(running_thread->tcb_block)->status = READY;
	
	// From this we can know that if a thread has yielded, this is useful to determine whether
	// thread should be in same priority or should priority be reduced in MLFQ scheduling.
	did_thread_yield = 1;

	//Disable the timer for this thread before context switching
	timer.it_value.tv_usec = 0;
	timer.it_value.tv_sec = 0;

	swapcontext(&((running_thread->tcb_block)->cntx), &scheduler_context);
	
	return 0;
};

/* terminate a thread */
void worker_exit(void *value_ptr) {
	// - de-allocate any dynamic memory created when starting this thread

	// YOUR CODE HERE
	
	// Get the index of the exiting thread
	int t_index = (running_thread->tcb_block)->tid;

	//Indicate that the thread exited
	thread_exit_status[t_index] = 1;

	//If passed value ptr is not NULL, populate it the exit status
	if(value_ptr != NULL)
	{
		thread_exit_value[t_index] = value_ptr;
	}
	else 
	{
		thread_exit_value[t_index] = NULL;
	}

	//Free up the memory of exiting thread
	free_thread_memory(running_thread);

	//Make current point to NULL, so scheduler is aware not to pick up new thread from queue.
	running_thread = NULL;

	//Disable the timer for this thread before exiting
	timer.it_value.tv_usec = 0;
	timer.it_value.tv_sec = 0;
	
	gettimeofday(&tv, NULL);
	unsigned long time = 1000000 * tv.tv_sec + tv.tv_usec;

	thread_completion_time[t_index] = time;
	//printf("for thread %d completion time is %ld\n",t_index, thread_completion_time[t_index]);


	response_time += (thread_schedule_time[t_index] - thread_start_time[t_index]);

	turnaround_time += (thread_completion_time[t_index] - thread_start_time[t_index]);



	// TODO; The above response time and turnaround time should be divided
	// by total number of threads   which is equal to (id -1).
	// printf("Response time is, %lld\n",response_time);
	// printf("Turnaround time is, %lld\n", turnaround_time);

	// long long int response_time_1 = response_time / (id-1);

	// long long int turn_around_time_1 = turnaround_time / (id-1);

	// printf("Response time after dividing is, %lld\n",response_time_1);
	// printf("Turnaround time after dividing is, %lld\n", turn_around_time_1);


	//Switch to the scheduler context to run new threads
	setcontext(&scheduler_context);
};


/* Wait for thread termination */
int worker_join(worker_t thread, void **value_ptr) {
	
	// - wait for a specific thread to terminate
	// - de-allocate any dynamic memory created by the joining thread
  
	// YOUR CODE HERE

	// The exit array will be 1 if thread has exited, we need to wait
	// until the thread exited to perform a thread_join.
	while (thread_exit_status[thread] == 0) {

	}

	// If the value provided by argument is not null, populate the exit status.

	if (value_ptr != NULL){
		*value_ptr = thread_exit_value[thread];
	}

	return 0;
};

/* initialize the mutex lock */
int worker_mutex_init(worker_mutex_t *mutex, 
                          const pthread_mutexattr_t *mutexattr) {
	//- initialize data structures for this mutex

	// YOUR CODE HERE

	//NULL check for passed parameter of mutex.
	// Mutex is allocated memory at user space and given 
	// a pointer to same
	if (mutex == NULL) {
		return -1;
	}

	mutex->flag = 0;

	return 0;
};

/* acquire the mutex lock */
int worker_mutex_lock(worker_mutex_t *mutex) {

        // - use the built-in test-and-set atomic function to test the mutex
        // - if the mutex is acquired successfully, enter the critical section
        // - if acquiring mutex fails, push current thread into block list and
        // context switch to the scheduler thread

        // YOUR CODE HERE

		if(!(__atomic_test_and_set(&(mutex->flag), 1) == 0))
		{
			// This path hits if mutex acquire failed.
			// We need to add this thread to the blocked list in this case.

			(running_thread->tcb_block)->status = BLOCKED;
			insert_queue(running_thread, blocked_thread_list);

			// To know if a thread that is waiting on the mutex is blocked.
			// We do not schedule the thread if its blocked
			is_current_thread_blocked = 1;
			
			//saving current context and moving to scheduler
			swapcontext(&((running_thread->tcb_block)->cntx), &scheduler_context);
		}

		// If acquiring lock was successful, then assign the thread
		// pointer in mutex to this thread
		mutex->t = running_thread;

		mutex->flag = 1;

        return 0;
};

/* release the mutex lock */
int worker_mutex_unlock(worker_mutex_t *mutex) {
	// - release mutex and make it available again. 
	// - put threads in block list to run queue 
	// so that they could compete for mutex later.

	// YOUR CODE HERE

	// Update the flag
	mutex->flag = 0;

	//Update the thread pointer
	mutex->t = NULL;

	//Release whatever from blocked list to run queue
	unblock_threads();

	return 0;
};


/* destroy the mutex */
int worker_mutex_destroy(worker_mutex_t *mutex) {
	// - de-allocate dynamic memory created in worker_mutex_init

	if(mutex == NULL) {
		return -1;
	}

	//free(mutex);

	return 0;
};

/* scheduler */
static void schedule() {
	// - every time a timer interrupt occurs, your worker thread library 
	// should be context switched from a thread context to this
	// schedule() function

	// - invoke scheduling algorithms according to the policy (RR or MLFQ)

	// if (sched == RR)
	//		sched_rr();
	// else if (sched == MLFQ)
	// 		sched_mlfq();

	// YOUR CODE HERE

	//condition to print the average_response_time and turn_around_time for threads
	//it checks the isPrinted value and mlfq 3 levels and RR main queue.
	if (isPrinted == 0 && RR->head == NULL && mlfq_level_1->head == NULL && mlfq_level_2->head == NULL && mlfq_level_3->head == NULL && blocked_thread_list->head == NULL)
    {
        printTimes();
    }

// - schedule policy
#ifndef MLFQ
	// Choose RR
	sched_rr(RR);
#else 
	// Choose MLFQ
	sched_mlfq();
#endif

}

/* Round-robin (RR) scheduling algorithm */
static void sched_rr(runqueue * queue) {
	// - your own implementation of RR
	// (feel free to modify arguments and return types)

	// YOUR CODE HERE

	#ifndef MLFQ //TODO: Why is this present?

		if (running_thread != NULL && is_current_thread_blocked != 1)
		{
			(running_thread->tcb_block)-> status = READY;
			insert_queue(running_thread, RR);
		}

	#endif

	// Do scheduling if there is a job present in the queue.
	if (queue->head !=NULL) {

		// Pop the head from the scheduler, which is to be run
		node *temp = queue->head;
		// Move the head pointer to next job
		queue->head = (queue->head)-> next;

		// If only 1 job is present in the queue and it has been popped, tail must also be pointed 
		// to null implying that queue is empty.
		if (queue->head == NULL) {
			queue->tail = NULL;
		}

		//Removing the reference to next jobs that temp is pointed, so we can schedule it.
		temp->next = NULL;

		// populate the current thread to the popped job from run queue.
		running_thread = temp->thrd;
		running_thread->tcb_block->status = SCHEDULED;

		//Update the current running thread is not blocked
		is_current_thread_blocked = 0;

		// free the node, since node pointing to thread is scheduled, we can free this
		free(temp);

		//Configure the timer to expire after the time slice
		timer.it_value.tv_usec = TIMESLICE*1000;
		timer.it_value.tv_sec = 0;	
		setitimer(ITIMER_PROF, &timer, NULL);
		

		gettimeofday(&tv, NULL);
		unsigned long time = 1000000 * tv.tv_sec + tv.tv_usec;

		thread_schedule_time[running_thread->tcb_block->tid] = time;
		//printf("for thread %d schedule time is %ld\n",running_thread->tcb_block->tid, thread_start_time[running_thread->tcb_block->tid]);

		setcontext(&(running_thread->tcb_block->cntx));
	}
}

/* Preemptive MLFQ scheduling algorithm */
static void sched_mlfq() {
	// - your own implementation of MLFQ
	// (feel free to modify arguments and return types)

	// YOUR CODE HERE
	
	/*if(j == 0)
	{
		printf("MLFQ is running\n");
		j++;
	}*/

	// Check if current thread is not Null and thread is
	// not in blocked state.


	if(is_current_thread_blocked != 1 && running_thread != NULL ){

		// Retrieve the priority
		int prior = (running_thread->tcb_block)->priority;

		// Change the status to Ready
		(running_thread->tcb_block)->status = READY;

		if (did_thread_yield == 1)
		{
			//If thread yielded put it in same priority level
			if(prior == 4){
				insert_queue(running_thread, RR);
			}else if (prior == 3){	
				insert_queue(running_thread, mlfq_level_3);
			}else if (prior == 2){
				insert_queue(running_thread, mlfq_level_2);
			}else{
				insert_queue(running_thread, mlfq_level_1);
			}
			
			did_thread_yield = 0;

		}
		else
		{		
			// If the thread is not yielded, the priority should be reduced by 1.
			// Hence we need to place it in a low priority queue.
			if(prior == 4){
				(running_thread->tcb_block)->priority = 3;
				insert_queue(running_thread, mlfq_level_3);
			}else if (prior == 3){	
				(running_thread->tcb_block)->priority = 2;
				insert_queue(running_thread, mlfq_level_2);
			}else if (prior == 2){
				(running_thread->tcb_block)->priority = 1;
				insert_queue(running_thread, mlfq_level_1);
			}else{
				insert_queue(running_thread, mlfq_level_1);
			}
			
		}
	}
	
	//Perform RR on no highest priority level
	if (RR->head != NULL){
		sched_rr(RR);
	}else if (mlfq_level_3->head != NULL){
		sched_rr(mlfq_level_3);
	}else if (mlfq_level_2->head != NULL){
		sched_rr(mlfq_level_2);
	}else if (mlfq_level_1->head != NULL){
		sched_rr(mlfq_level_1);
	}
}

// Feel free to add any other functions you need

// YOUR CODE HERE


//Timer handler API, that swaps context to scheduler when timer triggers
void timer_handler(int signum)
{
	swapcontext(&((running_thread->tcb_block)->cntx), &scheduler_context);
}


//code to enqueue a thread to the end of a particular queue
void insert_queue(worker_thread *new_thread, struct runqueue *queue)
{
	node *temp = (node *) malloc(sizeof(node));
	temp->thrd = new_thread;
	temp->next = NULL;

	//Queue is not empty if tail is pointing to a node
	if(queue->tail != NULL)
	{	
		//Updating the old tail point to the new thread
		(queue->tail)->next = temp;
		//Updating the tail to the new thread
		queue->tail = temp;
	}
	else // Empty Queue
	{
		queue->head = temp;
		queue->tail = temp;
	}
}


// Helper API to free all the memory associated with a thread
void free_thread_memory(worker_thread *thread)
{
	//Free the allocated thread stack
	free(((thread->tcb_block)->cntx).uc_stack.ss_sp);

	//Free the tcb
	free(thread->tcb_block);

	//Free the allocate memory for thread pointer
	free(thread);
}


// API to remove all threads from blocked list once mutex is available
void unblock_threads() {

	//Remove each thread from blocked list and put it on run queue.
	//Free the node in blocked list as well
	node *cur = blocked_thread_list->head;
	node *prev = blocked_thread_list->head;
	
	while (cur != NULL)
	{
		cur->thrd->tcb_block->status = READY;
		#ifndef MLFQ
			// Add to the RR queue
			insert_queue(cur->thrd, RR);
		#else
			int prior = cur->thrd->tcb_block-> priority;
					
			if(prior == 1){
				insert_queue(cur->thrd, mlfq_level_1);
			}else if (prior == 2){
				insert_queue(cur->thrd, mlfq_level_2);
			}else if (prior == 3){
				insert_queue(cur->thrd, mlfq_level_3);
			}else{
				insert_queue(cur->thrd, RR);
			}	

		#endif

		cur = cur->next;
		// Free the node as the node that holds the pointer to thread is no longer
		// required in blocked list.
		free(prev);
		//Move the previosu pointer
		prev = cur;
	}

	// Make blocked list empty as all nodes are removed
	blocked_thread_list->head = NULL;
	blocked_thread_list->tail = NULL;

}

void printTimes()
{

    // long int avgTime = rTime / val - 1;
    isPrinted = 1;
    // printf("VAL %d \n", val);
	//printing the average response time and turn-around time.

    printf("Average Response Time in micro seconds %lld \n", response_time / (id - 2));
    printf("Average Turnaround Time in micro seconds %lld \n", turnaround_time / (id - 2));
}
