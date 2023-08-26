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

#define INSERTION_SORT
#define MIN_RUN 32

#ifdef INSERTION_SORT
#include <stdio.h>
typedef struct element {
	struct list_head list;
	int val;
	int seq;
} element_t;

// function to find out middle element
static struct list_head* middle(struct list_head* start, struct list_head* last)
{
    if (start == NULL)
        return NULL;

    struct list_head* slow = start;
    struct list_head* fast = start->next;

    while (fast != last) {
        fast = fast->next;
        if (fast != last) {
            slow = slow->next;
            fast = fast->next;
        }
    }

    return slow;
}

// Function for implementing the Binary
// Search on linked list
static struct list_head* binarySearchAsc(struct list_head* head, struct list_head* value, void *priv, list_cmp_func_t cmp, struct list_head** sorted)
{
    struct list_head* start = head;
    struct list_head* last = NULL;
    struct list_head* result = NULL;

    while(start != last && start) {
        struct list_head* mid = middle(start, last);
        // if(mid->data <= value) {
		if(cmp(priv, mid, value) <= 0) {
            result = mid;
            start = mid->next;
        } else {
            last = mid;
        }
    }

    return result;
}
static struct list_head* binarySearchDes(struct list_head* head, struct list_head* value, void *priv, list_cmp_func_t cmp, struct list_head** sorted)
{
    struct list_head* start = head;
    struct list_head* last = NULL;
    struct list_head* result = NULL;

    while(start != last && start) {
        struct list_head* mid = middle(start, last);
        // if(mid->data <= value) {
		if(cmp(priv, mid, value) < 0) {
            result = mid;
            start = mid->next;
        } else {
            last = mid;
        }
    }

    return result;
}

static void sortedInsertAsc(void *priv, list_cmp_func_t cmp, struct list_head* newnode, struct list_head** sorted)
{
    /* Special case for the head end */
	// if()
	// printf("cmp %d, %d\n", list_entry((*sorted), element_t, list)->val, list_entry(newnode, element_t, list)->val);
    if (*sorted == NULL) {
        newnode->next = *sorted;
        *sorted = newnode;
    }
    else {
        struct list_head* current = *sorted;
        /* Locate the node before the point of insertion
         */
		// printf("cmp %d, %d\n", list_entry(current->next, element_t, list)->val, list_entry(newnode, element_t, list)->val);
        // while (current->next != NULL
        //        && cmp(priv, current->next, newnode) <= 0) {
        //     current = current->next;
        // }
		current = binarySearchAsc(*sorted, newnode, priv, cmp, sorted);
		if(current) {
        	newnode->next = current->next;
        	current->next = newnode;
		}
		else {
			newnode->next = *sorted;
        	*sorted = newnode;
		}
    }
}
static void sortedInsertDes(void *priv, list_cmp_func_t cmp, struct list_head* newnode, struct list_head** sorted)
{
    /* Special case for the head end */
	// if()
	// printf("cmp %d, %d\n", list_entry((*sorted), element_t, list)->val, list_entry(newnode, element_t, list)->val);
    if (*sorted == NULL) {
        newnode->next = *sorted;
        *sorted = newnode;
    }
    else {
        struct list_head* current = *sorted;
        /* Locate the node before the point of insertion
         */
		// printf("cmp %d, %d\n", list_entry(current->next, element_t, list)->val, list_entry(newnode, element_t, list)->val);
        // while (current->next != NULL
        //        && cmp(priv, current->next, newnode) < 0) {
        //     current = current->next;
        // }
		current = binarySearchDes(*sorted, newnode, priv, cmp, sorted);
		if(current) {
			newnode->next = current->next;
			current->next = newnode;
		}
		else {
			newnode->next = *sorted;
        	*sorted = newnode;
		}
    }
}
  
// function to sort a singly linked list 
// using insertion sort
static struct list_head* insertionsort(void *priv, list_cmp_func_t cmp, struct list_head* head, int asc)
{
	int cmp_count = *((int *)priv);
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

		current->next = NULL;
  
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
	// printf("cmp_count:%d\n", *((int *)priv) - cmp_count);
	return sorted;
}
#endif

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
	// need_insertion_sort = 0;
	if(need_insertion_sort) {
		// printf("sorting run len: %ld\n", *len);
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
