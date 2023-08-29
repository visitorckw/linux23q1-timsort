#include "list.h"
#include "list_sort.h"

#include <stdint.h>
#include <stdlib.h>
// #include <math.h>

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
struct list_head* arr[MIN_RUN];

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
static struct list_head* binarySearchAsc(struct list_head* head, struct list_head* value, void *priv, list_cmp_func_t cmp,
										struct list_head** sorted)
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

static int mysqrt(int x)
{
	int L = 0;
	int R = 100;
	int res = 0;
	while(L <= R) {
		int mid = (L + R) / 2;
		if(mid * mid <= x) {
			res = mid;
			L = mid + 1;
		}
		else R = mid - 1;
	}
	return res;
}

static struct list_head* linearSearch(void *priv, list_cmp_func_t cmp, struct list_head* start, struct list_head* end, struct list_head* target)
{
    struct list_head* result = NULL;
    struct list_head* cur = start;
    while(cur != end) {
		if(cmp(priv, cur, target) <= 0) result = cur;
        else break;
        cur = cur->next;
    }
    return result;
}

static struct list_head* jumpSearch(void *priv, list_cmp_func_t cmp, struct list_head* head, struct list_head* target, int length)
{
	if(!head) return NULL;
    int step = mysqrt(length);
	// printf("length:%d\n", length);
    // printf("step:%d\n", step);
	// printf("head val:%d\n", list_entry(head, element_t, list)->val);
    static struct list_head dummy_real;
    static struct list_head* dummy = &dummy_real;
    dummy->next = head;
    struct list_head* prev = head;
    struct list_head* cur = dummy;
    int index = -1;
    while(index + step < length) {
        prev = cur;
        index += step;
        for(int i = 0; i < step; i++) {
            cur = cur->next;
			// printf("val:%d\n", list_entry(cur, element_t, list)->val);
        }
		// printf("index:%d\n", index);
		// printf("val:%d\n", list_entry(cur, element_t, list)->val);
        if(cmp(priv, cur, target) > 0) {
            struct list_head* result = linearSearch(priv, cmp, prev->next, cur, target);
            return result ? result : (prev == dummy ? NULL : prev);
        }
    }

    struct list_head* result = linearSearch(priv, cmp, cur, NULL, target);
    return result ? result : cur;
}

// Function for implementing the Binary
// Search with array on linked list
int arrayBinarySearch(void *priv, list_cmp_func_t cmp, struct list_head* arr[], 
											struct list_head* target, int length)
{
	// printf("start array binary search, length: %d\n", length);
	// printf("target val:%d\n", list_entry(target, element_t, list)->val);
	// for(int i = 0; i < length; i++) {
	// 	if(!arr[i]) printf("NULL\n");
	// 	else printf("val:%d seq:%d\n", list_entry(arr[i], element_t, list)->val, list_entry(arr[i], element_t, list)->seq);
	// }
	// printf("\n");

	int L = 0;
	int R = length - 1;
	int M;
	int res = -1;
	while(L <= R) {
		M = (L + R) / 2;
		if(cmp(priv, arr[M], target) <= 0) {
			res = M;
			L = M + 1;
		}
		else {
			R = M - 1;
		}
	}
	return res;
}

static void sortedInsertAsc(void *priv, list_cmp_func_t cmp, struct list_head* newnode, int length)
{
    /* Special case for the head end */
	// if()
	// printf("cmp %d, %d\n", list_entry((*sorted), element_t, list)->val, list_entry(newnode, element_t, list)->val);

    if (!length) {
        // newnode->next = *sorted;
		arr[0] = newnode;
    }
    else {
        // struct list_head* current = *sorted;
        /* Locate the node before the point of insertion
         */
        
		// if(cmp(priv, newnode, current) <= 0) current = NULL;
		// else {
		// 	while (current->next != NULL
		// 		&& cmp(priv, current->next, newnode) <= 0) {
		// 		current = current->next;
		// 	}
		// }

		// struct list_head* current1 = binarySearchAsc(*sorted, newnode, priv, cmp, sorted);
		// current = jumpSearch(priv, cmp, *sorted, newnode, length);

		int index = arrayBinarySearch(priv, cmp, arr, newnode, length);
		if(index == -1) {
			newnode->next = arr[0];
			for(int i = length; i > 0; i--) {
				arr[i] = arr[i - 1];
			}
		}
		else {
			arr[index]->next = newnode;
			if(index + 1 != length)
				newnode->next = arr[index + 1];
			for(int i = length; i > index + 1; i--) {
				arr[i] = arr[i - 1];
			}
		}
		arr[index + 1] = newnode;
    }
}
static void sortedInsertDes(void *priv, list_cmp_func_t cmp, struct list_head* newnode, struct list_head** sorted, struct list_head** last)
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
	if(newnode->next == NULL) *last = newnode;
}
  
// function to sort a singly linked list 
// using insertion sort
static struct list_head* insertionsort(void *priv, list_cmp_func_t cmp, struct list_head* head,
										int asc, struct list_head** last)
{

	// int cmp_count = *((int *)priv);
    struct list_head* current = head;
	// struct list_head* sorted = NULL;
	int sorted_length = 0;
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
		// if(asc)
		sortedInsertAsc(priv, cmp, current, sorted_length++);
		// else sortedInsertDes(priv, cmp, current, &sorted, last);
  
        // Update current
        current = next;
    }
    // Update head to point to sorted linked list
    // head = sorted;
	// printf("cmp_count:%d\n", *((int *)priv) - cmp_count);
	// return sorted;
	*last = arr[sorted_length - 1];
	return arr[0];
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

    run_head->list = list;
    while(next && (*len) < MIN_RUN) {
        (*len)++;
        list = next;
        next = list->next;
    }
	list->next = NULL;
	struct list_head *last = NULL;
    run_head->list = insertionsort(priv, cmp, run_head->list, 1, &last);
	last->next = next;
	list = last;
    while (next && cmp(priv, list, next) <= 0) {
        (*len)++;
        list = next;
        next = list->next;
    }
	list->next = NULL;

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
