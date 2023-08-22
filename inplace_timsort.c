#include "list.h"
#include "list_sort.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#ifndef likely
#define likely(x) __builtin_expect(!!(x), 1)
#endif
#ifndef unlikely
#define unlikely(x) __builtin_expect(!!(x), 0)
#endif
// #define DEBUG_INPLACE_TIMSORT
#ifdef DEBUG_INPLACE_TIMSORT
typedef struct element {
	struct list_head list;
	int val;
	int seq;
} element_t;
#include <stdio.h>
#endif

// typedef struct element {
// 	struct list_head list;
// 	int val;
// 	int seq;
// } element_t;

static size_t get_run_size(struct list_head *run_head) {
	// if(run_head){
	// 	// printf("getting run size\n");
	// 	int ctr = 0;
	// 	for(struct list_head* cur = run_head; cur ; cur = cur->next) {
	// 		element_t *entry = list_entry(cur, element_t, list);
	// 		// printf("%d ", entry->val);
	// 		++ctr;
	// 	}
	// 	// printf("\nphysical size: %lld\n", (long long int)ctr);
	// 	if(run_head && !run_head->next) {
	// 		printf("report error\n");
	// 		exit(1);
	// 	}
	// 	// printf("logical size: %lld\n", (long long int)((run_head)->next->prev));
	// 	if((long long int)ctr != (long long int)((run_head)->next->prev)) {
	// 		printf("report error for difference\n");
	// 		exit(1);
	// 	}
	// }
	// if(run_head && !run_head->next) {
	// 	printf("report error\n");
	// 	exit(1);
	// }
	if(!run_head)
		return 0;
	if(!run_head->next)
		return 1;
	return (size_t)((run_head)->next->prev);
	// return run_head ? (size_t)((run_head)->next->prev) : 0;
}

struct pair {
	struct list_head *head;
	struct list_head *next;
};

size_t stk_size;

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

static struct pair find_run(void *priv, struct list_head *list,
				  list_cmp_func_t cmp)
{
	size_t len = 1;
	struct list_head *next = list->next;
	struct list_head *head = list;
	struct pair result;

	if (unlikely(next == NULL)) {
		result.head = head;
		result.next = next;
		return result;
	}

	if (cmp(priv, list, next) > 0) {
		/* decending run, also reverse the list */
		struct list_head *prev = NULL;
		do {
			len++;
			list->next = prev;
			prev = list;
			list = next;
			next = list->next;
			head = list;
		} while (next && cmp(priv, list, next) > 0);
		list->next = prev;
	} else {
		do {
			len++;
			list = next;
			next = list->next;
		} while (next && cmp(priv, list, next) <= 0);
		list->next = NULL;
	}
	head->prev = NULL;
	head->next->prev = (struct list_head *)len;
	result.head = head;
	result.next = next;
	return result;
}

static struct list_head *merge_at(void *priv, list_cmp_func_t cmp, struct list_head *at)
{
	size_t len = get_run_size(at) + get_run_size(at->prev);
	struct list_head *prev = at->prev->prev;
	struct list_head * a = at->prev;
	struct list_head * b = at;
	#ifdef DEBUG_INPLACE_TIMSORT
		printf("merging at:\n");
		printf("len a: %zu\n", get_run_size(a));	
		printf("len b: %zu\n", get_run_size(b));
		printf("len: %zu\n", len);
		for(struct list_head* cur = a; cur ; cur = cur->next) {
			element_t *entry = list_entry(cur, element_t, list);
			printf("A val:%d seq:%d\n", entry->val, entry->seq);
		}
		printf("\n");
		for(struct list_head* cur = b; cur ; cur = cur->next) {
			element_t *entry = list_entry(cur, element_t, list);
			// printf("%d ", entry->val);
			printf("B val:%d seq:%d\n", entry->val, entry->seq);
		}
		printf("\n");
		printf("end merging at\n\n");
	#endif
	struct list_head *list = merge(priv, cmp, at->prev, at);
	// struct list_head *list = merge(priv, cmp, at, at->prev);
	list->prev = prev;
	list->next->prev = (struct list_head *)len;
	--stk_size;
	#ifdef DEBUG_INPLACE_TIMSORT
		printf("after merging at:\n");
		printf("len: %zu\n", get_run_size(list));
		for(struct list_head* cur = list; cur ; cur = cur->next) {
			element_t *entry = list_entry(cur, element_t, list);
			// printf("%d ", entry->val);
			printf("Final val:%d seq:%d\n", entry->val, entry->seq);
		}
		printf("\n");
		printf("end after merging at\n\n");
	#endif
	return list;
}

