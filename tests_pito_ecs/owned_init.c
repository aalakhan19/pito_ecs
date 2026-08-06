#include "threads.h"

#define TEST_OWNED_INITIALIZE_COUNT             1000
#define TEST_OWNED_INITIALIZE_CAPACITY          4096

#define TEST_OWNED_INITIALIZE_SPEEDUP_COUNT     500000
#define TEST_OWNED_INITIALIZE_SPEEDUP_CAPACITY  2000000

#define TEST_OWNED_INITIALIZE_WORK_COUNT        5000
#define TEST_OWNED_INITIALIZE_WORK_ITERS        500

#define TEST_OWNED_SHARDED_SHARD_SIZE           64

#define TEST_OWNED_SHARDED_SPEEDUP_CAPACITY     2100000

typedef struct
{
    int count;
    size_t work_iterations;
    size_t shard_size;
    ecs_entity_t* out;
} owned_initialize_ctx_t;

static ecs_ret_t owned_initialize_id_atomic_system(ecs_t* ecs,
                                         ecs_entity_t* entities,
                                         size_t entity_count,
                                         void* udata)
{
    (void)entities;
    (void)entity_count;

    owned_initialize_ctx_t* ctx = (owned_initialize_ctx_t*)udata;

    for (int i = 0; i < ctx->count; i++)
    {
        ecs_entity_t entity = ecs_create_owned(ecs);

        if (ctx->out)
            ctx->out[i] = entity;

        volatile double acc = 0.0;
        for (size_t j = 0; j < ctx->work_iterations; j++)
            acc += (double)j * 1.0000001;
        (void)acc;
    }

    return 0;
}

static ecs_ret_t owned_initialize_id_sharded_system(ecs_t* ecs,
                                                    ecs_entity_t* entities,
                                                    size_t entity_count,
                                                    void* udata)
{
    (void)entities;
    (void)entity_count;

    owned_initialize_ctx_t* ctx = (owned_initialize_ctx_t*)udata;

    for (int i = 0; i < ctx->count; i++)
    {
        ecs_entity_t entity = ecs_create_owned_sharded_n(ecs, ctx->shard_size);

        if (ctx->out)
            ctx->out[i] = entity;

        volatile double acc = 0.0;
        for (size_t j = 0; j < ctx->work_iterations; j++)
            acc += (double)j * 1.0000001;
        (void)acc;
    }

    return 0;
}

TEST_CASE(test_owned_initialize_id_parallel_correctness)
{
    ecs_free(ecs);
    ecs = ecs_new(TEST_OWNED_INITIALIZE_CAPACITY, NULL);

    ecs_entity_t out1[TEST_OWNED_INITIALIZE_COUNT];
    ecs_entity_t out2[TEST_OWNED_INITIALIZE_COUNT];

    owned_initialize_ctx_t ctx1 = { .count = TEST_OWNED_INITIALIZE_COUNT, .out = out1 };
    owned_initialize_ctx_t ctx2 = { .count = TEST_OWNED_INITIALIZE_COUNT, .out = out2 };

    sys1 = ecs_define_system(ecs, owned_initialize_id_atomic_system,
                             &(ecs_sys_desc_t){ .owned_initialize = true, .udata = &ctx1 });
    sys2 = ecs_define_system(ecs, owned_initialize_id_atomic_system,
                             &(ecs_sys_desc_t){ .owned_initialize = true, .udata = &ctx2 });

    test_thread_t t1, t2;

    REQUIRE(TEST_THREAD_OK == TEST_THREAD_CREATE(&t1, run_system_worker, &sys1));
    REQUIRE(TEST_THREAD_OK == TEST_THREAD_CREATE(&t2, run_system_worker, &sys2));

    TEST_THREAD_JOIN(t1);
    TEST_THREAD_JOIN(t2);

    bool seen[TEST_OWNED_INITIALIZE_COUNT * 2] = { false };

    for (int i = 0; i < TEST_OWNED_INITIALIZE_COUNT; i++)
    {
        REQUIRE(ecs_is_ready(ecs, out1[i]));
        REQUIRE(out1[i].id < (ecs_id_t)(TEST_OWNED_INITIALIZE_COUNT * 2));
        REQUIRE(!seen[out1[i].id]);
        seen[out1[i].id] = true;
    }

    for (int i = 0; i < TEST_OWNED_INITIALIZE_COUNT; i++)
    {
        REQUIRE(ecs_is_ready(ecs, out2[i]));
        REQUIRE(out2[i].id < (ecs_id_t)(TEST_OWNED_INITIALIZE_COUNT * 2));
        REQUIRE(!seen[out2[i].id]);
        seen[out2[i].id] = true;
    }

    for (int i = 0; i < TEST_OWNED_INITIALIZE_COUNT * 2; i++)
    {
        REQUIRE(seen[i]);
    }

    return true;
}

