/**********************************************************************
 * Copyright (c) 2019-2021
 *  Sang-Hoon Kim <sanghoonkim@ajou.ac.kr>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTIABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 **********************************************************************/

/* THIS FILE IS ALL YOURS; DO WHATEVER YOU WANT TO DO IN THIS FILE */

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#include "types.h"
#include "list_head.h"

/**
 * The process which is currently running
 */
#include "process.h"
extern struct process *current;


/**
 * List head to hold the processes ready to run
 */
extern struct list_head readyqueue;


/**
 * Resources in the system.
 */
#include "resource.h"
extern struct resource resources[NR_RESOURCES];


/**
 * Monotonically increasing ticks
 */
extern unsigned int ticks;


/**
 * Quiet mode. True if the program was started with -q option
 */
extern bool quiet;

int mark[1000] = { 0 };
int change[1000] = { 0 };
/***********************************************************************
 * Default FCFS resource acquision function
 *
 * DESCRIPTION
 *   This is the default resource acquision function which is called back
 *   whenever the current process is to acquire resource @resource_id.
 *   The current implementation serves the resource in the requesting order
 *   without considering the priority. See the comments in sched.h
 ***********************************************************************/
bool fcfs_acquire(int resource_id)
{
	struct resource *r = resources + resource_id;

	if (!r->owner) {
		/* This resource is not owned by any one. Take it! */
		r->owner = current;
		return true;
	}

	/* OK, this resource is taken by @r->owner. */

	/* Update the current process state */
	current->status = PROCESS_WAIT;

	/* And append current to waitqueue */
	list_add_tail(&current->list, &r->waitqueue);

	/**
	 * And return false to indicate the resource is not available.
	 * The scheduler framework will soon call schedule() function to
	 * schedule out current and to pick the next process to run.
	 */
	return false;
}

/***********************************************************************
 * Default FCFS resource release function
 *
 * DESCRIPTION
 *   This is the default resource release function which is called back
 *   whenever the current process is to release resource @resource_id.
 *   The current implementation serves the resource in the requesting order
 *   without considering the priority. See the comments in sched.h
 ***********************************************************************/
void fcfs_release(int resource_id)
{
	struct resource *r = resources + resource_id;

	/* Ensure that the owner process is releasing the resource */
	assert(r->owner == current);

	/* Un-own this resource */
	r->owner = NULL;

	/* Let's wake up ONE waiter (if exists) that came first */
	if (!list_empty(&r->waitqueue)) {
		struct process *waiter =
				list_first_entry(&r->waitqueue, struct process, list);

		/**
		 * Ensure the waiter is in the wait status
		 */
		assert(waiter->status == PROCESS_WAIT);

		/**
		 * Take out the waiter from the waiting queue. Note we use
		 * list_del_init() over list_del() to maintain the list head tidy
		 * (otherwise, the framework will complain on the list head
		 * when the process exits).
		 */
		list_del_init(&waiter->list);

		/* Update the process status */
		waiter->status = PROCESS_READY;

		/**
		 * Put the waiter process into ready queue. The framework will
		 * do the rest.
		 */
		list_add_tail(&waiter->list, &readyqueue);
	}
}



#include "sched.h"

/***********************************************************************
 * FIFO scheduler
 ***********************************************************************/
static int fifo_initialize(void)
{
	return 0;
}

static void fifo_finalize(void)
{
}

static struct process *fifo_schedule(void)
{
	struct process *next = NULL;

	/* You may inspect the situation by calling dump_status() at any time */
	// dump_status();

	/**
	 * When there was no process to run in the previous tick (so does
	 * in the very beginning of the simulation), there will be
	 * no @current process. In this case, pick the next without examining
	 * the current process. Also, when the current process is blocked
	 * while acquiring a resource, @current is (supposed to be) attached
	 * to the waitqueue of the corresponding resource. In this case just
	 * pick the next as well.
	 */
	if (!current || current->status == PROCESS_WAIT) {
		goto pick_next;
	}

	/* The current process has remaining lifetime. Schedule it again */
	if (current->age < current->lifespan) {
		return current;
	}

pick_next:
	/* Let's pick a new process to run next */

