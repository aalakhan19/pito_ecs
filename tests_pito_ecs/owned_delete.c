#include "threads.h"

#define TEST_OWNED_DELETE_COUNT              4000
#define TEST_OWNED_DELETE_CAPACITY           16384

#define TEST_OWNED_DELETE_SPEEDUP_COUNT      200000
#define TEST_OWNED_DELETE_SPEEDUP_CAPACITY   400000
#define TEST_OWNED_DELETE_SPEEDUP_WORK_ITERS 500

typedef struct
{
    ecs_comp_t comp;
    size_t     work_iterations;
} owned_delete_ctx_t;

static ecs_ret_t owned_delete_system(ecs_t* ecs,
                                     ecs_entity_t* entities,
                                     size_t entity_count,
                                     void* udata)
{
    owned_delete_ctx_t* ctx = (owned_delete_ctx_t*)udata;

    for (size_t i = 0; i < entity_count; i++)
    {
        volatile double acc = 0.0;
        for (size_t j = 0; j < ctx->work_iterations; j++)
            acc += (double)j * 1.0000001;
        (void)acc;

        ecs_remove_owned(ecs, entities[i], ctx->comp);
    }

    return 0;
}

static ecs_ret_t deferred_delete_system(ecs_t* ecs,
                                        ecs_entity_t* entities,
                                        size_t entity_count,
                                        void* udata)
{
    owned_delete_ctx_t* ctx = (owned_delete_ctx_t*)udata;

    for (size_t i = 0; i < entity_count; i++)
    {
        volatile double acc = 0.0;
        for (size_t j = 0; j < ctx->work_iterations; j++)
            acc += (double)j * 1.0000001;
        (void)acc;

        ecs_remove(ecs, entities[i], ctx->comp);
    }

    return 0;
}

TEST_CASE(test_owned_delete_parallel_correctness)
{
    ecs_free(ecs);
    ecs = ecs_new(TEST_OWNED_DELETE_CAPACITY, NULL);

    comp1 = ecs_define_component(ecs, sizeof(comp_t), NULL);
    comp2 = ecs_define_component(ecs, sizeof(comp_t), NULL);

    static ecs_entity_t entities[TEST_OWNED_DELETE_COUNT];

    owned_delete_ctx_t ctx = { .comp = comp1, .work_iterations = 0 };

    sys1 = ecs_define_system(ecs, owned_delete_system,
                             &(ecs_sys_desc_t){ .owned_delete = true, .udata = &ctx });
    ecs_require(ecs, sys1, comp1);

    for (int i = 0; i < TEST_OWNED_DELETE_COUNT; i++)
    {
        entities[i] = ecs_create(ecs);
        ecs_add(ecs, entities[i], comp1, NULL);

        if (0 == i % 2)
            ecs_add(ecs, entities[i], comp2, NULL);
    }

    REQUIRE(TEST_OWNED_DELETE_COUNT == (int)ecs_get_entity_count(ecs, sys1));

    ecs_run_system(ecs, sys1, 0);

    for (int i = 0; i < TEST_OWNED_DELETE_COUNT; i++)
        REQUIRE(!ecs_has(ecs, entities[i], comp1));

    ecs_sync_owned_delete(ecs);

    REQUIRE(0 == (int)ecs_get_entity_count(ecs, sys1));

    ecs_run_system(ecs, sys1, 0);
    ecs_sync_owned_delete(ecs);

    REQUIRE(0 == (int)ecs_get_entity_count(ecs, sys1));

    return true;
}

