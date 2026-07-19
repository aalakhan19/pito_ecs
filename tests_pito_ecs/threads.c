#include "common.h"

#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#if defined(__STDC_NO_THREADS__)
    // TEST_HAS_C11_THREADS intentionally left undefined; pthread fallback.
#elif defined(__has_include)
    #if __has_include(<threads.h>)
        #define TEST_HAS_C11_THREADS 1
    #endif
#elif !defined(__APPLE__)
    #define TEST_HAS_C11_THREADS 1
#endif

#if defined(TEST_HAS_C11_THREADS)

#include <threads.h>

typedef thrd_t test_thread_t;

#define TEST_THREAD_CREATE(t, fn, arg) thrd_create((t), (fn), (arg))
#define TEST_THREAD_JOIN(t)            thrd_join((t), NULL)
#define TEST_THREAD_OK                 thrd_success

#else // Fallback for platforms without C11 <threads.h>

#include <pthread.h>

typedef pthread_t test_thread_t;

typedef struct
{
    int (*fn)(void*);
    void* arg;
} test_trampoline_args_t;

static void* test_trampoline(void* p)
{
    test_trampoline_args_t* args = (test_trampoline_args_t*)p;
    args->fn(args->arg);
    free(args);
    return NULL;
}

// Small helper struct is heap-allocated per thread and freed by the
// trampoline once the worker returns; this is a test harness, not the
// library, so no further cleanup machinery is needed.
static int test_thread_create(test_thread_t* t, int (*fn)(void*), void* arg)
{
    test_trampoline_args_t* targs = (test_trampoline_args_t*)malloc(sizeof(*targs));
    targs->fn = fn;
    targs->arg = arg;
    return pthread_create(t, NULL, test_trampoline, targs) == 0 ? 0 : -1;
}

#define TEST_THREAD_CREATE(t, fn, arg) test_thread_create((t), (fn), (arg))
#define TEST_THREAD_JOIN(t)            pthread_join((t), NULL)
#define TEST_THREAD_OK                 0

#endif

// --- Helpers (suite_threads) --------------------------------------------

#define TEST_THREAD_COUNT     4
#define TEST_ENTITIES_PER_RUN 1000

