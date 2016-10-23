/*
* elevator sstf
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

static int greater(void *priv, struct list_head *a, struct list_head *b)
{
	struct request *rqa = container_of(a, struct request, queuelist);
	struct request *rqb = container_of(b, struct request, queuelist);

	int a = blk_rq_pos(rqa);
	int b = blk_rq_pos(rqb);

	return a - b;
}

static int less(void *priv, struct list_head *a, struct list_head *b)
{
	struct request *rqa = container_of(a, struct request, queuelist);
	struct request *rqb = container_of(b, struct request, queuelist);

	int a = blk_rq_pos(rqa);
	int b = blk_rq_pos(rqb);

	return b - a;
}

static void sstf_merged_requests(struct request_queue *q, struct request *rq,
				 struct request *next)
{
	list_del_init(&next->queuelist);
}

static void _sstf_sort_ascend(struct request_queue *q, struct request *rq, struct request *next)
{
	struct sstf_data *nd = q->elevator->elevator_data;

	//List is empty, no sort is needed.
	if(list_empty(&nd->queue))
		return;
	list_sort(NULL,&nd->queue,greater);
}

static void _sstf_sort_decsend(struct request_queue *q, struct request *rq, struct request *next)
{
	struct sstf_data *nd = q->elevator->elevator_data;

	//List is empty, no sort is needed.
	if(list_empty(&nd->queue))
		return;
	list_sort(NULL, &nd->queue,less);
}

static void _make_lists(struct request_queue *q, struct request *rq, struct request *next)
{
	struct sstf_data *nd = q->elevator->elevator_data;
	struct sstf_data *tmp_less;
	struct sstf_data *tmp_greater;

	struct list_head *cursor;
	
	struct request *itr;

	if(list_empty(&nd->queue))
		return;
	struct request *rq = list_entry(nd->queue.next, struct request, queuelist);

	sector_t curr_pos = blk_rq_pos(rq);
	sector_t itr_pos;
	printk("Init Head Pos: %llx \n",curr_pos);

	list_for_each(cursor, &nd->queue)
	{
		itr = list_entry(cursor, struct request, queuelist);
		itr_pos = blk_rq_pos(itr);
		
		if(itr_pos < curr_pos){
			list_add(itr,&tmp_less->queue);
			printk("Adding to lesser\n");	
		}
		else{		
			list_add(itr,&tmp_greater->queue);
			printk("Adding to greater\n");
		}
		prink("Itr Post: %llx \n", itr_pos);
	
	}
	
}

 
static int sstf_dispatch(struct request_queue *q, int force)
{
	struct sstf_data *nd = q->elevator->elevator_data;

	if (!list_empty(&nd->queue)) {
		struct request *rq;
		rq = list_entry(nd->queue.next, struct request, queuelist);
		list_del_init(&rq->queuelist);
		elv_dispatch_sort(q, rq);
		return 1;
	}
	return 0;
}

static void sstf_add_request(struct request_queue *q, struct request *rq)
{
	struct sstf_data *nd = q->elevator->elevator_data;

	list_add_tail(&rq->queuelist, &nd->queue);
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


MODULE_AUTHOR("Jens Axboe");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("No-op IO scheduler");