TEST_CASE(test_owned_delete_join_is_deferred)
{
    ecs_free(ecs);
    ecs = ecs_new(TEST_OWNED_DELETE_CAPACITY, NULL);

    comp1 = ecs_define_component(ecs, sizeof(comp_t), NULL);
    comp2 = ecs_define_component(ecs, sizeof(comp_t), NULL);

    static ecs_entity_t entities[TEST_OWNED_DELETE_COUNT];

    owned_delete_ctx_t ctx = { .comp = comp2, .work_iterations = 0 };

    sys1 = ecs_define_system(ecs, owned_delete_system,
                             &(ecs_sys_desc_t){ .owned_delete = true, .udata = &ctx });
    ecs_require(ecs, sys1, comp2);

    sys2 = ecs_define_system(ecs, noop_system, NULL);
    ecs_require(ecs, sys2, comp1);
    ecs_exclude(ecs, sys2, comp2);

    for (int i = 0; i < TEST_OWNED_DELETE_COUNT; i++)
    {
        entities[i] = ecs_create(ecs);
        ecs_add(ecs, entities[i], comp1, NULL);

        if (0 == i % 2)
            ecs_add(ecs, entities[i], comp2, NULL);
    }

    REQUIRE(TEST_OWNED_DELETE_COUNT / 2 == (int)ecs_get_entity_count(ecs, sys2));

    ecs_run_system(ecs, sys1, 0);

    REQUIRE(!ecs_has(ecs, entities[0], comp2));
    REQUIRE(TEST_OWNED_DELETE_COUNT / 2 == (int)ecs_get_entity_count(ecs, sys2));

    ecs_sync_owned_delete(ecs);

    REQUIRE(0 == (int)ecs_get_entity_count(ecs, sys1));
    REQUIRE(TEST_OWNED_DELETE_COUNT == (int)ecs_get_entity_count(ecs, sys2));

    return true;
}

TEST_CASE(test_owned_delete_parallel_disjoint_components)
{
    ecs_free(ecs);
    ecs = ecs_new(TEST_OWNED_DELETE_CAPACITY, NULL);

    comp1 = ecs_define_component(ecs, sizeof(comp_t), NULL);
    comp2 = ecs_define_component(ecs, sizeof(comp_t), NULL);

    static ecs_entity_t entities[TEST_OWNED_DELETE_COUNT];

    owned_delete_ctx_t ctx1 = { .comp = comp1, .work_iterations = 0 };
    owned_delete_ctx_t ctx2 = { .comp = comp2, .work_iterations = 0 };

    sys1 = ecs_define_system(ecs, owned_delete_system,
                             &(ecs_sys_desc_t){ .owned_delete = true, .udata = &ctx1 });
    ecs_require(ecs, sys1, comp1);

    sys2 = ecs_define_system(ecs, owned_delete_system,
                             &(ecs_sys_desc_t){ .owned_delete = true, .udata = &ctx2 });
    ecs_require(ecs, sys2, comp2);

    for (int i = 0; i < TEST_OWNED_DELETE_COUNT; i++)
    {
        entities[i] = ecs_create(ecs);
        ecs_add(ecs, entities[i], comp1, NULL);
        ecs_add(ecs, entities[i], comp2, NULL);
    }

    test_thread_t t1, t2;

    REQUIRE(TEST_THREAD_OK == TEST_THREAD_CREATE(&t1, run_system_worker, &sys1));
    REQUIRE(TEST_THREAD_OK == TEST_THREAD_CREATE(&t2, run_system_worker, &sys2));

    TEST_THREAD_JOIN(t1);
    TEST_THREAD_JOIN(t2);

    ecs_sync_owned_delete(ecs);

    REQUIRE(0 == ecs_get_entity_count(ecs, sys1));
    REQUIRE(0 == ecs_get_entity_count(ecs, sys2));

    for (int i = 0; i < TEST_OWNED_DELETE_COUNT; i++)
    {
        REQUIRE(!ecs_has(ecs, entities[i], comp1));
        REQUIRE(!ecs_has(ecs, entities[i], comp2));
    }

    return true;
}

static void owned_delete_speedup_setup(ecs_system_fn cb, bool owned,
                                       owned_delete_ctx_t* ctx_a, owned_delete_ctx_t* ctx_b,
                                       ecs_system_t* sys_a, ecs_system_t* sys_b)
{
    ecs_free(ecs);
    ecs = ecs_new(TEST_OWNED_DELETE_SPEEDUP_CAPACITY, NULL);

    ctx_a->comp = ecs_define_component(ecs, sizeof(comp_t), NULL);
    ctx_b->comp = ecs_define_component(ecs, sizeof(comp_t), NULL);

    *sys_a = ecs_define_system(ecs, cb, &(ecs_sys_desc_t){ .owned_delete = owned, .udata = ctx_a });
    *sys_b = ecs_define_system(ecs, cb, &(ecs_sys_desc_t){ .owned_delete = owned, .udata = ctx_b });

    ecs_require(ecs, *sys_a, ctx_a->comp);
    ecs_require(ecs, *sys_b, ctx_b->comp);

    for (int i = 0; i < TEST_OWNED_DELETE_SPEEDUP_COUNT; i++)
    {
        ecs_entity_t entity = ecs_create(ecs);
        ecs_add(ecs, entity, ctx_a->comp, NULL);
        ecs_add(ecs, entity, ctx_b->comp, NULL);
    }
}

