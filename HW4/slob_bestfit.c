/*
 * 	CS444 HW 4: THE SLOB SLAB
 * 	By Jonathan and Kyle
 *
 *	Method: Pair Programming
 *
 *	Resources:
 *	
 *
 * 	SLOB BEST FIT ALGORITHM
 *
 */

#include <linux/kernel.h>
#include <linux/slab.h>

#include <linux/mm.h>
#include <linux/swap.h> /* struct reclaim_state */
#include <linux/cache.h>
#include <linux/init.h>
#include <linux/export.h>
#include <linux/rcupdate.h>
#include <linux/list.h>
#include <linux/kmemleak.h>

#include <trace/events/kmem.h>

#include <linux/atomic.h>

#include "slab.h"

/*
 * slob_block has a field 'units', which indicates size of block if +ve,
 * or offset of next block if -ve (in SLOB_UNITs).
 *
 * Free blocks of size 1 unit simply contain the offset of the next block.
 * Those with larger size contain their size in the first SLOB_UNIT of
 * memory, and the offset of the next free block in the second SLOB_UNIT.
 */

#if PAGE_SIZE <= (32767 * 2)
typedef s16 slobidx_t;
#else
typedef s32 slobidx_t;
#endif

// group29's code
int claimedMemory = 0;
int usedMemory = 0;

struct slob_block {
	slobidx_t units;
};
typedef struct slob_block slob_t;

/*
 * All partially free slob pages go on these lists.
 */
#define SLOB_BREAK1 256
#define SLOB_BREAK2 1024
static LIST_HEAD(free_slob_small);
static LIST_HEAD(free_slob_medium);
static LIST_HEAD(free_slob_large);

/*
 * slob_page_free: true for pages on free_slob_pages list.
 */
static inline int slob_page_free(struct page *sp)
{
	return PageSlobFree(sp);
}

static void set_slob_page_free(struct page *sp, struct list_head *list)
{
	list_add(&sp->list, list);
	__SetPageSlobFree(sp);
}

static inline void clear_slob_page_free(struct page *sp)
{
	list_del(&sp->list);
	__ClearPageSlobFree(sp);
}

#define SLOB_UNIT sizeof(slob_t)
#define SLOB_UNITS(size) DIV_ROUND_UP(size, SLOB_UNIT)

/*
 * struct slob_rcu is inserted at the tail of allocated slob blocks, which
 * were created with a SLAB_DESTROY_BY_RCU slab. slob_rcu is used to free
 * the block using call_rcu.
 */
struct slob_rcu {
	struct rcu_head head;
	int size;
};

/*
 * slob_lock protects all slob allocator structures.
 */
static DEFINE_SPINLOCK(slob_lock);

/*
 * Encode the given size and next info into a free slob block s.
 */
static void set_slob(slob_t *s, slobidx_t size, slob_t *next)
{
	slob_t *base = (slob_t *)((unsigned long)s & PAGE_MASK);
	slobidx_t offset = next - base;

	if (size > 1) {
		s[0].units = size;
		s[1].units = offset;
	} else
		s[0].units = -offset;
}

/*
 * Return the size of a slob block.
 */
static slobidx_t slob_units(slob_t *s)
{
	if (s->units > 0)
		return s->units;
	return 1;
}

/*
 * Return the next free slob block pointer after this one.
 */
static slob_t *slob_next(slob_t *s)
{
	slob_t *base = (slob_t *)((unsigned long)s & PAGE_MASK);
	slobidx_t next;

	if (s[0].units < 0)
		next = -s[0].units;
	else
		next = s[1].units;
	return base+next;
}

/*
 * Returns true if s is the last free block in its page.
 */
static int slob_last(slob_t *s)
{
	return !((unsigned long)slob_next(s) & ~PAGE_MASK);
}

static void *slob_new_pages(gfp_t gfp, int order, int node)
{
	void *page;

#ifdef CONFIG_NUMA
	if (node != NUMA_NO_NODE)
		page = alloc_pages_exact_node(node, gfp, order);
	else
#endif
		page = alloc_pages(gfp, order);

	if (!page)
		return NULL;

	return page_address(page);
}

static void slob_free_pages(void *b, int order)
{
	if (current->reclaim_state)
		current->reclaim_state->reclaimed_slab += 1 << order;
	free_pages((unsigned long)b, order);
}

/*
 * Allocate a slob block within a given slob_page sp.
 */
