/***
    This file is used to measure the performance of the list_sort with the same format as the cpython's listsort.txt
***/


#include "list.h"
#include "list_sort.h"
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <stdint.h>
#include <stdbool.h>

typedef struct element {
	struct list_head list;
	int val;
	int seq;
} element_t;

// #define SAMPLES ((1 << 20) + 20)
int SAMPLES = 1;

char title[8][500] = {
    "*sort",
    "\\sort",
    "/sort",
    "3sort",
    "+sort",
    "%sort",
    "~sort",
    "=sort"
};
/**
    flag = 0: random
    flag = 1: descending
    flag = 2: ascending
    flag = 3: ascending, then 3 random exchanges
    flag = 4: ascending, then 10 random at the end
    flag = 5: ascending, then randomly replace 1% of elements w/ random values
    flag = 6: many duplicates
    flag = 7: all equal
**/
/**
    *sort: random data
    \sort: descending data
    /sort: ascending data
    3sort: ascending, then 3 random exchanges
    +sort: ascending, then 10 random at the end
    %sort: ascending, then randomly replace 1% of elements w/ random values
    ~sort: many duplicates
    =sort: all equal
    !sort: worst case scenario
**/
static void swap(int *a, int *b) {
    int tmp = *a;
    *a = *b;
    *b = tmp;
}

int size_n[] = {
    32768,
    65536,
    131072,
    262144,
    524288,
    1048576
};
int size_nlgn[] = {
    444255,
    954037,
    2039137,
    4340409,
    9205096,
    19458756
};

static void create_sample(struct list_head *head, element_t *space, int samples, int flag)
{
    int* arr = malloc(sizeof(int) * samples);
	for (int i = 0; i < samples; i++) {
        if(flag == 0)
		    arr[i] = rand();
        else if(flag == 1)
            arr[i] = samples - i;
        else if(flag == 2)
            arr[i] = i;
        else if(flag == 3)
            arr[i] = i;
        else if(flag == 4)
            arr[i] = (i <= samples - 10 ? i * 1LL * RAND_MAX / samples : rand());
        else if(flag == 5)
            arr[i] = rand() % 100 ? i  * 1LL * RAND_MAX / samples : rand();
        else if(flag == 6) 
            arr[i] = i % 4 + 1;
        else if(flag == 7)
            arr[i] = 1;
	}
    if(flag == 3) {
        int idx1 = -1, idx2 = -1;
        for(int i = 0; i < 3; i++) {
            while(idx1 == idx2) {
                idx1 = rand() % samples;
                idx2 = rand() % samples;
                swap(&arr[idx1], &arr[idx2]);
            }
            idx1 = idx2 = -1;
        }
    }
    if(flag == 6) {
        for(int i = 0; i < samples; i++) {
            int idx = rand() % samples;
            swap(&arr[i], &arr[idx]);
        }
    }
    for (int i = 0; i < samples; i++) {
        element_t *elem = space+i;
		elem->val = arr[i];
		elem->seq = i;
		list_add_tail(&elem->list, head);
    }
    free(arr);
}

static void copy_list(struct list_head *from, struct list_head *to, element_t *space)
{
	if (list_empty(from))
		return;

	element_t *entry;
	list_for_each_entry(entry, from, list) {
		element_t *copy = space++;
		copy->val = entry->val;
		copy->seq = entry->seq;
		list_add_tail(&copy->list, to);
	}
}

static void free_list(struct list_head *head)
{
	element_t *entry, *safe;
	list_for_each_entry_safe(entry, safe, head, list) {
		list_del(&entry->list);
		free(entry);
	}
}


int compare(void *priv, const struct list_head *a, const struct list_head *b)
{
	if (a == b)
		return 0;

	int res = list_entry(a, element_t, list)->val -
		  list_entry(b, element_t, list)->val;

	if (priv) {
		*((int *)priv) += 1;
	}

	return res;
}

bool check_list(struct list_head *head, int count)
{
	if (list_empty(head))
		return count == 0;

	element_t *entry, *safe;
	list_for_each_entry_safe(entry, safe, head, list) {
		if (entry->list.next != head) {
			if (entry->val > safe->val ||
			    (entry->val == safe->val && entry->seq > safe->seq)) {
				return false;
			}
		}
	}
	return true;
}

typedef void (*test_func_t)(void *priv, struct list_head *head,
			    list_cmp_func_t cmp);

typedef struct test {
	test_func_t fp;
	char *name;
} test_t;

int main(int argc, char **argv)
{
    // SAMPLES = atoi(argv[2]);
    char filename[] = "data.txt";
    FILE *fp = fopen(filename, "w");

    test_t tests[] = {
            // { list_sort, "list_sort" },
            { list_sort_old, "list_sort" },
            // { shiverssort, "shiverssort" },
            // { timsort, "timsort" },
            { inplace_timsort, "inplace_timsort" },
            { inplace_shiverssort, "shiverssort" },
            { inplace_powersort, "powersort" },
            { NULL, NULL } },
        *test_head = tests, *test = tests;

    fprintf(fp, "%-10s ", "n");
    fprintf(fp, "%-10s ", "nlgn");
    for(int i = 0; i < 8; i++) {
        fprintf(fp, "%-10s ", title[i]);
    }
    fprintf(fp, "\n");
    for(int size_idx = 0; size_idx < 6; size_idx++) {
        test = test_head;
        int SAMPLES = size_n[size_idx];
        while (test->fp != NULL) {
            fprintf(fp, "%-10d ", size_n[size_idx]);
            fprintf(fp, "%-10d ", size_nlgn[size_idx]);
            for(int flag = 0; flag < 8; flag++) {
                struct list_head sample_head, warmdata_head, testdata_head;
                element_t *samples, *warmdata, *testdata;
                int count;

                INIT_LIST_HEAD(&sample_head);

                samples = malloc(sizeof(*samples) * SAMPLES);
                warmdata = malloc(sizeof(*warmdata) * SAMPLES);
                testdata = malloc(sizeof(*testdata) * SAMPLES);

                srand(atoi(argv[1]));
                // srand(5);

                create_sample(&sample_head, samples, SAMPLES, flag);

                printf("==== Testing %s ====\n", test->name);
                /* Warm up */
                INIT_LIST_HEAD(&warmdata_head);
                INIT_LIST_HEAD(&testdata_head);
                copy_list(&sample_head, &testdata_head, testdata);
                copy_list(&sample_head, &warmdata_head, warmdata);
                test->fp(&count, &warmdata_head, compare);
                /* Test */
                clock_t begin, end;
                count = 0;
                // begin = clock();
                // test->fp(&count, &testdata_head, compare);
                // end = clock();
                struct timespec tstart = {0, 0}, tend = {0, 0};
                clock_gettime(CLOCK_MONOTONIC, &tstart);
                test->fp(&count, &testdata_head, compare);
                clock_gettime(CLOCK_MONOTONIC, &tend);
                long long duration = (1e9 * tend.tv_sec + tend.tv_nsec) -
                                    (1e9 * tstart.tv_sec + tstart.tv_nsec);
                printf("  Elapsed time:   %lld\n", duration);
                printf("  Comparisons:    %d\n", count);
                printf("  List is %s\n",
                    check_list(&testdata_head, SAMPLES) ? "sorted" : "not sorted");
                // fprintf(fp, "%lld ", duration);
                fprintf(fp, "%-10d ", count);
                // test++;
                free(samples);
                free(warmdata);
                free(testdata);
            }
            fprintf(fp, "\n");
            test++;
        }
        fprintf(fp, "\n");
    }
    fclose(fp);

	return 0;
}
