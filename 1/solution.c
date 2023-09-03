#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include "libcoro.h"

#define OUT_FNAME "out1.txt"

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

unsigned long microsec(struct timespec ts) {
    return ts.tv_sec * (unsigned)1e6 + ts.tv_nsec / (unsigned)1e3;
}

unsigned long get_cur_time() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);

    return microsec(ts);
}

void my_qsort(int* arr, int i_start, int i_end, unsigned long* t_hold, unsigned long* t_coro_iteration_start, int coro_latency) {
    int m = arr[(i_end + i_start) / 2];

    int i = i_start;
    int j = i_end;

    do {
        while (arr[i] < m) {
            i++;
        }

        while (arr[j] > m) {
            j--;
        }

        if (i <= j) {
            int temp = arr[i];
            arr[i] = arr[j];
            arr[j] = temp;

            i++;
            j--;

            unsigned long passed_time_since_iteration = get_cur_time() - *t_coro_iteration_start;
            if (passed_time_since_iteration >= coro_latency) {
                size_t t1 = get_cur_time();

                coro_yield(); // ==================== YIELD HERE =================

                size_t t2 = get_cur_time();
                *t_coro_iteration_start = t2;

                size_t diff = t2 - t1;
                *t_hold += diff;
            }
        }
    } while (i <= j);

    if (j > i_start) {
        my_qsort(arr, i_start, j, t_hold, t_coro_iteration_start, coro_latency);
    }
    if (i < i_end) {
        my_qsort(arr, i, i_end, t_hold, t_coro_iteration_start, coro_latency);
    }
}

void sort_int_arr(struct int_arr* ia, unsigned long* t_hold, unsigned long* t_coro_iteration_start, int coro_latency) {
    my_qsort(ia->start, 0, ia->length - 1, t_hold, t_coro_iteration_start, coro_latency);
}

int arr_from_file(char* fname, struct int_arr* ia_out) {
    FILE* fin = fopen(fname, "r");
    if (fin == NULL) {
        fprintf(stderr, "File not found\n");
        //        exit(-1);
        return -1;
    }

    // Get file size
    fseek(fin, 0L, SEEK_END);
    long fsize = ftell(fin);
    rewind(fin);

    int n_scanf, i = 0;
    int* arr = malloc(fsize * sizeof(int));

    while (fscanf(fin, "%d", &n_scanf) == 1) {
        arr[i] = n_scanf;
        i++;
    }
    fclose(fin);
    int* new_arr_ptr = realloc(arr, i * sizeof(int));
    if (new_arr_ptr != NULL) {
        arr = new_arr_ptr;
    }

    *ia_out = (struct int_arr){ arr, i };
    return 0;
}

void arr_to_file(struct int_arr* arr, char* fname) {
    FILE* fout = fopen(fname, "w");

    for (int j = 0; j < arr->length; ++j) {
        fprintf(fout, "%d ", arr->start[j]);
    }
    fprintf(fout, "\n");

    fclose(fout);
}

struct int_arr create_int_arr(int of_size) {
    return (struct int_arr) { .start = malloc(of_size * sizeof(int)), .length = of_size };
}

struct int_arr* merge_two_arrays(struct int_arr* ia1, struct int_arr* ia2) {
    int* p1 = ia1->start;
    int* p2 = ia2->start;

    int* end1 = ia1->start + ia1->length;
    int* end2 = ia2->start + ia2->length;

    int res_length = ia1->length + ia2->length;
    struct int_arr* result_arr = malloc(sizeof(struct int_arr));
    *result_arr = create_int_arr(res_length);

    int i = 0;
    while (p1 < end1 || p2 < end2) {
        if ((*p1 < *p2 || p2 >= end2) && p1 < end1) {
            result_arr->start[i] = *p1;
            p1++;
        }
        else if ((*p1 >= *p2 || p1 >= end1) && p2 < end2) {
            result_arr->start[i] = *p2;
            p2++;
        }
        else {
            fprintf(stderr, "Err :(\n");
            exit(-1);
        }
        i++;
    }

    return result_arr;
}

void free_int_arr(struct int_arr* ia) {
    free(ia->start);
    free(ia);
}

struct int_arr* multiple_ia_sort(struct int_arr** ia_arr, int size) {
    struct int_arr* result_arr;

    // TODO: Optimize to bin-tree merging
    for (int i = 0; i < size; ++i) {
        if (i == 0) {
            result_arr = ia_arr[i];
        }
        else {
            struct int_arr* temp = merge_two_arrays(result_arr, ia_arr[i]);

            if (i > 1) { free_int_arr(result_arr); }
            result_arr = temp;
        }
    }

