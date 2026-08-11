#include "threads.h"

#define TEST_OWNED_LOCAL_COUNT      1000
#define TEST_OWNED_LOCAL_CAPACITY   8192

#define TEST_OWNED_COUNTER_COUNT    2000
#define TEST_OWNED_COUNTER_SPINS    100000000

#define TEST_OWNED_BENCH_COUNT      100000
#define TEST_OWNED_BENCH_CAPACITY   250000
#define TEST_OWNED_BENCH_ROUNDS     4

typedef struct
{
    int counter;
} counter_comp_t;

typedef struct
{
    int           count;
    ecs_comp_t    comp;
    ecs_entity_t* out;
} owned_local_ctx_t;

static ecs_ret_t owned_local_system(ecs_t* ecs,
                                    ecs_entity_t* entities,
                                    size_t entity_count,
                                    void* udata)
{
    (void)entities;
    (void)entity_count;

    owned_local_ctx_t* ctx = (owned_local_ctx_t*)udata;

    for (int i = 0; i < ctx->count; i++)
    {
        ecs_entity_t entity = ecs_create_owned_local(ecs);

        comp_t* c = (comp_t*)ecs_add_owned(ecs, entity, ctx->comp, NULL);
        c->used = true;

        if (ctx->out)
            ctx->out[i] = entity;
    }

    return 0;
}

typedef struct
{
    size_t seen;
    size_t runs;
} reader_ctx_t;

static ecs_ret_t reader_system(ecs_t* ecs,
                               ecs_entity_t* entities,
                               size_t entity_count,
                               void* udata)
{
    (void)ecs;
    (void)entities;

    reader_ctx_t* ctx = (reader_ctx_t*)udata;

    ctx->seen += entity_count;
    ctx->runs++;

    return 0;
}

TEST_CASE(test_owned_local)
{
    ecs_free(ecs);
    ecs = ecs_new(TEST_OWNED_LOCAL_CAPACITY, NULL);

    ecs_comp_t comp = ecs_define_component(ecs, sizeof(comp_t), NULL);

    static ecs_entity_t out1[TEST_OWNED_LOCAL_COUNT];
    static ecs_entity_t out2[TEST_OWNED_LOCAL_COUNT];

    owned_local_ctx_t ctx1 = { .count = TEST_OWNED_LOCAL_COUNT, .comp = comp, .out = out1 };
    owned_local_ctx_t ctx2 = { .count = TEST_OWNED_LOCAL_COUNT, .comp = comp, .out = out2 };

    sys1 = ecs_define_system(ecs, owned_local_system,
                             &(ecs_sys_desc_t){ .owned_initialize = true, .udata = &ctx1 });
    sys2 = ecs_define_system(ecs, owned_local_system,
                             &(ecs_sys_desc_t){ .owned_initialize = true, .udata = &ctx2 });

    reader_ctx_t reader = { 0, 0 };
    reader_ctx_t blind  = { 0, 0 };

    ecs_system_t consumer = ecs_define_system(ecs, reader_system,
                                              &(ecs_sys_desc_t){ .reads_owned = true,
                                                                 .udata = &reader });
    ecs_require(ecs, consumer, comp);

    ecs_system_t no_opt_in = ecs_define_system(ecs, reader_system,
                                               &(ecs_sys_desc_t){ .udata = &blind });
    ecs_require(ecs, no_opt_in, comp);

    test_thread_t t1, t2;

    REQUIRE(TEST_THREAD_OK == TEST_THREAD_CREATE(&t1, run_system_worker, &sys1));
    REQUIRE(TEST_THREAD_OK == TEST_THREAD_CREATE(&t2, run_system_worker, &sys2));

    TEST_THREAD_JOIN(t1);
    TEST_THREAD_JOIN(t2);

    REQUIRE(0 == ecs_get_entity_count(ecs, consumer));

    ecs_run_system(ecs, consumer, 0);
    ecs_run_system(ecs, no_opt_in, 0);

    // One run for the empty storage plus one per local store
    REQUIRE(3 == reader.runs);
    REQUIRE(reader.seen == (size_t)(2 * TEST_OWNED_LOCAL_COUNT));

    REQUIRE(0 == blind.seen);

    static bool seen[TEST_OWNED_LOCAL_CAPACITY];
    memset(seen, 0, sizeof(seen));

    ecs_entity_t* outs[2] = { out1, out2 };

    for (int o = 0; o < 2; o++)
    {
        for (int i = 0; i < TEST_OWNED_LOCAL_COUNT; i++)
        {
            ecs_entity_t entity = outs[o][i];

            REQUIRE(ecs_is_ready(ecs, entity));
            REQUIRE(ecs_has(ecs, entity, comp));

            comp_t* c = (comp_t*)ecs_get(ecs, entity, comp);
            REQUIRE(c->used);

            REQUIRE(!seen[entity.id]);
            seen[entity.id] = true;
        }
    }

    ecs_flush_owned(ecs);

    REQUIRE(ecs_get_entity_count(ecs, consumer) == (size_t)(2 * TEST_OWNED_LOCAL_COUNT));

    REQUIRE(0 == ecs_get_entity_count(ecs, sys1));
    REQUIRE(0 == ecs_get_entity_count(ecs, sys2));

    reader.seen = 0;
    reader.runs = 0;
    blind.seen  = 0;

    ecs_run_system(ecs, consumer, 0);
    ecs_run_system(ecs, no_opt_in, 0);

    // The local stores are empty
    REQUIRE(1 == reader.runs);
    REQUIRE(reader.seen == (size_t)(2 * TEST_OWNED_LOCAL_COUNT));
    REQUIRE(blind.seen == (size_t)(2 * TEST_OWNED_LOCAL_COUNT));

    ecs_entity_t* entities = ecs_get_entity_array(ecs, consumer);

    ecs_destroy(ecs, entities[0]);

    REQUIRE(ecs_get_entity_count(ecs, consumer) == (size_t)(2 * TEST_OWNED_LOCAL_COUNT - 1));

    return true;
}

