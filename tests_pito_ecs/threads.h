#pragma once

#include "common.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <pthread.h>

typedef pthread_t test_thread_t;

typedef struct
{
    int (*fn)(void*);
    void* arg;
} test_wrapper_args_t;

static inline void* test_wrapper(void* p)
{
    test_wrapper_args_t* args = (test_wrapper_args_t*)p;
    args->fn(args->arg);
    free(args);
    return NULL;
}


static inline int test_thread_create(test_thread_t* t, int (*fn)(void*), void* arg)
{
    test_wrapper_args_t* targs = (test_wrapper_args_t*)malloc(sizeof(*targs));
    targs->fn = fn;
    targs->arg = arg;
    return pthread_create(t, NULL, test_wrapper, targs) == 0 ? 0 : -1;
}

#define TEST_THREAD_CREATE(t, fn, arg) test_thread_create((t), (fn), (arg))
#define TEST_THREAD_JOIN(t)            pthread_join((t), NULL)
#define TEST_THREAD_OK                 0

static inline ecs_ret_t noop_system(ecs_t* ecs,
                             ecs_entity_t* entities,
                             size_t entity_count,
                             void* udata)
{
    (void)ecs;
    (void)entities;
    (void)entity_count;
    (void)udata;
    return 0;
}

typedef struct
{
    ecs_comp_t comp;
    int        count;
} spawn_ctx_t;


static inline int spawn_worker(void* arg)
{
    spawn_ctx_t* ctx = (spawn_ctx_t*)arg;

    for (int i = 0; i < ctx->count; i++)
    {
        ecs_entity_t entity = ecs_create(ecs);
        ecs_add(ecs, entity, ctx->comp, NULL);
    }

    return 0;
}

static inline ecs_ret_t spawn_system(ecs_t* ecs,
                              ecs_entity_t* entities,
                              size_t entity_count,
                              void* udata)
{
    (void)ecs;
    (void)entities;
    (void)entity_count;

    return spawn_worker(udata);
}

static inline int run_system_worker(void* arg)
{
    ecs_run_system(ecs, *(ecs_system_t*)arg, 0);
    return 0;
}

static inline double test_elapsed_seconds(struct timespec start, struct timespec end)
{
    return (double)(end.tv_sec - start.tv_sec) +
           (double)(end.tv_nsec - start.tv_nsec) / 1e9;
}
