// File:	mypthread.c

// List all group member's name: Alan Luo and Patrick Meng
// username of iLab: afl59
// iLab Server:

#include <errno.h>
#include <sys/time.h>
#include <unistd.h>
#include <signal.h>
#include <stdio.h>
#include "mypthread.h"

#ifdef DEBUG
#define debug(...) \
  fprintf(stderr, __VA_ARGS__);
#else
  #define debug(...) \
  {}
#endif

void mypthread_timer_block(void);
void mypthread_timer_unblock(void);
static void schedule();

const uint MYPTHREAD_MAX_THREAD_ID  = 50000;
const uint MYPTHREAD_TIMER_INTERVAL = 15000;
const uint MYPTHREAD_STACK_SIZE     = 8388608;

uint mypthread_init_flag = 1;
uint mypthread_id = 0;

queue_t *ready, *completed;
tcb* tcb_curr;

void queue_push(queue_t* q, tcb* data) {
	qnode_t* node = (qnode_t*)calloc(1, sizeof(qnode_t));
	if (node == NULL) {
		perror("mem");
		abort();
	}
	q->size++;
	node->next = NULL;
	node->data = data;
	if (q->head == NULL) {
		q->head = node;
		return;
    	}
	qnode_t* curr = q->head;
	qnode_t* prev = curr;
	while(curr != NULL && curr->data->age <= node->data->age) {
		prev = curr;
		 curr = curr->next;
	}
	if (curr == q->head) {
		// insert as head
		q->head = node;
	} else {
		prev->next = node;
	}
	node->next = curr;
}

int queue_is_empty(queue_t* q) {
	if (q->head == NULL) return 1;
	return 0;
}

int queue_is_member(queue_t* q, mypthread_t tid) {
	if (q->head == NULL) return 0;
	tcb* data = NULL;
	qnode_t* curr = q->head;
	qnode_t* prev = curr;
	while(curr != NULL && curr->data->tid != tid) {
		prev = curr;
		curr = curr->next;
	}
	if (curr != NULL) {
		return 1;
	}

	return 0;
}

tcb* queue_pop(queue_t* q) {
	if (q == NULL) return NULL;
	if (q->head == NULL) return NULL;
	q->size--;
	qnode_t* node = q->head;
	q->head = q->head->next;
	tcb* data = node->data;
	free(node);
	return data;
}

tcb* queue_remove(queue_t* q, mypthread_t tid) {
	if (q->head == NULL) return NULL;
	tcb* data = NULL;
	qnode_t* curr = q->head;
	qnode_t* prev = curr;
	while(curr != NULL && curr->data->tid != tid) {
		prev = curr;
		curr = curr->next;
	}
	if (curr != NULL) {
		q->size--;
		// curr is found
		data = curr->data;
		if (prev == curr) {
			// found is the head
			q->head = curr->next;
		} else {
			// found is NOT the head
			prev->next = curr->next;
		}
		free(curr);
	}

	return data;
}

// ** SIGNAL BLOCKING AND HANDLING STUFF **

sigset_t sigprof_set;
void mypthread_timer_block(void) {
	if (sigprocmask(SIG_BLOCK, &sigprof_set, NULL) != 0) {
		perror("sigprocmask");
		abort();
	}
}

void mypthread_timer_unblock(void) {
	if (sigprocmask(SIG_UNBLOCK, &sigprof_set, NULL) != 0) {
		perror("sigprocmask");
		abort();
	}
}

void mypthread_timer_handler(int signum, siginfo_t *info, void *context) {
	mypthread_timer_block();
	schedule();
}

// Register both the signal handler and the actual timer itself
void mypthread_timer_init(void) {
	sigemptyset(&sigprof_set);
	sigaddset(&sigprof_set, SIGPROF);

	struct sigaction mypthread_timer_action;
	sigset_t sigset;
	sigfillset(&sigset);
	mypthread_timer_action.sa_mask = sigset;
	mypthread_timer_action.sa_sigaction = mypthread_timer_handler;
	mypthread_timer_action.sa_flags = SA_SIGINFO | SA_RESTART;
	if (sigaction(SIGPROF, &mypthread_timer_action, NULL) != 0) {
		perror("sigaction");
		abort();
	}

	struct itimerval mypthread_timer;
	mypthread_timer.it_value.tv_sec = 0;
	mypthread_timer.it_value.tv_usec = MYPTHREAD_TIMER_INTERVAL;
	mypthread_timer.it_interval.tv_sec = 0;
	mypthread_timer.it_interval.tv_usec = 0;
	mypthread_timer.it_interval = mypthread_timer.it_value;
	if (setitimer(ITIMER_PROF, &mypthread_timer, NULL) != 0) {
		perror("sigaction");
		abort();
	}
}