	if (!list_empty(&readyqueue)) {
		/**
		 * If the ready queue is not empty, pick the first process
		 * in the ready queue
		 */
		next = list_first_entry(&readyqueue, struct process, list);

		/**
		 * Detach the process from the ready queue. Note we use list_del_init()
		 * instead of list_del() to maintain the list head tidy. Otherwise,
		 * the framework will complain (assert) on process exit.
		 */
		list_del_init(&next->list);
	}

	/* Return the next process to run */
	return next;
}

struct scheduler fifo_scheduler = {
	.name = "FIFO",
	.acquire = fcfs_acquire,
	.release = fcfs_release,
	.initialize = fifo_initialize,
	.finalize = fifo_finalize,
	.schedule = fifo_schedule,
};


/***********************************************************************
 * SJF scheduler
 ***********************************************************************/

static struct process *sjf_schedule(void)
{
	/**
	 * Implement your own SJF scheduler here.
	 */
	
	struct process *next = NULL;

        if(!current || current->status == PROCESS_WAIT){
                goto pick_next;
        }

        if (current->age < current->lifespan) {
                return current;
        }

pick_next :
	if(!list_empty(&readyqueue)){

		struct process *p; //순회용 process
		
		//find minimum lifespan process
		unsigned int min_lifespan = 1000;
		list_for_each_entry(p, &readyqueue, list){
			if(p->lifespan < min_lifespan)
			{
				next = p;
				min_lifespan = p->lifespan;
			}
		}
		list_del_init(&next->list);
		return next;

	}
	return next;
}

struct scheduler sjf_scheduler = {
	.name = "Shortest-Job First",
	.acquire = fcfs_acquire, /* Use the default FCFS acquire() */
	.release = fcfs_release, /* Use the default FCFS release() */
	.schedule = sjf_schedule,		 /* TODO: Assign sjf_schedule()
								to this function pointer to activate
								SJF in the system */
};


/***********************************************************************
 * SRTF scheduler
 ***********************************************************************/
static struct process *srtf_schedule(void)
{

	struct process *next = NULL;
	
        if(!current || current->status == PROCESS_WAIT){
                goto pick_next;
        }

        if (current->age < current->lifespan) {
                return current;
        }

pick_next :
        if(!list_empty(&readyqueue)){

                struct process *p; //순회용 process

                //find minimum remain process
                unsigned int min_remain = 1000;
                list_for_each_entry(p, &readyqueue, list){
                        if(p->lifespan - p->age < min_remain)
                        {
                                next = p;
                                min_remain = p->lifespan - p->age;
                        }
                }
		
                list_del_init(&next->list);
                return next;

        }
        return next;	

}

void preemptive_remain(struct process* p)
{
	if(current != NULL){

		if(p->lifespan < current->lifespan - current->age)
		{
			current->status = PROCESS_WAIT;
			list_move_tail(&current->list, &readyqueue);
			current = p;
			list_del_init(&current->list);
		}
	}	
}

struct scheduler srtf_scheduler = {
	.name = "Shortest Remaining Time First",
	.acquire = fcfs_acquire, /* Use the default FCFS acquire() */
	.release = fcfs_release, /* Use the default FCFS release() */
	.schedule = srtf_schedule, 
	.forked = preemptive_remain,
	/* You need to check the newly created processes to implement SRTF.
	 * Use @forked() callback to mark newly created processes */
	/* Obviously, you should implement srtf_schedule() and attach it here */
};


/***********************************************************************
 * Round-robin scheduler
 ***********************************************************************/
static struct process *rr_schedule(void)
{

	struct process *next = NULL;
	
	if(!current || current->status == PROCESS_WAIT){
		goto pick_next;
	}
	
	if(current->age < current->lifespan){
		//quantum=1
		current->status = PROCESS_WAIT;
		list_move_tail(&current->list, &readyqueue);
		
		//next process!
		next = list_first_entry(&readyqueue, struct process, list);
		list_del_init(&next->list);
		return next;
	}


pick_next :
	if(!list_empty(&readyqueue)){
		next = list_first_entry(&readyqueue, struct process, list);
		
		list_del_init(&next->list);
	}

	
	return next;
}

struct scheduler rr_scheduler = {
	.name = "Round-Robin",
	.acquire = fcfs_acquire, /* Use the default FCFS acquire() */
	.release = fcfs_release, /* Use the default FCFS release() */
	/* Obviously, you should implement rr_schedule() and attach it here */
	.schedule = rr_schedule,
};