static void *slob_page_alloc(struct page *sp, size_t size, int align)
{
	slob_t *prev, *cur, *aligned = NULL;
	int delta = 0, units = SLOB_UNITS(size);

	for (prev = NULL, cur = sp->freelist; ; prev = cur, cur = slob_next(cur)) {
		slobidx_t avail = slob_units(cur);

		if (align) {
			aligned = (slob_t *)ALIGN((unsigned long)cur, align);
			delta = aligned - cur;
		}
		if (avail >= units + delta) { /* room enough? */
			slob_t *next;

			if (delta) { /* need to fragment head to align? */
				next = slob_next(cur);
				set_slob(aligned, avail - delta, next);
				set_slob(cur, delta, aligned);
				prev = cur;
				cur = aligned;
				avail = slob_units(cur);
			}

			next = slob_next(cur);
			if (avail == units) { /* exact fit? unlink. */
				if (prev)
					set_slob(prev, slob_units(prev), next);
				else
					sp->freelist = next;
			} else { /* fragment */
				if (prev)
					set_slob(prev, slob_units(prev), cur + units);
				else
					sp->freelist = cur + units;
				set_slob(cur + units, avail - units, next);
			}

			sp->units -= units;
			if (!sp->units)
				clear_slob_page_free(sp);
			return cur;
		}
		if (slob_last(cur))
			return NULL;
	}
}

/*
 * slob_alloc: entry point into the slob allocator.
 */
static void *slob_alloc(size_t size, gfp_t gfp, int align, int node)
{
	struct page *sp;
	struct list_head *prev;
	struct list_head *slob_list;
	slob_t *b = NULL;
	unsigned long flags;

	if (size < SLOB_BREAK1)
		slob_list = &free_slob_small;
	else if (size < SLOB_BREAK2)
		slob_list = &free_slob_medium;
	else
		slob_list = &free_slob_large;

	spin_lock_irqsave(&slob_lock, flags);
	/* Iterate through each partially free page, try to find room */
	list_for_each_entry(sp, slob_list, list) {
#ifdef CONFIG_NUMA
		/*
		 * If there's a node specification, search for a partial
		 * page with a matching node id in the freelist.
		 */
		if (node != NUMA_NO_NODE && page_to_nid(sp) != node)
			continue;
#endif
		/* Enough room on this page? */
		if (sp->units < SLOB_UNITS(size))
			continue;

		/* Attempt to alloc */
		prev = sp->list.prev;
		b = slob_page_alloc(sp, size, align);
	
		// group 29's code
		if (!b){
			continue;
		} else {
			usedMemory = usedMemory + size;
		}

		/* Improve fragment distribution and reduce our average
		 * search time by starting our next search here. (see
		 * Knuth vol 1, sec 2.5, pg 449) */
		if (prev != slob_list->prev &&
				slob_list->next != prev->next)
			list_move_tail(slob_list, prev->next);
		break;
	}
	spin_unlock_irqrestore(&slob_lock, flags);

	/* Not enough space: must allocate a new page */
	if (!b) {
		b = slob_new_pages(gfp & ~__GFP_ZERO, 0, node);
		if (!b)
			return NULL;
		sp = virt_to_page(b);
		__SetPageSlab(sp);

		spin_lock_irqsave(&slob_lock, flags);
		sp->units = SLOB_UNITS(PAGE_SIZE);
		sp->freelist = b;
		INIT_LIST_HEAD(&sp->list);
		set_slob(b, SLOB_UNITS(PAGE_SIZE), b + SLOB_UNITS(PAGE_SIZE));
		set_slob_page_free(sp, slob_list);
		b = slob_page_alloc(sp, size, align);
		BUG_ON(!b);
		spin_unlock_irqrestore(&slob_lock, flags);
		// group 29's code
		claimedMemory = claimedMemory + PAGE_SIZE;
	}
	if (unlikely((gfp & __GFP_ZERO) && b))
		memset(b, 0, size);
	return b;
}

/*
 * slob_free: entry point into the slob allocator.
 */