// We need this in order to reset our timer after a context swap
void mypthread_timer_reset(void) {
	struct itimerval mypthread_timer;
	mypthread_timer.it_value.tv_sec = 0;
	mypthread_timer.it_value.tv_usec = MYPTHREAD_TIMER_INTERVAL;
	mypthread_timer.it_interval.tv_sec = 0;
	mypthread_timer.it_interval.tv_usec = 0;
	mypthread_timer.it_interval = mypthread_timer.it_value;
	if (setitimer(ITIMER_PROF, &mypthread_timer, NULL) != 0) {
		perror("sigaction");
		abort();
	}
}

// Basic thread init function
void mypthread_init(void) {
	ready = (queue_t*)calloc(1, sizeof(queue_t));
	completed = (queue_t*)calloc(1, sizeof(queue_t));

	tcb_curr = calloc(1, sizeof(tcb));
	tcb_curr->tid = mypthread_id++;
	tcb_curr->status = 0; // 0 ready, -1 completed
	tcb_curr->age = 0;
	getcontext(&tcb_curr->context);
	mypthread_timer_init();
}

// We need this function wrapper so we have a way to guarantee exiting at the end of a
// function's runtime
void mypthread_func_wrapper(void) {
	mypthread_timer_block();
	tcb* tcb_now = tcb_curr;
	mypthread_timer_unblock();
	void *retval = tcb_now->func(tcb_now->arg);
	mypthread_exit(retval);
}

/* create a new thread */
int mypthread_create(mypthread_t * thread, pthread_attr_t * attr,
                      void *(*function)(void*), void * arg) {
	if (mypthread_init_flag) {
		mypthread_init_flag = 0;
		mypthread_init();
	}

	// create & init tcb for new thread
	tcb* tcb_new;
	tcb_new = calloc(1, sizeof(tcb));
	if (tcb_new == NULL) {
		perror("tcb mem");
		abort();
	}
	tcb_new->status = 0;  // 0 ready, -1 completed
	tcb_new->age = 0;
	tcb_new->func = function;
	tcb_new->arg = arg;
	// create * init conext for new thread
	if (getcontext(&tcb_new->context) != 0) {
		perror("getcontext");
		abort();
	}
	tcb_new->context.uc_stack.ss_flags = 0;
  	tcb_new->context.uc_stack.ss_size = MYPTHREAD_STACK_SIZE;

	void* stack = malloc(MYPTHREAD_STACK_SIZE);
	if (stack == NULL) {
		perror("tcb stack mem");
		abort();
	}
  	tcb_new->context.uc_stack.ss_sp = stack;
	makecontext(&tcb_new->context, mypthread_func_wrapper, 1, tcb_new->tid);

	// add new thread to ready queue
	mypthread_timer_block();
	if (mypthread_id >= MYPTHREAD_MAX_THREAD_ID) {
		free(tcb_new->context.uc_stack.ss_sp);
		free(tcb_new);
		mypthread_timer_unblock();
		return EAGAIN;
	}

	tcb_new->tid = mypthread_id++;
  	queue_push(ready, tcb_new);
	*thread = tcb_new->tid;
	debug("create thread: %d\n", tcb_new->tid);

	mypthread_timer_unblock();
  	return 0;
};

/* give CPU possession to other user-level threads voluntarily */
int mypthread_yield() {
  	mypthread_timer_block();
	schedule();
	return 0;
};

/* terminate a thread */
void mypthread_exit(void *value_ptr) {
	// mark current thread as completed
  	mypthread_timer_block();
	debug("exit thread %d\n", tcb_curr->tid);
	tcb_curr->status = -1;
	tcb_curr->retval = value_ptr;
	if (queue_is_empty(ready)) {
	// last thread, terminate the process
		exit(0);
	}
	schedule();
};


/* Wait for thread termination */
int mypthread_join(mypthread_t thread, void **value_ptr) {
	while(1) {
		mypthread_timer_block();
		tcb* tcb_found = queue_remove(completed, thread);
		if (tcb_found != NULL) {
			debug("join found thread %d\n", tcb_found->tid);
			if (value_ptr != NULL)
				*value_ptr = tcb_found->retval;
			mypthread_timer_unblock();
			return thread;
		} else {
			schedule();
		}
	}

	mypthread_timer_unblock();
	return 0;
};