TEST_CASE(test_owned_local_reader_skips_non_matching)
{
    ecs_free(ecs);
    ecs = ecs_new(TEST_OWNED_LOCAL_CAPACITY, NULL);

    ecs_comp_t comp  = ecs_define_component(ecs, sizeof(comp_t), NULL);
    ecs_comp_t other = ecs_define_component(ecs, sizeof(comp_t), NULL);

    owned_local_ctx_t ctx1 = { .count = TEST_OWNED_LOCAL_COUNT, .comp = comp };

    sys1 = ecs_define_system(ecs, owned_local_system,
                             &(ecs_sys_desc_t){ .owned_initialize = true, .udata = &ctx1 });

    reader_ctx_t matching = { 0, 0 };
    reader_ctx_t missing  = { 0, 0 };
    reader_ctx_t excluded = { 0, 0 };

    ecs_system_t sys_matching = ecs_define_system(ecs, reader_system,
                                                  &(ecs_sys_desc_t){ .reads_owned = true,
                                                                     .udata = &matching });
    ecs_require(ecs, sys_matching, comp);

    ecs_system_t sys_missing = ecs_define_system(ecs, reader_system,
                                                 &(ecs_sys_desc_t){ .reads_owned = true,
                                                                    .udata = &missing });
    ecs_require(ecs, sys_missing, other);

    ecs_system_t sys_excluded = ecs_define_system(ecs, reader_system,
                                                  &(ecs_sys_desc_t){ .reads_owned = true,
                                                                     .udata = &excluded });
    ecs_exclude(ecs, sys_excluded, comp);

    test_thread_t t1;

    REQUIRE(TEST_THREAD_OK == TEST_THREAD_CREATE(&t1, run_system_worker, &sys1));
    TEST_THREAD_JOIN(t1);

    ecs_run_system(ecs, sys_matching, 0);
    ecs_run_system(ecs, sys_missing, 0);
    ecs_run_system(ecs, sys_excluded, 0);

    REQUIRE(matching.seen == (size_t)TEST_OWNED_LOCAL_COUNT);
    REQUIRE(0 == missing.seen);
    REQUIRE(0 == excluded.seen);

    return true;
}

