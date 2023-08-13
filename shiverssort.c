#include "list.h"
#include "list_sort.h"

#include <stdint.h>

#ifndef likely
#define likely(x) __builtin_expect(!!(x), 1)
#endif
#ifndef unlikely
#define unlikely(x) __builtin_expect(!!(x), 0)
#endif

#define MAX_MERGE_PENDING (sizeof(size_t) * 8) + 1

struct run {
	struct list_head *list;
	size_t len;
};

static struct list_head *merge(void *priv, list_cmp_func_t cmp,
				struct list_head *a, struct list_head *b)
{
	struct list_head *head, **tail = &head;

	for (;;) {
		/* if equal, take 'a' -- important for sort stability */
		if (cmp(priv, a, b) <= 0) {
			*tail = a;
			tail = &a->next;
			a = a->next;
			if (!a) {
				*tail = b;
				break;
			}
		} else {
			*tail = b;
			tail = &b->next;
			b = b->next;
			if (!b) {
				*tail = a;
				break;
			}
		}
	}
	return head;
}

static void build_prev_link(struct list_head *head, struct list_head *tail,
			    struct list_head *list)
{
	tail->next = list;
	do {
		list->prev = tail;
		tail = list;
		list = list->next;
	} while (list);

	/* The final links to make a circular doubly-linked list */
	tail->next = head;
	head->prev = tail;
}

static void merge_final(void *priv, list_cmp_func_t cmp, struct list_head *head,
			struct list_head *a, struct list_head *b)
{
	struct list_head *tail = head;
	uint8_t count = 0;

	for (;;) {
		/* if equal, take 'a' -- important for sort stability */
		if (cmp(priv, a, b) <= 0) {
			tail->next = a;
			a->prev = tail;
			tail = a;
			a = a->next;
			if (!a)
				break;
		} else {
			tail->next = b;
			b->prev = tail;
			tail = b;
			b = b->next;
			if (!b) {
				b = a;
				break;
			}
		}
	}

	/* Finish linking remainder of list b on to tail */
	build_prev_link(head, tail, b);
}

static struct list_head *find_run(void *priv, struct list_head *list,
				  size_t *len, list_cmp_func_t cmp, struct run* run_head)
{
	*len = 1;
	struct list_head *next = list->next;

	if (unlikely(next == NULL))
		return NULL;

	if (cmp(priv, list, next) > 0) {
		/* decending run, also reverse the list */
		struct list_head *prev = NULL;
		do {
			(*len)++;
			list->next = prev;
			prev = list;
			list = next;
			next = list->next;
			run_head->list = list;
		} while (next && cmp(priv, list, next) > 0);
		list->next = prev;
	} else {
		run_head->list = list;
		do {
			(*len)++;
			list = next;
			next = list->next;
		} while (next && cmp(priv, list, next) <= 0);
		list->next = NULL;
	}

	return next;
}

static void merge_at(void *priv, list_cmp_func_t cmp, struct run *at)
{
	at[0].list = merge(priv, cmp, at[0].list, at[1].list);
	at[0].len += at[1].len;
}

static struct run *merge_force_collapse(void *priv, list_cmp_func_t cmp,
					struct run *stk, struct run *tp)
{
	while ((tp - stk + 1) >= 3) {
		merge_at(priv, cmp, &tp[-1]);
		tp--;
	}
	return tp;
}

static struct run *merge_collapse(void *priv, list_cmp_func_t cmp,
				  struct run *stk, struct run *tp)
{
	while ((tp - stk + 1) >= 3) {
		if (__builtin_clzl(tp[-2].len) <
		    __builtin_clzl(tp[-1].len | tp[0].len))
			break;
		merge_at(priv, cmp, &tp[-2]);
		tp[-1] = tp[0];
		tp--;
	}

	return tp;
}

void shiverssort(void *priv, struct list_head *head, list_cmp_func_t cmp)
{
	struct list_head *list = head->next;
	struct run stk[MAX_MERGE_PENDING], *tp = stk - 1;

	if (head == head->prev)
		return;

	/* Convert to a null-terminated singly-linked list. */
	head->prev->next = NULL;

	do {
		tp++;
		/* Find next run */
		tp->list = list;
		list = find_run(priv, list, &tp->len, cmp, tp);
		tp = merge_collapse(priv, cmp, stk, tp);
	} while (list);

	/* End of input; merge together all the runs. */
	tp = merge_force_collapse(priv, cmp, stk, tp);

	/* The final merge; rebuild prev links */
	if (tp > stk) {
		merge_final(priv, cmp, head, stk[0].list, stk[1].list);
	} else {
		build_prev_link(head, head, stk->list);
	}
}