/* initialize the mutex lock */
int mypthread_mutex_init(mypthread_mutex_t *mutex,
                          const pthread_mutexattr_t *mutexattr) {
	atomic_flag_clear(&(mutex->flag));
	mutex->owner = -1;
	mutex->blocked = (queue_t*) calloc(1, sizeof(queue_t));
	mutex->status = 1; // 1 - initialized
	return 0;
};

/* aquire the mutex lock */
int mypthread_mutex_lock(mypthread_mutex_t *mutex) {
	if (mutex->status != 1) return EBUSY;
	if (tcb_curr->tid == mutex->owner) return 0; // only thread owning the lock can change the owner
	while (atomic_flag_test_and_set(&(mutex->flag))) {
		mypthread_timer_block(); // critical section
		tcb_curr->status = 1;  // set tcb status as blocked
		queue_push(mutex->blocked, tcb_curr);  // add it into per mutex blocked queue
		debug("thread %d failed to lock mutex and yield\n", tcb_curr->tid);
		schedule(); // Yield to next
	};
	// debug("thread %d locked mutex\n", tcb_curr->tid);
	mutex->owner = tcb_curr->tid;
  	return 0;
};

/* release the mutex lock */
int mypthread_mutex_unlock(mypthread_mutex_t *mutex) {
	if (mutex->owner == tcb_curr->tid) {
		mutex->owner = -1;
		atomic_flag_clear(&(mutex->flag));
		tcb* tcb_blocked = NULL;

		if(queue_is_empty(mutex->blocked) == 0) {
			// Optimization to avoid calling timer_block for each mutex_unlock
			// At this point, the mutexx is unlocked
			// If there another thread waiting between stmt and timer_block
			// Then another thread must've locked the mutex already
			// It must be free for the blocked thread
			mypthread_timer_block();
			if((tcb_blocked=queue_pop(mutex->blocked)) != NULL) {
				tcb_blocked->status = 0;
				queue_push(ready, tcb_blocked);
				debug("Thread %d changed from blocked to ready\n", tcb_blocked->tid);
			}
			mypthread_timer_unblock();
		}
		return 0;
	}
	return EBUSY;
};


/* destroy the mutex */
int mypthread_mutex_destroy(mypthread_mutex_t *mutex) {
	if (mutex->status != 1) return EBUSY;
	mutex->status = 0;
	if (tcb_curr->tid != mutex->owner) {
		while (atomic_flag_test_and_set(&(mutex->flag))) {
			debug("thread %d failed to lock mutex and yield\n", tcb_curr->tid);
			mypthread_yield();
		};
		mutex->owner = -1;
		atomic_flag_clear(&(mutex->flag));
	} else {
		mutex->owner = -1;
		atomic_flag_clear(&(mutex->flag));
	}
	return 0;
};

int mypthread_context_switch(void) {

}

/* Preemptive SJF (STCF) scheduling algorithm */
static void sched_stcf() {
  	// mypthread_timer_block();
	if (queue_is_empty(ready)) {
  	mypthread_timer_unblock();
		// debug("schedule stay on\n");
		return;
	}

	tcb* tcb_saved = tcb_curr;
	tcb_saved->age++;
	if (tcb_saved->status == 0)
  	queue_push(ready, tcb_saved);
	if (tcb_saved->status == -1)
  	queue_push(completed, tcb_saved);
  	tcb_curr = queue_pop(ready);
	debug("schedule from thread %d to thread %d\n", tcb_saved->tid, tcb_curr->tid);
	mypthread_timer_reset();
	swapcontext(&tcb_saved->context, &tcb_curr->context);
}

/* Preemptive MLFQ scheduling algorithm */
static void sched_mlfq() {
	// Your own implementation of MLFQ
	// (feel free to modify arguments and return types)

	// YOUR CODE HERE
}

/* scheduler */
static void schedule() {
	// Every time when timer interrup happens, your thread library
	// should be contexted switched from thread context to this
	// schedule function

	// Invoke different actual scheduling algorithms
	// according to policy (STCF or MLFQ)

	// if (sched == STCF)
	//		sched_stcf();
	// else if (sched == MLFQ)
	// 		sched_mlfq();

	// YOUR CODE HERE

// schedule policy
#ifndef MLFQ
	// Choose STCF
	sched_stcf();
#else
	// Choose MLFQ
#endif

}