static void slob_free(void *block, int size)
{
	struct page *sp;
	slob_t *prev, *next, *b = (slob_t *)block;
	slobidx_t units;
	unsigned long flags;
	struct list_head *slob_list;

	if (unlikely(ZERO_OR_NULL_PTR(block)))
		return;
	BUG_ON(!size);

	sp = virt_to_page(block);
	units = SLOB_UNITS(size);

	// group 29's code
	usedMemory = usedMemory - size;

	spin_lock_irqsave(&slob_lock, flags);

	if (sp->units + units == SLOB_UNITS(PAGE_SIZE)) {

		// group 29's code
		claimedMemory = claimedMemory + PAGE_SIZE;		

		/* Go directly to page allocator. Do not pass slob allocator */
		if (slob_page_free(sp))
			clear_slob_page_free(sp);
		spin_unlock_irqrestore(&slob_lock, flags);
		__ClearPageSlab(sp);
		page_mapcount_reset(sp);
		slob_free_pages(b, 0);
		return;
	}

	if (!slob_page_free(sp)) {
		/* This slob page is about to become partially free. Easy! */
		sp->units = units;
		sp->freelist = b;
		set_slob(b, units,
			(void *)((unsigned long)(b +
					SLOB_UNITS(PAGE_SIZE)) & PAGE_MASK));
		if (size < SLOB_BREAK1)
			slob_list = &free_slob_small;
		else if (size < SLOB_BREAK2)
			slob_list = &free_slob_medium;
		else
			slob_list = &free_slob_large;
		set_slob_page_free(sp, slob_list);
		goto out;
	}

	/*
	 * Otherwise the page is already partially free, so find reinsertion
	 * point.
	 */
	sp->units += units;

	if (b < (slob_t *)sp->freelist) {
		if (b + units == sp->freelist) {
			units += slob_units(sp->freelist);
			sp->freelist = slob_next(sp->freelist);
		}
		set_slob(b, units, sp->freelist);
		sp->freelist = b;
	} else {
		prev = sp->freelist;
		next = slob_next(prev);
		while (b > next) {
			prev = next;
			next = slob_next(prev);
		}

		if (!slob_last(prev) && b + units == next) {
			units += slob_units(next);
			set_slob(b, units, slob_next(next));
		} else
			set_slob(b, units, next);

		if (prev + slob_units(prev) == b) {
			units = slob_units(b) + slob_units(prev);
			set_slob(prev, units, slob_next(b));
		} else
			set_slob(prev, slob_units(prev), b);
	}
out:
	spin_unlock_irqrestore(&slob_lock, flags);
}

/*
 * End of slob allocator proper. Begin kmem_cache_alloc and kmalloc frontend.
 */

static __always_inline void *
__do_kmalloc_node(size_t size, gfp_t gfp, int node, unsigned long caller)
{
	unsigned int *m;
	int align = max_t(size_t, ARCH_KMALLOC_MINALIGN, ARCH_SLAB_MINALIGN);
	void *ret;

	gfp &= gfp_allowed_mask;

	lockdep_trace_alloc(gfp);

	if (size < PAGE_SIZE - align) {
		if (!size)
			return ZERO_SIZE_PTR;

		m = slob_alloc(size + align, gfp, align, node);

		if (!m)
			return NULL;
		*m = size;
		ret = (void *)m + align;

		trace_kmalloc_node(caller, ret,
				   size, size + align, gfp, node);
	} else {
		unsigned int order = get_order(size);

		if (likely(order))
			gfp |= __GFP_COMP;
		ret = slob_new_pages(gfp, order, node);

		trace_kmalloc_node(caller, ret,
				   size, PAGE_SIZE << order, gfp, node);
	}

	kmemleak_alloc(ret, size, 1, gfp);
	return ret;
}

void *__kmalloc(size_t size, gfp_t gfp)
{
	return __do_kmalloc_node(size, gfp, NUMA_NO_NODE, _RET_IP_);
}
EXPORT_SYMBOL(__kmalloc);

#ifdef CONFIG_TRACING
void *__kmalloc_track_caller(size_t size, gfp_t gfp, unsigned long caller)
{
	return __do_kmalloc_node(size, gfp, NUMA_NO_NODE, caller);
}

#ifdef CONFIG_NUMA
void *__kmalloc_node_track_caller(size_t size, gfp_t gfp,
					int node, unsigned long caller)
{
	return __do_kmalloc_node(size, gfp, node, caller);
}
#endif
#endif

void kfree(const void *block)
{
	struct page *sp;

	trace_kfree(_RET_IP_, block);

	if (unlikely(ZERO_OR_NULL_PTR(block)))
		return;
	kmemleak_free(block);

	sp = virt_to_page(block);
	if (PageSlab(sp)) {
		int align = max_t(size_t, ARCH_KMALLOC_MINALIGN, ARCH_SLAB_MINALIGN);
		unsigned int *m = (unsigned int *)(block - align);
		slob_free(m, *m + align);
	} else
		__free_pages(sp, compound_order(sp));
}
EXPORT_SYMBOL(kfree);

/* can't use ksize for kmem_cache_alloc memory, only kmalloc */
size_t ksize(const void *block)
{
	struct page *sp;
	int align;
	unsigned int *m;

	BUG_ON(!block);
	if (unlikely(block == ZERO_SIZE_PTR))
		return 0;

	sp = virt_to_page(block);
	if (unlikely(!PageSlab(sp)))
		return PAGE_SIZE << compound_order(sp);

	align = max_t(size_t, ARCH_KMALLOC_MINALIGN, ARCH_SLAB_MINALIGN);
	m = (unsigned int *)(block - align);
	return SLOB_UNITS(*m) * SLOB_UNIT;
}
EXPORT_SYMBOL(ksize);