static ecs_ret_t noop_system(ecs_t* ecs,
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


static int spawn_worker(void* arg)
{
    spawn_ctx_t* ctx = (spawn_ctx_t*)arg;

    for (int i = 0; i < ctx->count; i++)
    {
        ecs_entity_t entity = ecs_create(ecs);
        ecs_add(ecs, entity, ctx->comp, NULL);
    }

    return 0;
}

static ecs_ret_t spawn_system(ecs_t* ecs,
                              ecs_entity_t* entities,
                              size_t entity_count,
                              void* udata)
{
    (void)ecs;
    (void)entities;
    (void)entity_count;

    return spawn_worker(udata);
}

static int run_system_worker(void* arg)
{
    ecs_run_system(ecs, *(ecs_system_t*)arg, 0);
    return 0;
}

// --- Helpers (owned_update parallelism) ---------------------------------

#define TEST_OWNED_UPDATE_ENTITIES   20000
#define TEST_OWNED_UPDATE_WORK_ITERS 500

typedef struct
{
    ecs_comp_t comp;
    size_t     work_iterations;
} owned_update_ctx_t;

static ecs_ret_t owned_update_system(ecs_t* ecs,
                                     ecs_entity_t* entities,
                                     size_t entity_count,
                                     void* udata)
{
    owned_update_ctx_t* ctx = (owned_update_ctx_t*)udata;

    for (size_t i = 0; i < entity_count; i++)
    {
        comp_t* c = (comp_t*)ecs_get(ecs, entities[i], ctx->comp);

        volatile double acc = 0.0;
        for (size_t j = 0; j < ctx->work_iterations; j++)
            acc += (double)j * 1.0000001;
        (void)acc;

        c->used = true;
    }

    return 0;
}

static double test_elapsed_seconds(struct timespec start, struct timespec end)
{
    return (double)(end.tv_sec - start.tv_sec) +
           (double)(end.tv_nsec - start.tv_nsec) / 1e9;
}

TEST_CASE(test_concurrent_create_and_add)
{
    sys1 = ecs_define_system(ecs, noop_system, NULL);
    ecs_require(ecs, sys1, comp1);

    spawn_ctx_t ctx = { .comp = comp1, .count = TEST_ENTITIES_PER_RUN };

    test_thread_t threads[TEST_THREAD_COUNT];

    for (int i = 0; i < TEST_THREAD_COUNT; i++)
    {
        REQUIRE(TEST_THREAD_OK == TEST_THREAD_CREATE(&threads[i], spawn_worker, &ctx));
    }

    for (int i = 0; i < TEST_THREAD_COUNT; i++)
    {
        TEST_THREAD_JOIN(threads[i]);
    }

    REQUIRE(ecs_get_entity_count(ecs, sys1) == (size_t)(TEST_THREAD_COUNT * TEST_ENTITIES_PER_RUN));

    return true;
}


TEST_CASE(test_concurrent_run_system)
{
    spawn_ctx_t ctx1 = { .comp = comp1, .count = TEST_ENTITIES_PER_RUN };
    spawn_ctx_t ctx2 = { .comp = comp2, .count = TEST_ENTITIES_PER_RUN };

    sys1 = ecs_define_system(ecs, spawn_system, &(ecs_sys_desc_t){ .udata = &ctx1 });
    sys2 = ecs_define_system(ecs, spawn_system, &(ecs_sys_desc_t){ .udata = &ctx2 });

    ecs_require(ecs, sys1, comp1);
    ecs_require(ecs, sys2, comp2);

    test_thread_t t1, t2;

    REQUIRE(TEST_THREAD_OK == TEST_THREAD_CREATE(&t1, run_system_worker, &sys1));
    REQUIRE(TEST_THREAD_OK == TEST_THREAD_CREATE(&t2, run_system_worker, &sys2));

    TEST_THREAD_JOIN(t1);
    TEST_THREAD_JOIN(t2);

    REQUIRE(ecs_get_entity_count(ecs, sys1) == (size_t)TEST_ENTITIES_PER_RUN);
    REQUIRE(ecs_get_entity_count(ecs, sys2) == (size_t)TEST_ENTITIES_PER_RUN);

    return true;
}

TEST_CASE(test_owned_update_parallel_correctness)
{
    sys1 = ecs_define_system(ecs, owned_update_system, &(ecs_sys_desc_t){ .owned_update = true });
    sys2 = ecs_define_system(ecs, owned_update_system, &(ecs_sys_desc_t){ .owned_update = true });

    ecs_require(ecs, sys1, comp1);
    ecs_require(ecs, sys2, comp2);

    owned_update_ctx_t ctx1 = { .comp = comp1, .work_iterations = TEST_OWNED_UPDATE_WORK_ITERS };
    owned_update_ctx_t ctx2 = { .comp = comp2, .work_iterations = TEST_OWNED_UPDATE_WORK_ITERS };
    ecs_set_system_udata(ecs, sys1, &ctx1);
    ecs_set_system_udata(ecs, sys2, &ctx2);

    for (int i = 0; i < TEST_OWNED_UPDATE_ENTITIES; i++)
    {
        ecs_entity_t entity = ecs_create(ecs);
        ecs_add(ecs, entity, comp1, NULL);
        ecs_add(ecs, entity, comp2, NULL);
    }

    test_thread_t t1, t2;

    REQUIRE(TEST_THREAD_OK == TEST_THREAD_CREATE(&t1, run_system_worker, &sys1));
    REQUIRE(TEST_THREAD_OK == TEST_THREAD_CREATE(&t2, run_system_worker, &sys2));

    TEST_THREAD_JOIN(t1);
    TEST_THREAD_JOIN(t2);

    size_t count = ecs_get_entity_count(ecs, sys1);
    REQUIRE(count == (size_t)TEST_OWNED_UPDATE_ENTITIES);
    REQUIRE(ecs_get_entity_count(ecs, sys2) == (size_t)TEST_OWNED_UPDATE_ENTITIES);

    ecs_entity_t* entities = ecs_get_entity_array(ecs, sys1);

    for (size_t i = 0; i < count; i++)
    {
        REQUIRE(((comp_t*)ecs_get(ecs, entities[i], comp1))->used);
        REQUIRE(((comp_t*)ecs_get(ecs, entities[i], comp2))->used);
    }

    return true;
}


TEST_CASE(test_owned_update_parallel_speedup)
{
    sys1 = ecs_define_system(ecs, owned_update_system, &(ecs_sys_desc_t){ .owned_update = true });
    sys2 = ecs_define_system(ecs, owned_update_system, &(ecs_sys_desc_t){ .owned_update = true });

    ecs_require(ecs, sys1, comp1);
    ecs_require(ecs, sys2, comp2);

    owned_update_ctx_t ctx1 = { .comp = comp1, .work_iterations = TEST_OWNED_UPDATE_WORK_ITERS };
    owned_update_ctx_t ctx2 = { .comp = comp2, .work_iterations = TEST_OWNED_UPDATE_WORK_ITERS };
    ecs_set_system_udata(ecs, sys1, &ctx1);
    ecs_set_system_udata(ecs, sys2, &ctx2);

    for (int i = 0; i < TEST_OWNED_UPDATE_ENTITIES; i++)
    {
        ecs_entity_t entity = ecs_create(ecs);
        ecs_add(ecs, entity, comp1, NULL);
        ecs_add(ecs, entity, comp2, NULL);
    }

    struct timespec start, mid, end;

    timespec_get(&start, TIME_UTC);
    ecs_run_system(ecs, sys1, 0);
    ecs_run_system(ecs, sys2, 0);
    timespec_get(&mid, TIME_UTC);

    test_thread_t t1, t2;

    REQUIRE(TEST_THREAD_OK == TEST_THREAD_CREATE(&t1, run_system_worker, &sys1));
    REQUIRE(TEST_THREAD_OK == TEST_THREAD_CREATE(&t2, run_system_worker, &sys2));

    TEST_THREAD_JOIN(t1);
    TEST_THREAD_JOIN(t2);
    timespec_get(&end, TIME_UTC);

    double serial_s   = test_elapsed_seconds(start, mid);
    double parallel_s = test_elapsed_seconds(mid, end);

    printf("owned_update timing: serial=%.4fs parallel=%.4fs (%.2fx)\n",
           serial_s, parallel_s, parallel_s > 0.0 ? serial_s / parallel_s : 0.0);

    return true;
}

TEST_SUITE(suite_threads)
{
    RUN_TEST_CASE(test_concurrent_create_and_add);
    RUN_TEST_CASE(test_concurrent_run_system);
    RUN_TEST_CASE(test_owned_update_parallel_correctness);
    RUN_TEST_CASE(test_owned_update_parallel_speedup);
}