typedef struct
{
    ecs_comp_t comp;
    size_t     seen;
} counter_ctx_t;

static ecs_ret_t counter_spawn_system(ecs_t* ecs,
                                      ecs_entity_t* entities,
                                      size_t entity_count,
                                      void* udata)
{
    (void)entities;
    (void)entity_count;

    owned_local_ctx_t* ctx = (owned_local_ctx_t*)udata;

    for (int i = 0; i < ctx->count; i++)
    {
        ecs_entity_t entity = ecs_create_owned_local(ecs);

        ecs_add_owned(ecs, entity, ctx->comp, NULL);

        ctx->out[i] = entity;
    }

    return 0;
}

static ecs_ret_t counter_update_system(ecs_t* ecs,
                                       ecs_entity_t* entities,
                                       size_t entity_count,
                                       void* udata)
{
    counter_ctx_t* ctx = (counter_ctx_t*)udata;

    for (size_t i = 0; i < entity_count; i++)
    {
        counter_comp_t* c = (counter_comp_t*)ecs_get(ecs, entities[i], ctx->comp);
        c->counter++;
    }

    ctx->seen += entity_count;

    return 0;
}

static int counter_update_worker(void* arg)
{
    counter_ctx_t* ctx = (counter_ctx_t*)arg;

    for (size_t i = 0; i < TEST_OWNED_COUNTER_SPINS; i++)
    {
        ctx->seen = 0;
        ecs_run_system(ecs, sys2, 0);

        if (ctx->seen >= (size_t)TEST_OWNED_COUNTER_COUNT)
            break;
    }

    return 0;
}

TEST_CASE(test_owned_local_concurrent_update)
{
    ecs_free(ecs);
    ecs = ecs_new(TEST_OWNED_LOCAL_CAPACITY, NULL);

    ecs_comp_t comp = ecs_define_component(ecs, sizeof(counter_comp_t), NULL);

    static ecs_entity_t out[TEST_OWNED_COUNTER_COUNT];

    owned_local_ctx_t spawn_ctx = { .count = TEST_OWNED_COUNTER_COUNT, .comp = comp, .out = out };
    counter_ctx_t update_ctx = { .comp = comp, .seen = 0 };

    sys1 = ecs_define_system(ecs, counter_spawn_system,
                             &(ecs_sys_desc_t){ .owned_initialize = true, .udata = &spawn_ctx });

    sys2 = ecs_define_system(ecs, counter_update_system,
                             &(ecs_sys_desc_t){ .owned_update = true,
                                                .reads_owned = true,
                                                .udata = &update_ctx });
    ecs_require(ecs, sys2, comp);

    test_thread_t t1, t2;

    REQUIRE(TEST_THREAD_OK == TEST_THREAD_CREATE(&t1, run_system_worker, &sys1));
    REQUIRE(TEST_THREAD_OK == TEST_THREAD_CREATE(&t2, counter_update_worker, &update_ctx));

    TEST_THREAD_JOIN(t1);
    TEST_THREAD_JOIN(t2);

    REQUIRE(update_ctx.seen == (size_t)TEST_OWNED_COUNTER_COUNT);

    int first = ((counter_comp_t*)ecs_get(ecs, out[0], comp))->counter;
    int prev  = first;

    for (int i = 0; i < TEST_OWNED_COUNTER_COUNT; i++)
    {
        counter_comp_t* c = (counter_comp_t*)ecs_get(ecs, out[i], comp);

        // components in the begginign of the store cannot have a lower counter then at the end
        REQUIRE(c->counter >= 1);
        REQUIRE(c->counter <= prev);

        prev = c->counter;
    }

    ecs_flush_owned(ecs);

    REQUIRE(ecs_get_entity_count(ecs, sys2) == (size_t)TEST_OWNED_COUNTER_COUNT);

    printf("concurrent update: counters between %d and %d\n", prev, first);

    return true;
}