static bool owned_delete_speedup_run(size_t work_iterations)
{
    owned_delete_ctx_t ctx_a = { .work_iterations = work_iterations };
    owned_delete_ctx_t ctx_b = { .work_iterations = work_iterations };
    ecs_system_t def1, def2;

    // baseline
    owned_delete_speedup_setup(deferred_delete_system, false, &ctx_a, &ctx_b, &def1, &def2);

    struct timespec t0, t1;

    timespec_get(&t0, TIME_UTC);
    ecs_run_system(ecs, def1, 0);
    ecs_run_system(ecs, def2, 0);
    timespec_get(&t1, TIME_UTC);

    double deferred_s = test_elapsed_seconds(t0, t1);

    // owned one trhead
    owned_delete_speedup_setup(owned_delete_system, true, &ctx_a, &ctx_b, &sys1, &sys2);

    struct timespec t2, t3, t4;

    timespec_get(&t2, TIME_UTC);
    ecs_run_system(ecs, sys1, 0);
    ecs_run_system(ecs, sys2, 0);
    timespec_get(&t3, TIME_UTC);
    ecs_sync_owned_delete(ecs);
    timespec_get(&t4, TIME_UTC);

    double serial_remove_s = test_elapsed_seconds(t2, t3);
    double serial_sync_s = test_elapsed_seconds(t3, t4);

    // owned 2 threads
    owned_delete_speedup_setup(owned_delete_system, true, &ctx_a, &ctx_b, &sys1, &sys2);

    test_thread_t th1, th2;
    struct timespec t5, t6, t7;

    timespec_get(&t5, TIME_UTC);
    REQUIRE(TEST_THREAD_OK == TEST_THREAD_CREATE(&th1, run_system_worker, &sys1));
    REQUIRE(TEST_THREAD_OK == TEST_THREAD_CREATE(&th2, run_system_worker, &sys2));

    TEST_THREAD_JOIN(th1);
    TEST_THREAD_JOIN(th2);
    timespec_get(&t6, TIME_UTC);
    ecs_sync_owned_delete(ecs);
    timespec_get(&t7, TIME_UTC);

    double parallel_remove_s = test_elapsed_seconds(t5, t6);
    double parallel_sync_s   = test_elapsed_seconds(t6, t7);

    double serial_s  = serial_remove_s + serial_sync_s;
    double parallel_s = parallel_remove_s + parallel_sync_s;

    printf("owned_delete, %d entities x 2 systems, work_iterations=%zu:\n"
           "    deferred = %.4fs\n"
           "    owned    (1 thread)  remove=%.4fs sync=%.4fs total=%.4fs (%.2fx vs deferred)\n"
           "    owned    (2 threads) remove=%.4fs sync=%.4fs total=%.4fs (%.2fx vs deferred, %.2fx vs owned serial)\n",
           TEST_OWNED_DELETE_SPEEDUP_COUNT, work_iterations,
           deferred_s,
           serial_remove_s, serial_sync_s, serial_s,
           serial_s > 0.0 ? deferred_s / serial_s : 0.0,
           parallel_remove_s, parallel_sync_s, parallel_s,
           parallel_s > 0.0 ? deferred_s / parallel_s : 0.0,
           parallel_s > 0.0 ? serial_s / parallel_s : 0.0);

    return true;
}


TEST_CASE(test_owned_delete_parallel_speedup_no_work)
{
    return owned_delete_speedup_run(0);
}

TEST_CASE(test_owned_delete_parallel_speedup)
{
    return owned_delete_speedup_run(TEST_OWNED_DELETE_SPEEDUP_WORK_ITERS);
}

TEST_SUITE(suite_owned_delete)
{
    RUN_TEST_CASE(test_owned_delete_parallel_correctness);
    RUN_TEST_CASE(test_owned_delete_join_is_deferred);
    RUN_TEST_CASE(test_owned_delete_parallel_disjoint_components);
    RUN_TEST_CASE(test_owned_delete_parallel_speedup_no_work);
    RUN_TEST_CASE(test_owned_delete_parallel_speedup);
}
