#include "list.h"
#include "list_sort.h"

#include <stdint.h>

#ifndef likely
#define likely(x) __builtin_expect(!!(x), 1)
#endif
#ifndef unlikely
#define unlikely(x) __builtin_expect(!!(x), 0)
#endif

// #define DEBUG_TIMSORT
#define MAX_MERGE_PENDING 85

struct run {
	struct list_head *list;
	size_t len;
};

// #define INSERTION_SORT

#ifdef INSERTION_SORT
#include <stdio.h>
typedef struct element {
	struct list_head list;
	int val;
	int seq;
} element_t;
static void sortedInsertAsc(void *priv, list_cmp_func_t cmp, struct list_head* newnode, struct list_head** sorted)
{
    /* Special case for the head end */
	// if()
	// printf("cmp %d, %d\n", list_entry((*sorted), element_t, list)->val, list_entry(newnode, element_t, list)->val);
    if (*sorted == NULL || cmp(priv, *sorted, newnode) > 0) {
        newnode->next = *sorted;
        *sorted = newnode;
    }
    else {
        struct list_head* current = *sorted;
        /* Locate the node before the point of insertion
         */
		// printf("cmp %d, %d\n", list_entry(current->next, element_t, list)->val, list_entry(newnode, element_t, list)->val);
        while (current->next != NULL
               && cmp(priv, current->next, newnode) <= 0) {
            current = current->next;
        }
        newnode->next = current->next;
        current->next = newnode;
    }
}
static void sortedInsertDes(void *priv, list_cmp_func_t cmp, struct list_head* newnode, struct list_head** sorted)
{
    /* Special case for the head end */
	// if()
	// printf("cmp %d, %d\n", list_entry((*sorted), element_t, list)->val, list_entry(newnode, element_t, list)->val);
    if (*sorted == NULL || cmp(priv, *sorted, newnode) >= 0) {
        newnode->next = *sorted;
        *sorted = newnode;
    }
    else {
        struct list_head* current = *sorted;
        /* Locate the node before the point of insertion
         */
		// printf("cmp %d, %d\n", list_entry(current->next, element_t, list)->val, list_entry(newnode, element_t, list)->val);
        while (current->next != NULL
               && cmp(priv, current->next, newnode) < 0) {
            current = current->next;
        }
        newnode->next = current->next;
        current->next = newnode;
    }
}
  
// function to sort a singly linked list 
// using insertion sort
static struct list_head* insertionsort(void *priv, list_cmp_func_t cmp, struct list_head* head, int asc)
{
    struct list_head* current = head;
	struct list_head* sorted = NULL;

	// printf("start insertion sort\n");
	// for(struct list_head* cur = head; cur ; cur = cur->next) {
	// 	printf("val:%d seq:%d\n", list_entry(cur, element_t, list)->val, list_entry(cur, element_t, list)->seq);
	// }
	// printf("\n");
  
    // Traverse the given linked list and insert every
    // node to sorted
    while (current != NULL) {
  
        // Store next for next iteration
        struct list_head* next = current->next;
  
        // insert current in sorted linked list
		if(asc) sortedInsertAsc(priv, cmp, current, &sorted);
		else sortedInsertDes(priv, cmp, current, &sorted);
  
        // Update current
        current = next;
    }
    // Update head to point to sorted linked list
    // head = sorted;
	// printf("after insertion sort\n");
	// for(struct list_head* cur = sorted; cur ; cur = cur->next) {
	// 	printf("val:%d seq:%d\n", list_entry(cur, element_t, list)->val, list_entry(cur, element_t, list)->seq);
	// }
	// printf("\n");
	return sorted;
}
#endif

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

#define MIN_RUN 35