/***********************************************************************
 * Priority scheduler
 ***********************************************************************/
static struct process *prio_schedule(void)
{
	struct process *next = NULL;

	//fprintf(stderr,"tick:%d\n",ticks);
	//dump_status();
	if(!current || current->status == PROCESS_WAIT){
		goto pick_next;
	}

	if(current->age < current->lifespan){
		
		struct process *tmp;
		unsigned int max = 0;
		bool check2 = false;
		list_for_each_entry(tmp, &readyqueue, list){
			if(tmp->prio > max){
				max = tmp->prio;
				next = tmp;
				check2 = true;
			}
			if(tmp->prio == 0 && check2 == false)
			{
				next = tmp;
				max = tmp->prio;
				check2 = true;
			}
		}
		if (max == current->prio && !list_empty(&readyqueue))
		{
			current->status = PROCESS_WAIT;
			list_move_tail(&current->list, &readyqueue);

			list_del_init(&next->list);

			return next;
		}
		if(change[current->pid] == 1)
		{
			if(current->prio > next->prio)
			{
				change[current->pid] = 0;
				return current;
			}
			current->status = PROCESS_WAIT;
			list_move_tail(&current->list, &readyqueue);
			
			change[current->pid] = 0;
			list_del_init(&next->list);
			return next;
		}
		//dump_status();

		return current;
	}
	
pick_next :
	if(!list_empty(&readyqueue)){

		struct process *p;
		//dump_status();
		bool check = false;
		unsigned int max_prio = 0;
		list_for_each_entry(p, &readyqueue, list){
			if(p->prio > max_prio)
			{	
				next = p;
				max_prio = p->prio;
				check = true;
			}
			if(p->prio == 0 && check == false)
			{
				next = p;
				max_prio = p->prio;
				check = true;
			}
		}
	
		list_del_init(&next->list);
	//	dump_status();
		return next;
	}
	return next;

}

bool prio_acquire(int resource_id)
{
	struct resource *r = resources + resource_id;

	if(!r->owner){
		r->owner = current;
		return true;
	}

	current->status = PROCESS_WAIT;

	mark[current->pid] = 1;
	list_move_tail(&current->list, &r->waitqueue);

	return false;
}

void prio_release(int resource_id)
{
	struct resource *r = resources + resource_id;

	assert(r->owner == current);

	r->owner = NULL;
	
	if(!list_empty(&r->waitqueue)){

		unsigned int max_pri = 0;
		struct process *waiter, *max_waiter;
		//maximum priority
		list_for_each_entry(waiter,&r->waitqueue,list){
			if(max_pri < waiter->prio)
			{
				max_pri = waiter->prio;
				max_waiter = waiter;
			}
		}
		assert(max_waiter->status == PROCESS_WAIT);
		
		mark[max_waiter->pid] = 0;
		list_del_init(&max_waiter->list);
		max_waiter->status = PROCESS_READY;
		list_add_tail(&max_waiter->list, &readyqueue);
	}
}

void preemptive_prio(struct process *p)
{

	if (current != NULL){
		//dump_status();
		if(p->prio > current->prio)
		{
			if(mark[current->pid] == 0){
				current->status = PROCESS_WAIT;
				list_move_tail(&current->list,&readyqueue);
				
				current = p;
				list_del_init(&current->list);
			}
		}
			
	}
}

struct scheduler prio_scheduler = {
	.name = "Priority",
	.acquire = prio_acquire,
	.release = prio_release,
	.schedule = prio_schedule,
	.forked = preemptive_prio,

	/**
	 * Implement your own acqure/release function to make priority
	 * scheduler correct.
	 */
	/* Implement your own prio_schedule() and attach it here */
};


/***********************************************************************
 * Priority scheduler with aging
 ***********************************************************************/
static struct process *pa_schedule(void)
{
	struct process *next = NULL;


	if(current){
		current->prio = current->prio_orig;
		if(!list_empty(&readyqueue))
        	{
                	struct process *p;
        	        list_for_each_entry(p, &readyqueue, list){
                       		p->prio++;
                	}
        	}
	}
	//fprintf(stderr,"tick:%d\n",ticks);
	//dump_status();

	if(!current || current->status == PROCESS_WAIT){
		goto pick_next;
	}