TEST_CASE(test_owned_initialize_id_parallel_speedup)
{
    ecs_free(ecs);
    ecs = ecs_new(TEST_OWNED_INITIALIZE_SPEEDUP_CAPACITY, NULL);

    owned_initialize_ctx_t ctx1 = { .count = TEST_OWNED_INITIALIZE_SPEEDUP_COUNT, .out = NULL };
    owned_initialize_ctx_t ctx2 = { .count = TEST_OWNED_INITIALIZE_SPEEDUP_COUNT, .out = NULL };

    sys1 = ecs_define_system(ecs, owned_initialize_id_atomic_system,
                             &(ecs_sys_desc_t){ .owned_initialize = true, .udata = &ctx1 });
    sys2 = ecs_define_system(ecs, owned_initialize_id_atomic_system,
                             &(ecs_sys_desc_t){ .owned_initialize = true, .udata = &ctx2 });

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

    printf("owned_initialize timing: serial=%.4fs parallel=%.4fs (%.2fx)\n",
           serial_s, parallel_s, parallel_s > 0.0 ? serial_s / parallel_s : 0.0);

    return true;
}

TEST_CASE(test_owned_initialize_id_parallel_speedup_with_work)
{
    ecs_free(ecs);
    ecs = ecs_new(TEST_OWNED_INITIALIZE_SPEEDUP_CAPACITY, NULL);

    owned_initialize_ctx_t ctx1 = { .count = TEST_OWNED_INITIALIZE_WORK_COUNT,
                                    .work_iterations = TEST_OWNED_INITIALIZE_WORK_ITERS,
                                    .out = NULL };
    owned_initialize_ctx_t ctx2 = { .count = TEST_OWNED_INITIALIZE_WORK_COUNT,
                                    .work_iterations = TEST_OWNED_INITIALIZE_WORK_ITERS,
                                    .out = NULL };

    sys1 = ecs_define_system(ecs, owned_initialize_id_atomic_system,
                             &(ecs_sys_desc_t){ .owned_initialize = true, .udata = &ctx1 });
    sys2 = ecs_define_system(ecs, owned_initialize_id_atomic_system,
                             &(ecs_sys_desc_t){ .owned_initialize = true, .udata = &ctx2 });

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

    printf("owned_initialize_id timing (with per-entity work): serial=%.4fs parallel=%.4fs (%.2fx)\n",
           serial_s, parallel_s, parallel_s > 0.0 ? serial_s / parallel_s : 0.0);

    return true;
}

TEST_CASE(test_owned_initialize_sharded_id_parallel_correctness)
{
    ecs_free(ecs);
    ecs = ecs_new(TEST_OWNED_INITIALIZE_CAPACITY, NULL);

    ecs_entity_t out1[TEST_OWNED_INITIALIZE_COUNT];
    ecs_entity_t out2[TEST_OWNED_INITIALIZE_COUNT];

    owned_initialize_ctx_t ctx1 = { .count = TEST_OWNED_INITIALIZE_COUNT,
                                    .shard_size = TEST_OWNED_SHARDED_SHARD_SIZE,
                                    .out = out1 };
    owned_initialize_ctx_t ctx2 = { .count = TEST_OWNED_INITIALIZE_COUNT,
                                    .shard_size = TEST_OWNED_SHARDED_SHARD_SIZE,
                                    .out = out2 };

    sys1 = ecs_define_system(ecs, owned_initialize_id_sharded_system,
                             &(ecs_sys_desc_t){ .owned_initialize = true, .udata = &ctx1 });
    sys2 = ecs_define_system(ecs, owned_initialize_id_sharded_system,
                             &(ecs_sys_desc_t){ .owned_initialize = true, .udata = &ctx2 });

    test_thread_t t1, t2;

    REQUIRE(TEST_THREAD_OK == TEST_THREAD_CREATE(&t1, run_system_worker, &sys1));
    REQUIRE(TEST_THREAD_OK == TEST_THREAD_CREATE(&t2, run_system_worker, &sys2));

    TEST_THREAD_JOIN(t1);
    TEST_THREAD_JOIN(t2);

    static bool seen[TEST_OWNED_INITIALIZE_CAPACITY];
    memset(seen, 0, sizeof(seen));

    ecs_entity_t* outs[2] = { out1, out2 };

    for (int o = 0; o < 2; o++)
    {
        for (int i = 0; i < TEST_OWNED_INITIALIZE_COUNT; i++)
        {
            ecs_entity_t entity = outs[o][i];

            REQUIRE(ecs_is_ready(ecs, entity));
            REQUIRE(entity.id < (ecs_id_t)TEST_OWNED_INITIALIZE_CAPACITY);
            REQUIRE(!seen[entity.id]);
            seen[entity.id] = true;
        }
    }

    return true;
}