int __kmem_cache_create(struct kmem_cache *c, unsigned long flags)
{
	if (flags & SLAB_DESTROY_BY_RCU) {
		/* leave room for rcu footer at the end of object */
		c->size += sizeof(struct slob_rcu);
	}
	c->flags = flags;
	return 0;
}

void *slob_alloc_node(struct kmem_cache *c, gfp_t flags, int node)
{
	void *b;

	flags &= gfp_allowed_mask;

	lockdep_trace_alloc(flags);

	if (c->size < PAGE_SIZE) {
		b = slob_alloc(c->size, flags, c->align, node);
		trace_kmem_cache_alloc_node(_RET_IP_, b, c->object_size,
					    SLOB_UNITS(c->size) * SLOB_UNIT,
					    flags, node);
	} else {
		b = slob_new_pages(flags, get_order(c->size), node);
		trace_kmem_cache_alloc_node(_RET_IP_, b, c->object_size,
					    PAGE_SIZE << get_order(c->size),
					    flags, node);
	}

	if (b && c->ctor)
		c->ctor(b);

	kmemleak_alloc_recursive(b, c->size, 1, c->flags, flags);
	return b;
}
EXPORT_SYMBOL(slob_alloc_node);

void *kmem_cache_alloc(struct kmem_cache *cachep, gfp_t flags)
{
	return slob_alloc_node(cachep, flags, NUMA_NO_NODE);
}
EXPORT_SYMBOL(kmem_cache_alloc);

#ifdef CONFIG_NUMA
void *__kmalloc_node(size_t size, gfp_t gfp, int node)
{
	return __do_kmalloc_node(size, gfp, node, _RET_IP_);
}
EXPORT_SYMBOL(__kmalloc_node);

void *kmem_cache_alloc_node(struct kmem_cache *cachep, gfp_t gfp, int node)
{
	return slob_alloc_node(cachep, gfp, node);
}
EXPORT_SYMBOL(kmem_cache_alloc_node);
#endif

static void __kmem_cache_free(void *b, int size)
{
	if (size < PAGE_SIZE)
		slob_free(b, size);
	else
		slob_free_pages(b, get_order(size));
}

static void kmem_rcu_free(struct rcu_head *head)
{
	struct slob_rcu *slob_rcu = (struct slob_rcu *)head;
	void *b = (void *)slob_rcu - (slob_rcu->size - sizeof(struct slob_rcu));

	__kmem_cache_free(b, slob_rcu->size);
}

void kmem_cache_free(struct kmem_cache *c, void *b)
{
	kmemleak_free_recursive(b, c->flags);
	if (unlikely(c->flags & SLAB_DESTROY_BY_RCU)) {
		struct slob_rcu *slob_rcu;
		slob_rcu = b + (c->size - sizeof(struct slob_rcu));
		slob_rcu->size = c->size;
		call_rcu(&slob_rcu->head, kmem_rcu_free);
	} else {
		__kmem_cache_free(b, c->size);
	}

	trace_kmem_cache_free(_RET_IP_, b);
}
EXPORT_SYMBOL(kmem_cache_free);

int __kmem_cache_shutdown(struct kmem_cache *c)
{
	/* No way to check for remaining objects */
	return 0;
}

int kmem_cache_shrink(struct kmem_cache *d)
{
	return 0;
}
EXPORT_SYMBOL(kmem_cache_shrink);

struct kmem_cache kmem_cache_boot = {
	.name = "kmem_cache",
	.size = sizeof(struct kmem_cache),
	.flags = SLAB_PANIC,
	.align = ARCH_KMALLOC_MINALIGN,
};

void __init kmem_cache_init(void)
{
	kmem_cache = &kmem_cache_boot;
	slab_state = UP;
}

void __init kmem_cache_init_late(void)
{
	slab_state = FULL;
}

// ************* Write separate system calls at the bottom of mm/slob.c 
// to return each of these
// amounts in bytes and as an unsigned int *********

// Comparing the values returned by each of these functions will give 
// you some idea of the degree of fragmentation suffered by SLOB. 
// Write a simple program that uses these two syscalls to make this comparison.

// asmlinkage: tells your compiler to look on the CPU stack for 
// the function parameters, instead of registers.

asmlinkage long sys_get_slob_amt_claimed(void){
	return claimedMemory;
}

asmlinkage long sys_get_slob_amt_free(void){
	return claimedMemory - usedMemory;
}
