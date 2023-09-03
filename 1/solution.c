#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include "libcoro.h"

#define out_fname "out1.txt"

struct int_arr {
	int* start;
	int length;
};

struct coro_args {
	char** fnames;
	int fnames_cnt;
	int* global_i;
	struct int_arr** ia_arr;
	int coro_latency;
};

void print_arr(struct int_arr* arr) {
	printf("%p %p  || ", arr, arr->start);
	int* ptr = arr->start;
	printf("Arr: ");
	for (int i = 0; i < arr->length; ++i) {
		printf("%d ", *ptr);
		ptr++;
	}
	printf(" |\n");
}
static void
other_function(const char *name, int depth)
{
	printf("%s: entered function, depth = %d\n", name, depth);
	coro_yield();
	if (depth < 3)
		other_function(name, depth + 1);
}

/**
 * Coroutine body. This code is executed by all the coroutines. Here you
 * implement your solution, sort each individual file.
 */
static int
coroutine_func_f(void *context)
{
	/* IMPLEMENT SORTING OF INDIVIDUAL FILES HERE. */

	struct coro *this = coro_this();
	char *name = context;
	printf("Started coroutine %s\n", name);
	printf("%s: switch count %lld\n", name, coro_switch_count(this));
	printf("%s: yield\n", name);
	coro_yield();

	printf("%s: switch count %lld\n", name, coro_switch_count(this));
	printf("%s: yield\n", name);
	coro_yield();

	printf("%s: switch count %lld\n", name, coro_switch_count(this));
	other_function(name, 1);
	printf("%s: switch count after other function %lld\n", name,
	       coro_switch_count(this));
	free(name);
	/* This will be returned from coro_status(). */
	return 0;
}

int
main(int argc, char **argv)
{
	/* Delete these suppressions when start using the args. */
	(void)argc;
	(void)argv;
	/* Initialize our coroutine global cooperative scheduler. */
	coro_sched_init();
	/* Start several coroutines. */
	for (int i = 0; i < 3; ++i) {
		/*
		 * The coroutines can take any 'void *' interpretation of which
		 * depends on what you want. Here as an example I give them
		 * some names.
		 */
		char name[16];
		sprintf(name, "coro_%d", i);
		/*
		 * I have to copy the name. Otherwise all the coroutines would
		 * have the same name when they finally start.
		 */
		coro_new(coroutine_func_f, strdup(name));
	}
	/* Wait for all the coroutines to end. */
	struct coro *c;
	while ((c = coro_sched_wait()) != NULL) {
		/*
		 * Each 'wait' returns a finished coroutine with which you can
		 * do anything you want. Like check its exit status, for
		 * example. Don't forget to free the coroutine afterwards.
		 */
		printf("Finished %d\n", coro_status(c));
		coro_delete(c);
	}
	/* All coroutines have finished. */

	/* IMPLEMENT MERGING OF THE SORTED ARRAYS HERE. */

	return 0;
}