TEST_CASE(test_owned_local_publish_at_end)
{
    ecs_free(ecs);
    ecs = ecs_new(TEST_OWNED_LOCAL_CAPACITY, NULL);

    ecs_comp_t comp = ecs_define_component(ecs, sizeof(counter_comp_t), NULL);

    static ecs_entity_t out[TEST_OWNED_COUNTER_COUNT];

    owned_local_ctx_t spawn_ctx = { .count = TEST_OWNED_COUNTER_COUNT, .comp = comp, .out = out };
    counter_ctx_t update_ctx = { .comp = comp, .seen = 0 };

    sys1 = ecs_define_system(ecs, counter_spawn_system,
                             &(ecs_sys_desc_t){ .owned_initialize = true,
                                                .owned_publish_at_end = true,
                                                .udata = &spawn_ctx });

    sys2 = ecs_define_system(ecs, counter_update_system,
                             &(ecs_sys_desc_t){ .owned_update = true,
                                                .reads_owned = true,
                                                .udata = &update_ctx });
    ecs_require(ecs, sys2, comp);

    test_thread_t t1, t2;

    REQUIRE(TEST_THREAD_OK == TEST_THREAD_CREATE(&t1, run_system_worker, &sys1));
    REQUIRE(TEST_THREAD_OK == TEST_THREAD_CREATE(&t2, counter_update_worker, &update_ctx));

    TEST_THREAD_JOIN(t1);
    TEST_THREAD_JOIN(t2);

    REQUIRE(update_ctx.seen == (size_t)TEST_OWNED_COUNTER_COUNT);

    for (int i = 0; i < TEST_OWNED_COUNTER_COUNT; i++)
    {
        counter_comp_t* c = (counter_comp_t*)ecs_get(ecs, out[i], comp);

        REQUIRE(1 == c->counter);
    }

    return true;
}

static ecs_ret_t bench_normal_system(ecs_t* ecs,
                                     ecs_entity_t* entities,
                                     size_t entity_count,
                                     void* udata)
{
    (void)entities;
    (void)entity_count;

    owned_local_ctx_t* ctx = (owned_local_ctx_t*)udata;

    for (int i = 0; i < ctx->count; i++)
    {
        ecs_entity_t entity = ecs_create(ecs);
        ecs_add(ecs, entity, ctx->comp, NULL);
    }

    return 0;
}

static ecs_ret_t bench_sync_system(ecs_t* ecs,
                                   ecs_entity_t* entities,
                                   size_t entity_count,
                                   void* udata)
{
    (void)entities;
    (void)entity_count;

    owned_local_ctx_t* ctx = (owned_local_ctx_t*)udata;

    for (int i = 0; i < ctx->count; i++)
    {
        ecs_entity_t entity = ecs_create_owned_sharded(ecs);

        ecs_add_owned(ecs, entity, ctx->comp, NULL);

        ctx->out[i] = entity;
    }

    return 0;
}

static ecs_ret_t bench_local_system(ecs_t* ecs,
                                    ecs_entity_t* entities,
                                    size_t entity_count,
                                    void* udata)
{
    (void)entities;
    (void)entity_count;

    owned_local_ctx_t* ctx = (owned_local_ctx_t*)udata;

    for (int i = 0; i < ctx->count; i++)
    {
        ecs_entity_t entity = ecs_create_owned_local(ecs);

        ecs_add_owned(ecs, entity, ctx->comp, NULL);
    }

    return 0;
}

