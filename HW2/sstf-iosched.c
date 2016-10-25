/*
 *	CS 444 HW 2 - I/O SCHEDULER
 *	By: Kyle and Jonathan 
 *
 *	Method: Pair Programming
 *
 *	Implemetation:
 *
 *	Resources:
 * 	lxr.free-electrons.com
 *	noop-iosched.c
 *	http://www.cs.iit.edu/~cs561/cs450/disksched/disksched.html
 *
 */
#include <linux/blkdev.h>
#include <linux/elevator.h>
#include <linux/bio.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/list.h>
#include <linux/list_sort.h>

struct sstf_data {
	struct list_head queue;
	sector_t position;	
};

static int less(void *priv, struct list_head *a, struct list_head *b)
{
	struct request *rqa = container_of(a, struct request, queuelist);
	struct request *rqb = container_of(b, struct request, queuelist);

	return blk_rq_pos(rqb) - blk_rq_pos(rqa);
}

static void sstf_merged_requests(struct request_queue *q, struct request *rq,
				 struct request *next)
{
	list_del_init(&next->queuelist);
}


static int sstf_dispatch(struct request_queue *q, int force)
{
	struct sstf_data *nd = q->elevator->elevator_data;

	if (!list_empty(&nd->queue)) {
		struct request *rq;
		rq = list_entry(nd->queue.next, struct request, queuelist);
		list_del_init(&rq->queuelist);
		printk("\n\nDispatching Request Number: %llu\n\n",(unsigned long long) blk_rq_pos(rq)); 
		elv_dispatch_sort(q, rq);
		return 1;
	}
	return 0;
}

static void sstf_add_request(struct request_queue *q, struct request *rq)
{
	struct sstf_data *nd = q->elevator->elevator_data;
    	struct request *nextRequest, *prevRequest;
	sector_t pos1, pos2;

	printk("--- ADD REQUEST SECTION ---\n");

	// Convert direct references of the fields to the accessors
	pos1 = blk_rq_pos(rq); //New request that arrived.

	printk("Received request: %llu \n", (unsigned long long) blk_rq_pos(rq));	

	// Check whether the list is empty or not	
	if(list_empty(&nd->queue))
    	{
        	printk("Condition: The list is currently empty.\n");
		list_add(&rq->queuelist, &nd->queue);
    	}
    	else
    	{		
		nextRequest = list_entry(nd->queue.next, struct request, queuelist); //accesss request element
		pos2 = blk_rq_pos(nextRequest); //Itr sector position
        	prevRequest = list_entry(nd->queue.prev, struct request, queuelist);
		
		while(pos1 > pos2) //while new request is greater than iterator, iterate. when new request is less than iterator, insert into queue
        	{
          		nextRequest = list_entry(nextRequest->queuelist.next, struct request, queuelist);
            		prevRequest = list_entry(prevRequest->queuelist.prev, struct request, queuelist);
			
			pos2 = blk_rq_pos(nextRequest); //update iterator sector position
        	}
		
		list_add_tail(&rq->queuelist, &nextRequest->queuelist);	//insert neq request before iterator (Descending order)
		
		printk("Condition: Added request + Inserted at correct position.\n");
	}
    	printk("Condition: Adding %llu\n", (unsigned long long) blk_rq_pos(rq));
	list_sort(NULL,&nd->queue,less);
}

static struct request *
sstf_former_request(struct request_queue *q, struct request *rq)
{
	struct sstf_data *nd = q->elevator->elevator_data;

	if (rq->queuelist.prev == &nd->queue)
		return NULL;
	return list_entry(rq->queuelist.prev, struct request, queuelist);
}

static struct request *
sstf_latter_request(struct request_queue *q, struct request *rq)
{
	struct sstf_data *nd = q->elevator->elevator_data;

	if (rq->queuelist.next == &nd->queue)
		return NULL;
	return list_entry(rq->queuelist.next, struct request, queuelist);
}

static int sstf_init_queue(struct request_queue *q, struct elevator_type *e)
{
	struct sstf_data *nd;
	struct elevator_queue *eq;

	eq = elevator_alloc(q, e);
	if (!eq)
		return -ENOMEM;

	nd = kmalloc_node(sizeof(*nd), GFP_KERNEL, q->node);
	if (!nd) {
		kobject_put(&eq->kobj);
		return -ENOMEM;
	}
	eq->elevator_data = nd;

	INIT_LIST_HEAD(&nd->queue);

	spin_lock_irq(q->queue_lock);
	q->elevator = eq;
	spin_unlock_irq(q->queue_lock);
	return 0;
}

static void sstf_exit_queue(struct elevator_queue *e)
{
	struct sstf_data *nd = e->elevator_data;

	BUG_ON(!list_empty(&nd->queue));
	kfree(nd);
}

static struct elevator_type elevator_sstf = {
	.ops = {
		.elevator_merge_req_fn		= sstf_merged_requests,
		.elevator_dispatch_fn		= sstf_dispatch,
		.elevator_add_req_fn		= sstf_add_request,
		.elevator_former_req_fn		= sstf_former_request,
		.elevator_latter_req_fn		= sstf_latter_request,
		.elevator_init_fn		= sstf_init_queue,
		.elevator_exit_fn		= sstf_exit_queue,
	},
	.elevator_name = "sstf",
	.elevator_owner = THIS_MODULE,
};

static int __init sstf_init(void)
{
	return elv_register(&elevator_sstf);
}

static void __exit sstf_exit(void)
{
	elv_unregister(&elevator_sstf);
}

module_init(sstf_init);
module_exit(sstf_exit);


MODULE_AUTHOR("Kyle & Jonathan - Group 29");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("SSTF IO scheduler");

