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
struct pair {
	struct list_head *head;
	struct list_head *next;
};

// node->prev: to connect the whole stack
// node->next->prev: the index of the run
// node->next->next->prev: the length of the run

static size_t get_run_size(struct list_head *run_head) {
	if(!run_head)
		return 0;
	if(!run_head->next)
		return 1;
    if(!run_head->next->next)
        return 2;
	return (size_t)((run_head)->next->next->prev);
}
static unsigned int get_run_index(struct list_head* run_head) {
    // This will cause an issue for using insertion sort
    if(!run_head->next)
        return N;
    return (unsigned int)((run_head)->next->prev);
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
				  list_cmp_func_t cmp, unsigned int index)
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
	// head->next->prev = (struct list_head *)len;
    // set the index of the run
    if(head->next) head->next->prev = (struct list_head *)index;
    // set the length of the run
    if(head->next && head->next->next) head->next->next->prev = (struct list_head *)len;
	result.head = head;
	result.next = next;
	return result;
}

static struct list_head *merge_at(void *priv, list_cmp_func_t cmp, struct list_head *at)
{
	size_t len = get_run_size(at) + get_run_size(at->prev);
    unsigned int index = get_run_index(at->prev);
	struct list_head *prev = at->prev->prev;
	struct list_head * a = at->prev;
	struct list_head * b = at;
	struct list_head *list = merge(priv, cmp, at->prev, at);
	list->prev = prev;
	// list->next->prev = (struct list_head *)len;
    if(list->next) list->next->prev = (struct list_head *)index;
    if(list->next->next) list->next->next->prev = (struct list_head *)len;
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

static int nodePower(struct list_head* tp)
{
    // printf("calculating node power\n");
    int n1 = get_run_size(tp->prev);
    int n2 = get_run_size(tp);
    int L = 0;
    int s1 = get_run_index(tp->prev);
    int s2 = get_run_index(tp);
    double a = (s1 * 1.0 + n1 / 2.0 - 1.0) / N;
    double b = (s2 * 1.0 + n2 / 2.0 - 1.0) / N;
    while((long long int)(a * (1 << L)) == (long long int)(b * (1 << L))) {
        L++;
    }
    #ifdef DEBUG_INPLACE_POWERSORT
    // printf("L:%d\n", L);
    #endif
    return L;
}

static struct list_head *merge_collapse(void *priv, list_cmp_func_t cmp,
				  struct list_head *tp)
{
	while (stk_size >= 3) {
    //     if (__builtin_clzl(get_run_size(tp->prev->prev)) <
	// 	    __builtin_clzl(get_run_size(tp->prev) | get_run_size(tp)))
	// 		break;
		int power = nodePower(tp);
        int power_in_stack = nodePower(tp->prev);
        if(power_in_stack <= power)
            break;
		// tp = merge_at(priv, cmp, tp);
        tp->prev = merge_at(priv, cmp, tp->prev);
	}

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

    unsigned int index = 1;
	do {
		/* Find next run */
        struct pair result = find_run(priv, list, cmp, index);
        #ifdef DEBUG_INPLACE_POWERSORT
        // printf("find run\n");
        // for(struct list_head* cur = result.head; cur; cur = cur->next) {
        //     printf("%d ", list_entry(cur, element_t, list)->val);
        // }
        #endif
        result.head->prev = tp;
		tp = result.head;
		list = result.next;
		stk_size++;
        index += get_run_size(tp);
        #ifdef DEBUG_INPLACE_POWERSORT
            printf("new run size: %zu\n", get_run_size(tp));
            for(struct list_head* cur = tp; cur; cur = cur->next) {
                printf("%d ", list_entry(cur, element_t, list)->val);
            }
            printf("\n");
        #endif
        // printf("index:%d\n", index);
		tp = merge_collapse(priv, cmp, tp);
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