static struct list_head *merge_force_collapse(void *priv, list_cmp_func_t cmp,
					struct list_head *tp)
{
	while (stk_size >= 3) {
		if (get_run_size(tp->prev->prev) < get_run_size(tp)) {
			tp->prev = merge_at(priv, cmp, tp->prev);
		} else {
			tp = merge_at(priv, cmp, tp);
		}
	}
	return tp;
}

static struct list_head *merge_collapse(void *priv, list_cmp_func_t cmp,
				  struct list_head *tp)
{
	#ifdef DEBUG_INPLACE_TIMSORT
	printf("merge_collapse\n");
	#endif
	int n;
	while ((n = stk_size) >= 2) {
		#ifdef DEBUG_INPLACE_TIMSORT
		printf("stk_size: %lld\n", (long long int)stk_size);
		for(int i = 0; i < n; i++) {
			struct list_head *cur = tp;
			printf("get_run_size(tp");
			for(int j = 0; j < i; j++) {
				printf("->prev");
				cur = cur->prev;
			}
			printf("): %lld\n", (long long int)get_run_size(cur));
		}
		#endif
		
		if ((n >= 3 && get_run_size(tp->prev->prev) <= get_run_size(tp->prev) + get_run_size(tp)) ||
		    (n >= 4 && get_run_size(tp->prev->prev->prev) <= get_run_size(tp->prev->prev) + get_run_size(tp->prev))) {
			if (get_run_size(tp->prev->prev) < get_run_size(tp)) {
				tp->prev = merge_at(priv, cmp, tp->prev);
			} else {
				tp = merge_at(priv, cmp, tp);
			}
		} else if (get_run_size(tp->prev) <= get_run_size(tp)) {
			tp = merge_at(priv, cmp, tp);
		} else {
			break;
		}
	}
	#ifdef DEBUG_INPLACE_TIMSORT
	printf("end merge_collapse\n\n");
	#endif

	return tp;
}

#ifdef DEBUG_INPLACE_TIMSORT
void printing_list(struct list_head* tp)
{
	printf("start print stack:\n");
	while(tp){
		printf("len: %zu\n", get_run_size(tp));
		for(struct list_head* cur = tp; cur ; cur = cur->next) {
			element_t *entry = list_entry(cur, element_t, list);
			printf("%d ", entry->val);
		}
		printf("\n");
		tp = tp->prev;
	}
	printf("\nend print stack\n\n");
}
#endif

void inplace_timsort(void *priv, struct list_head *head, list_cmp_func_t cmp)
{
	stk_size = 0;

	struct list_head *list = head->next;
	struct list_head *tp = NULL;

	if (head == head->prev)
		return;

	/* Convert to a null-terminated singly-linked list. */
	head->prev->next = NULL;

	do {
		/* Find next run */
		struct pair result = find_run(priv, list, cmp);
		#ifdef DEBUG_INPLACE_TIMSORT
		printf("new run:\n");
		for(struct list_head* cur = result.head; cur ; cur = cur->next) {
			element_t *entry = list_entry(cur, element_t, list);
			printf("%d ", entry->val);
		}
		printf("\n");
		#endif
		result.head->prev = tp;
		tp = result.head;
		list = result.next;
		stk_size++;
		#ifdef DEBUG_INPLACE_TIMSORT
		printf("before merge collapse:\n");
		printing_list(tp);
		#endif
		tp = merge_collapse(priv, cmp, tp);
		#ifdef DEBUG_INPLACE_TIMSORT
		printf("after merge collapse:\n");
		printing_list(tp);
		#endif
	} while (list);

	#ifdef DEBUG_INPLACE_TIMSORT
	printf("end of finding runs\n\n");
	#endif

	/* End of input; merge together all the runs. */
	tp = merge_force_collapse(priv, cmp, tp);

	/* The final merge; rebuild prev links */
	struct list_head *stk0 = tp;
	struct list_head *stk1 = stk0->prev;
	while(stk1 && stk1->prev){
		stk0 = stk0->prev;
		stk1 = stk1->prev;
	}
	if (stk_size > 1) {
		merge_final(priv, cmp, head, stk1, stk0);
	} else {
		build_prev_link(head, head, stk0);
	}
}
