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

// #define DEBUG_INPLACE_POWERSORT
#ifdef DEBUG_INPLACE_POWERSORT
typedef struct element {
	struct list_head list;
	int val;
	int seq;
} element_t;
#include <stdio.h>
#endif

size_t stk_size;
int N = 0;
int s1, s2, e1, e2;

struct pair {
	struct list_head *head;
	struct list_head *next;
};

// node->prev: to connect the whole stack
// node->next->prev: the power value of the run

static size_t get_power(struct list_head *run_head) {
    if(!run_head->next) return -1;
    return (size_t)((run_head)->next->prev);
}

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
				  list_cmp_func_t cmp, size_t *len)
{
	*len = 1;
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
			*len++;
			list->next = prev;
			prev = list;
			list = next;
			next = list->next;
			head = list;
		} while (next && cmp(priv, list, next) > 0);
		list->next = prev;
	} else {
		do {
			*len++;
			list = next;
			next = list->next;
		} while (next && cmp(priv, list, next) <= 0);
		list->next = NULL;
	}
	head->prev = NULL;
	result.head = head;
	result.next = next;
	return result;
}

static struct list_head *merge_at(void *priv, list_cmp_func_t cmp, struct list_head *at)
{
	struct list_head *prev = at->prev->prev;
	struct list_head * a = at->prev;
	struct list_head * b = at;
	struct list_head *list = merge(priv, cmp, at->prev, at);
	list->prev = prev;
	--stk_size;

	return list;
}

static struct list_head *merge_force_collapse(void *priv, list_cmp_func_t cmp,
					struct list_head *tp)
{
	while (stk_size >= 3) {
		tp = merge_at(priv, cmp, tp);
	}
	return tp;
}

static size_t nodePower()
{
    // printf("calculating node power\n");
    int n1 = e1 - s1 + 1;
    int n2 = e2 - s2 + 1;
    size_t L = 0;
    double a = (s1 * 1.0 + n1 / 2.0 - 1.0) / N;
    double b = (s2 * 1.0 + n2 / 2.0 - 1.0) / N;
    while((long long int)(a * (1 << L)) == (long long int)(b * (1 << L))) {
        L++;
    }
    return L;
}

#include <stdio.h>

static struct list_head *merge_collapse(void *priv, list_cmp_func_t cmp,
				  struct list_head *tp)
{
    size_t power = nodePower();
    // printf("current power: %zu\n", power);
    // if(tp->next) {
    //     tp->next->prev = (struct list_head*)(power);
    // }
    // printf("all power in stack:\n");
    // for(struct list_head *cur = tp; cur; cur = cur->prev) {
    //     printf("%zu ", get_power(cur));
    // }
    // printf("\n");
	while (stk_size >= 3) {
        size_t power_in_stack = get_power(tp->prev->prev);
        // printf("power in stack: %zu\n", power_in_stack);
        if(power_in_stack <= power)
            break;
		// tp = merge_at(priv, cmp, tp);
        tp->prev = merge_at(priv, cmp, tp->prev);

	}
    // printf("before setting power\n");
    // for(struct list_head *cur = tp; cur; cur = cur->prev) {
    //     printf("%zu ", get_power(cur));
    // }
    // printf("\n");
    if(tp->prev && tp->prev->next) tp->prev->next->prev = (struct list_head*)(power);
    // printf("after setting power\n");
    // for(struct list_head *cur = tp; cur; cur = cur->prev) {
    //     printf("%zu ", get_power(cur));
    // }
    // printf("\n");

	return tp;
}

void inplace_powersort(void *priv, struct list_head *head, list_cmp_func_t cmp)
{
	stk_size = 0;
    N = 0;
    struct list_head *cur;
    list_for_each(cur, head) {
        N++;
    }

	struct list_head *list = head->next;
	struct list_head *tp = NULL;

	if (head == head->prev)
		return;

	/* Convert to a null-terminated singly-linked list. */
	head->prev->next = NULL;

    s1 = 1;
    size_t len;
    struct pair result = find_run(priv, list, cmp, &len);
    e1 = len;
    tp = result.head;
    list = result.next;
    stk_size = 1;

	do {
		/* Find next run */
        s2 = e1 + 1;
        struct pair result = find_run(priv, list, cmp, &len);
        e2 = s2 + len - 1;
        result.head->prev = tp;
		tp = result.head;
		list = result.next;
		stk_size++;
		tp = merge_collapse(priv, cmp, tp);
        s1 = s2;
        e1 = e2;
	} while (list);

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