    return result_arr;
}

static int coroutine_func_f(void* arg1) {
    struct coro_args* ca = arg1;

    while (*ca->global_i < ca->fnames_cnt) {
        int cur_i = *ca->global_i;
        (*ca->global_i)++;
        char* cur_fname = ca->fnames[cur_i];

        printf("CA: %p - %s\n", ca, cur_fname);

        struct int_arr* ia_i = malloc(sizeof(struct int_arr));
        int res_read = arr_from_file(cur_fname, ia_i);
        if (res_read == -1) {
            return -1;
        }

        unsigned long t_start = get_cur_time();
        unsigned long t_hold = 0;
        unsigned long t_coro_iteration_start = get_cur_time();

        /* IMPLEMENT SORTING OF INDIVIDUAL FILES HERE. */

        sort_int_arr(ia_i, &t_hold, &t_coro_iteration_start, ca->coro_latency);
        ca->ia_arr[cur_i] = ia_i;

        size_t exec_time = get_cur_time() - t_start - t_hold;

        struct coro* this = coro_this();
        printf("%p: %s t_hold: %lu microsec\n", ca, cur_fname, t_hold);
        printf("%p: %s exec_time: %lu microsec\n", ca, cur_fname, exec_time);
        printf("%p: %s switch count: %lld\n", ca, cur_fname, coro_switch_count(this));
    }

    free(ca);
    return 0;
}

int main(int argc, char** argv) {
    struct timespec exec_time;
    clock_gettime(CLOCK_MONOTONIC, &exec_time);

    unsigned long start_time = get_cur_time();

    argv++;
    int coro_latency = (int)strtol(*(argv++), NULL, 10);
    int coro_count = (int)strtol(*(argv++), NULL, 10);

    if (coro_count < 1) {
        printf("WRONG CORO COUNT: %d\n", coro_count);
        return -1;
    }

    printf("CORO COUNT: %d\n", coro_count);

    int fnames_cnt = argc - 3;
    char** fnames = argv;
    int GLOBAL_I = 0;

    for (int i = 0; i < fnames_cnt; ++i) {
        if (access(fnames[i], F_OK) != 0) {
            printf("FILE NOT FOUND: %s\n", fnames[i]);
            exit(-1);
        }
    }

    struct int_arr** ia_arr = malloc(fnames_cnt * sizeof(struct int_arr*));
    struct coro_args* ca_arr[coro_count];

    /* Initialize our coroutine global cooperative scheduler. */
    coro_sched_init();

    /* Start several coroutines. */
    for (int i = 0; i < coro_count; ++i) {
        /*
         * The coroutines can take any 'void *' interpretation of which
         * depends on what you want. Here as an example I give them
         * some names.
         */

        struct coro_args* ca_i = malloc(sizeof(struct coro_args));
        ca_i->ia_arr = ia_arr;
        ca_i->fnames = fnames;
        ca_i->fnames_cnt = fnames_cnt;
        ca_i->global_i = &GLOBAL_I;
        ca_i->coro_latency = coro_latency / coro_count;

        ca_arr[i] = ca_i;
    }

    for (int i = 0; i < coro_count; ++i) {
        coro_new(coroutine_func_f, ca_arr[i]);
    }

    /* Wait for all the coroutines to end. */
    struct coro* c;

    int ii = 0;
    while ((c = coro_sched_wait()) != NULL) {
        /*
         * Each 'wait' returns a finished coroutine with which you can
         * do anything you want. Like check its exit status, for
         * example. Don't forget to free the coroutine afterwards.
         */
        int coro_code = coro_status(c);
        printf("Finished (%p) %d\n", ca_arr[ii], coro_code);
        coro_delete(c);
        ii++;
    }

    /* All coroutines have finished. */

    /* IMPLEMENT MERGING OF THE SORTED ARRAYS HERE. */

    if (fnames_cnt > 1) {
        struct int_arr* res_ia = multiple_ia_sort(ia_arr, fnames_cnt);
        arr_to_file(res_ia, OUT_FNAME);
        free_int_arr(res_ia);
    }
    else {
        arr_to_file(ia_arr[0], OUT_FNAME);
    }

    for (int i = 0; i < fnames_cnt; ++i) {
        free_int_arr(ia_arr[i]);
    }
    free(ia_arr);

    unsigned long finish_time;

    clock_gettime(CLOCK_MONOTONIC, &exec_time);
    finish_time = get_cur_time();

    unsigned long msecs = finish_time - start_time;
    printf("Overall execution time: %lu microsec\n", msecs);

    return 0;
}