static struct list_head *find_run(void *priv, struct list_head *list,
				  size_t *len, list_cmp_func_t cmp, struct run* run_head)
{
	*len = 1;
	struct list_head *next = list->next;

	if (unlikely(next == NULL))
		return NULL;
	
	int need_insertion_sort = 0;
	int asc = cmp(priv, list, next) <= 0;
	if (!asc) {
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
		#ifdef INSERTION_SORT
		while (next && (*len) < MIN_RUN) {
			(*len)++;
			list->next = prev;
			prev = list;
			list = next;
			next = list->next;
			run_head->list = list;
			need_insertion_sort = 1;
		}
		#endif
		list->next = prev;
	} else {
		run_head->list = list;
		do {
			(*len)++;
			list = next;
			next = list->next;
		} while (next && cmp(priv, list, next) <= 0);
		#ifdef INSERTION_SORT
		while(next && (*len) < MIN_RUN) {
			(*len)++;
			list = next;
			next = list->next;
			need_insertion_sort = 1;
		}
		#endif
		list->next = NULL;
	}
	#ifdef INSERTION_SORT
	if(need_insertion_sort) {
		run_head->list = insertionsort(priv, cmp, run_head->list, asc);
	}
	#endif

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
		if (tp[-2].len < tp[0].len) {
			merge_at(priv, cmp, &tp[-2]);
			tp[-1] = tp[0];
		} else {
			merge_at(priv, cmp, &tp[-1]);
		}
		tp--;
	}
	return tp;
}

static struct run *merge_collapse(void *priv, list_cmp_func_t cmp,
				  struct run *stk, struct run *tp)
{
	int n;
	while ((n = tp - stk + 1) >= 2) {
		if ((n >= 3 && tp[-2].len <= tp[-1].len + tp[0].len) ||
		    (n >= 4 && tp[-3].len <= tp[-2].len + tp[-1].len)) {
			if (tp[-2].len < tp[0].len) {
				merge_at(priv, cmp, &tp[-2]);
				tp[-1] = tp[0];
			} else {
				merge_at(priv, cmp, &tp[-1]);
			}
		} else if (tp[-1].len <= tp[0].len) {
			merge_at(priv, cmp, &tp[-1]);
		} else {
			break;
		}
		tp--;
	}

	return tp;
}

#ifdef DEBUG_TIMSORT
typedef struct element {
	struct list_head list;
	int val;
	int seq;
} element_t;
// #include <stdio.h>
void printing_list(struct run* stk, struct run* tp)
{
	// return;
	// if(!head)
	// 	return;
	// element_t *entry, *safe;
	// for(struct list_head* cur = head->next; cur ; cur = cur->next) {
	// 	entry = list_entry(cur, element_t, list);
	// 	printf("%d ", entry->val);

	// }
	// printf("\n");
	printf("start print stack:\n");
	while(tp >= stk) {
		printf("len: %lu\n", tp->len);
		struct list_head* cur = tp->list;
		while(cur) {
			element_t *entry = list_entry(cur, element_t, list);
			printf("%d ", entry->val);
			cur = cur->next;
		}
		printf("\n");
		tp--;
	}
	printf("end print stack\n\n");
}
#endif

void timsort(void *priv, struct list_head *head, list_cmp_func_t cmp)
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
		#ifdef DEBUG_TIMSORT
		printf("new run:\n");
		for(struct list_head* cur = tp->list; cur ; cur = cur->next) {
			element_t *entry = list_entry(cur, element_t, list);
			printf("%d ", entry->val);
		}
		printf("\n");
		#endif
		tp = merge_collapse(priv, cmp, stk, tp);
		#ifdef DEBUG_TIMSORT
		printing_list(stk, tp);
		#endif
	} while (list);

	#ifdef DEBUG_TIMSORT
	printf("end of finding runs\n\n");
	#endif

	/* End of input; merge together all the runs. */
	tp = merge_force_collapse(priv, cmp, stk, tp);

	/* The final merge; rebuild prev links */
	if (tp > stk) {
		merge_final(priv, cmp, head, stk[0].list, stk[1].list);
	} else {
		build_prev_link(head, head, stk->list);
	}
}