TEST_CASE(compare_owned_local_speedup)
{
    int per_round = TEST_OWNED_BENCH_COUNT / TEST_OWNED_BENCH_ROUNDS;

    size_t bytes = per_round * sizeof(ecs_entity_t);
    ecs_entity_t* out1 = (ecs_entity_t*)malloc(bytes);
    ecs_entity_t* out2 = (ecs_entity_t*)malloc(bytes);

    REQUIRE(NULL != out1 && NULL != out2);

    ecs_comp_t comp;
    ecs_system_t consumer;
    counter_ctx_t update_ctx;
    owned_local_ctx_t ctx1, ctx2;
    struct timespec start, mid, end;
    test_thread_t t1, t2;

    double normal_build = 0.0, normal_consume = 0.0;
    double sync_build   = 0.0, sync_consume   = 0.0;
    double local_build  = 0.0, local_consume  = 0.0;
    double flush_s      = 0.0;

    // normal
    ecs_free(ecs);
    ecs = ecs_new(TEST_OWNED_BENCH_CAPACITY, NULL);

    comp = ecs_define_component(ecs, sizeof(counter_comp_t), NULL);

    ctx1 = (owned_local_ctx_t){ .count = per_round, .comp = comp, .out = out1 };
    ctx2 = (owned_local_ctx_t){ .count = per_round, .comp = comp, .out = out2 };
    update_ctx = (counter_ctx_t){ .comp = comp, .seen = 0 };

    sys1 = ecs_define_system(ecs, bench_normal_system, &(ecs_sys_desc_t){ .udata = &ctx1 });
    sys2 = ecs_define_system(ecs, bench_normal_system, &(ecs_sys_desc_t){ .udata = &ctx2 });

    consumer = ecs_define_system(ecs, counter_update_system,
                                 &(ecs_sys_desc_t){ .owned_update = true,
                                                    .reads_owned = true,
                                                    .udata = &update_ctx });
    ecs_require(ecs, consumer, comp);

    for (int round = 0; round < TEST_OWNED_BENCH_ROUNDS; round++)
    {
        timespec_get(&start, TIME_UTC);
        REQUIRE(TEST_THREAD_OK == TEST_THREAD_CREATE(&t1, run_system_worker, &sys1));
        REQUIRE(TEST_THREAD_OK == TEST_THREAD_CREATE(&t2, run_system_worker, &sys2));
        TEST_THREAD_JOIN(t1);
        TEST_THREAD_JOIN(t2);
        timespec_get(&mid, TIME_UTC);

        update_ctx.seen = 0;
        ecs_run_system(ecs, consumer, 0);
        timespec_get(&end, TIME_UTC);

        normal_build   += test_elapsed_seconds(start, mid);
        normal_consume += test_elapsed_seconds(mid, end);
    }

    REQUIRE(update_ctx.seen == (size_t)(2 * TEST_OWNED_BENCH_COUNT));

    // owned + sync_owned
    ecs_free(ecs);
    ecs = ecs_new(TEST_OWNED_BENCH_CAPACITY, NULL);

    comp = ecs_define_component(ecs, sizeof(counter_comp_t), NULL);

    ctx1 = (owned_local_ctx_t){ .count = per_round, .comp = comp, .out = out1 };
    ctx2 = (owned_local_ctx_t){ .count = per_round, .comp = comp, .out = out2 };
    update_ctx = (counter_ctx_t){ .comp = comp, .seen = 0 };

    sys1 = ecs_define_system(ecs, bench_sync_system,
                             &(ecs_sys_desc_t){ .owned_initialize = true, .udata = &ctx1 });
    sys2 = ecs_define_system(ecs, bench_sync_system,
                             &(ecs_sys_desc_t){ .owned_initialize = true, .udata = &ctx2 });

    consumer = ecs_define_system(ecs, counter_update_system,
                                 &(ecs_sys_desc_t){ .owned_update = true,
                                                    .reads_owned = true,
                                                    .udata = &update_ctx });
    ecs_require(ecs, consumer, comp);

    for (int round = 0; round < TEST_OWNED_BENCH_ROUNDS; round++)
    {
        timespec_get(&start, TIME_UTC);
        REQUIRE(TEST_THREAD_OK == TEST_THREAD_CREATE(&t1, run_system_worker, &sys1));
        REQUIRE(TEST_THREAD_OK == TEST_THREAD_CREATE(&t2, run_system_worker, &sys2));
        TEST_THREAD_JOIN(t1);
        TEST_THREAD_JOIN(t2);

        ecs_sync_owned(ecs, out1, per_round);
        ecs_sync_owned(ecs, out2, per_round);
        timespec_get(&mid, TIME_UTC);

        update_ctx.seen = 0;
        ecs_run_system(ecs, consumer, 0);
        timespec_get(&end, TIME_UTC);

        sync_build   += test_elapsed_seconds(start, mid);
        sync_consume += test_elapsed_seconds(mid, end);
    }

    REQUIRE(update_ctx.seen == (size_t)(2 * TEST_OWNED_BENCH_COUNT));

    // local stores
    ecs_free(ecs);
    ecs = ecs_new(TEST_OWNED_BENCH_CAPACITY, NULL);

    comp = ecs_define_component(ecs, sizeof(counter_comp_t), NULL);

    ctx1 = (owned_local_ctx_t){ .count = per_round, .comp = comp };
    ctx2 = (owned_local_ctx_t){ .count = per_round, .comp = comp };
    update_ctx = (counter_ctx_t){ .comp = comp, .seen = 0 };

    sys1 = ecs_define_system(ecs, bench_local_system,
                             &(ecs_sys_desc_t){ .owned_initialize = true, .udata = &ctx1 });
    sys2 = ecs_define_system(ecs, bench_local_system,
                             &(ecs_sys_desc_t){ .owned_initialize = true, .udata = &ctx2 });

    consumer = ecs_define_system(ecs, counter_update_system,
                                 &(ecs_sys_desc_t){ .owned_update = true,
                                                    .reads_owned = true,
                                                    .udata = &update_ctx });
    ecs_require(ecs, consumer, comp);

    for (int round = 0; round < TEST_OWNED_BENCH_ROUNDS; round++)
    {
        timespec_get(&start, TIME_UTC);
        REQUIRE(TEST_THREAD_OK == TEST_THREAD_CREATE(&t1, run_system_worker, &sys1));
        REQUIRE(TEST_THREAD_OK == TEST_THREAD_CREATE(&t2, run_system_worker, &sys2));
        TEST_THREAD_JOIN(t1);
        TEST_THREAD_JOIN(t2);
        timespec_get(&mid, TIME_UTC);

        update_ctx.seen = 0;
        ecs_run_system(ecs, consumer, 0);
        timespec_get(&end, TIME_UTC);

        local_build   += test_elapsed_seconds(start, mid);
        local_consume += test_elapsed_seconds(mid, end);
    }

    REQUIRE(update_ctx.seen == (size_t)(2 * TEST_OWNED_BENCH_COUNT));

    timespec_get(&start, TIME_UTC);
    ecs_flush_owned(ecs);
    timespec_get(&end, TIME_UTC);

    flush_s = test_elapsed_seconds(start, end);

    double normal_s = normal_build + normal_consume;
    double sync_s   = sync_build + sync_consume;
    double local_s  = local_build + local_consume;

    printf("%d entities x 2 threads over %d rounds:\n"
           "    normal       build=%.4fs consume=%.4fs total=%.4fs\n"
           "    sync_owned   build=%.4fs consume=%.4fs total=%.4fs (%.2fx vs normal)\n"
           "    local stores build=%.4fs consume=%.4fs total=%.4fs (%.2fx vs normal, %.2fx vs sync_owned)\n"
           "    ecs_flush_owned afterwards = %.4fs\n",
           TEST_OWNED_BENCH_COUNT, TEST_OWNED_BENCH_ROUNDS,
           normal_build, normal_consume, normal_s,
           sync_build, sync_consume, sync_s,
           sync_s > 0.0 ? normal_s / sync_s : 0.0,
           local_build, local_consume, local_s,
           local_s > 0.0 ? normal_s / local_s : 0.0,
           local_s > 0.0 ? sync_s / local_s : 0.0,
           flush_s);

    free(out1);
    free(out2);

    return true;
}

TEST_SUITE(suite_owned_local)
{
    RUN_TEST_CASE(test_owned_local);
    RUN_TEST_CASE(test_owned_local_reader_skips_non_matching);
    RUN_TEST_CASE(test_owned_local_concurrent_update);
    RUN_TEST_CASE(test_owned_local_publish_at_end);
    RUN_TEST_CASE(compare_owned_local_speedup);
}