TEST_CASE(test_owned_initialize_sharded_id_parallel_speedup)
{
    ecs_free(ecs);
    ecs = ecs_new(TEST_OWNED_SHARDED_SPEEDUP_CAPACITY, NULL);

    owned_initialize_ctx_t ctx1 = { .count = TEST_OWNED_INITIALIZE_SPEEDUP_COUNT,
                                    .shard_size = TEST_OWNED_SHARDED_SHARD_SIZE,
                                    .out = NULL };
    owned_initialize_ctx_t ctx2 = { .count = TEST_OWNED_INITIALIZE_SPEEDUP_COUNT,
                                    .shard_size = TEST_OWNED_SHARDED_SHARD_SIZE,
                                    .out = NULL };

    sys1 = ecs_define_system(ecs, owned_initialize_id_sharded_system,
                             &(ecs_sys_desc_t){ .owned_initialize = true, .udata = &ctx1 });
    sys2 = ecs_define_system(ecs, owned_initialize_id_sharded_system,
                             &(ecs_sys_desc_t){ .owned_initialize = true, .udata = &ctx2 });

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

    printf("owned_initialize_sharded timing: serial=%.4fs parallel=%.4fs (%.2fx)\n",
           serial_s, parallel_s, parallel_s > 0.0 ? serial_s / parallel_s : 0.0);

    return true;
}

TEST_CASE(test_owned_initialize_sharded_id_parallel_speedup_with_work)
{
    ecs_free(ecs);
    ecs = ecs_new(TEST_OWNED_SHARDED_SPEEDUP_CAPACITY, NULL);

    owned_initialize_ctx_t ctx1 = { .count = TEST_OWNED_INITIALIZE_WORK_COUNT,
                                    .work_iterations = TEST_OWNED_INITIALIZE_WORK_ITERS,
                                    .shard_size = TEST_OWNED_SHARDED_SHARD_SIZE,
                                    .out = NULL };
    owned_initialize_ctx_t ctx2 = { .count = TEST_OWNED_INITIALIZE_WORK_COUNT,
                                    .work_iterations = TEST_OWNED_INITIALIZE_WORK_ITERS,
                                    .shard_size = TEST_OWNED_SHARDED_SHARD_SIZE,
                                    .out = NULL };

    sys1 = ecs_define_system(ecs, owned_initialize_id_sharded_system,
                             &(ecs_sys_desc_t){ .owned_initialize = true, .udata = &ctx1 });
    sys2 = ecs_define_system(ecs, owned_initialize_id_sharded_system,
                             &(ecs_sys_desc_t){ .owned_initialize = true, .udata = &ctx2 });

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

    printf("owned_initialize_sharded timing (with per-entity work): serial=%.4fs parallel=%.4fs (%.2fx)\n",
           serial_s, parallel_s, parallel_s > 0.0 ? serial_s / parallel_s : 0.0);

    return true;
}
TEST_SUITE(suite_owned_init)
{
    RUN_TEST_CASE(test_owned_initialize_id_parallel_correctness);
    RUN_TEST_CASE(test_owned_initialize_id_parallel_speedup);
    RUN_TEST_CASE(test_owned_initialize_id_parallel_speedup_with_work);
    RUN_TEST_CASE(test_owned_initialize_sharded_id_parallel_correctness);
    RUN_TEST_CASE(test_owned_initialize_sharded_id_parallel_speedup);
    RUN_TEST_CASE(test_owned_initialize_sharded_id_parallel_speedup_with_work);
}