	if(current->age < current->lifespan){
		
		struct process *tmp;
		unsigned int max = 0;
		bool check2 = false;
		list_for_each_entry(tmp, &readyqueue, list){
			if(tmp->prio > max){
				max = tmp->prio;
				next = tmp;
				check2 = true;
			}
			if(tmp->prio == 0 && check2 == false)
			{
				next = tmp;
				max = tmp->prio;
				check2 = true;
			}
		}
		if (max == current->prio && !list_empty(&readyqueue))
		{
			current->status = PROCESS_WAIT;
			list_move_tail(&current->list, &readyqueue);

			list_del_init(&next->list);
			
			return next;
		}
		if(current->prio < max)
		{
			current->status = PROCESS_WAIT;
			list_move_tail(&current->list, &readyqueue);

			list_del_init(&next->list);
			
			return next;
		}
		return current;
	}
	
pick_next :
	if(!list_empty(&readyqueue)){
		struct process *p;
		bool check = false;
		unsigned int max_prio = 0;
		list_for_each_entry(p, &readyqueue, list){
			if(p->prio > max_prio)
			{	
				next = p;
				max_prio = p->prio;
				check = true;
			}
			if(p->prio == 0 && check == false)
			{
				next = p;
				max_prio = p->prio;
				check = true;
			}
		}
		//if(current){
		//	if(current->prio > next->prio)
		//		return current;
		//}
		list_del_init(&next->list);
		return next;
	}
	return next;

}

struct scheduler pa_scheduler = {
	.name = "Priority + aging",
	.forked = preemptive_prio,
	.schedule = pa_schedule,
	/**
	 * Implement your own acqure/release function to make priority
	 * scheduler correct.
	 */
	/* Implement your own prio_schedule() and attach it here */
};


/***********************************************************************
 * Priority scheduler with priority ceiling protocol
 ***********************************************************************/

bool PCP_acquire(int resource_id)
{
	struct resource *r = resources + resource_id;

	if(!r->owner){
		r->owner = current;
		//celling
		r->owner->prio = MAX_PRIO;
		return true;
	}

	current->status = PROCESS_WAIT;
	
	mark[current->pid] = 1;
	list_add_tail(&current->list, &r->waitqueue);

	return false;
}

void PCP_release(int resource_id)
{
	struct resource *r = resources + resource_id;

	assert(r->owner == current);
	
	//fprintf(stderr,"%d %d\n",current->prio, current->prio_orig);
	current->prio = current->prio_orig;
	change[current->pid] = 1;

	r->owner = NULL;

	if(!list_empty(&r->waitqueue)){

		unsigned int max_prio = 0;
		struct process *waiter, *max_waiter;
	       
		list_for_each_entry(waiter, &r->waitqueue, list){
			if(max_prio < waiter->prio)
			{
				max_prio = waiter->prio;
				max_waiter = waiter;
			}
		}
		assert(max_waiter->status == PROCESS_WAIT);
		
		mark[max_waiter->pid] = 0;
		list_del_init(&max_waiter->list);
		max_waiter->status = PROCESS_READY;
		list_add_tail(&max_waiter->list, &readyqueue);
	
		
	}


}


struct scheduler pcp_scheduler = {
	.name = "Priority + PCP Protocol",
	.acquire = PCP_acquire,
	.release = PCP_release,
	.schedule = prio_schedule,
	.forked = preemptive_prio,
	/**
	 * Implement your own acqure/release function too to make priority
	 * scheduler correct.
	 */
};


/***********************************************************************
 * Priority scheduler with priority inheritance protocol
 ***********************************************************************/
bool PIP_acquire(int resource_id)
{
	struct resource *r = resources + resource_id;

	if(!r->owner){
		//fprintf(stderr,"%d %d\n",current->prio, current->prio_orig);
		r->owner = current;
		return true;
	}

	current->status = PROCESS_WAIT;

	list_add_tail(&current->list, &r->waitqueue);
	
	//fprintf(stderr,"%d %d \n",current->prio, current->prio_orig);
	
	//inheritance
	r->owner->prio = current->prio;

	return false;
}


struct scheduler pip_scheduler = {
	.name = "Priority + PIP Protocol",
	.schedule = prio_schedule,
	.forked = preemptive_prio,
	.acquire = PIP_acquire,
	.release = PCP_release,
	/**
	 * Ditto
	 */
};
