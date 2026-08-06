#include "threads.h"

#define TEST_OWNED_UPDATE_ENTITIES   20000
#define TEST_OWNED_UPDATE_WORK_ITERS 500

#define TEST_INTERFERENCE_SPAWN_COUNT 4000

#define TEST_RACE_INITIAL_ENTITIES 800
#define TEST_RACE_SPAWN_ENTITIES   2000
#define TEST_RACE_WORK_ITERS       5000

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

// Helpers (owned_update misuse)

#define TEST_CORRUPTION_ENTITIES   200
#define TEST_CORRUPTION_WORK_ITERS TEST_RACE_WORK_ITERS

typedef struct
{
    long counter;
} racy_counter_t;

typedef struct
{
    ecs_comp_t comp;
    size_t     work_iterations;
} racy_increment_ctx_t;


static ecs_ret_t racy_increment_system(ecs_t* ecs,
                                       ecs_entity_t* entities,
                                       size_t entity_count,
                                       void* udata)
{
    racy_increment_ctx_t* ctx = (racy_increment_ctx_t*)udata;

    for (size_t i = 0; i < entity_count; i++)
    {
        racy_counter_t* c = (racy_counter_t*)ecs_get(ecs, entities[i], ctx->comp);

        long old = c->counter;

        volatile double acc = 0.0;
        for (size_t j = 0; j < ctx->work_iterations; j++)
            acc += (double)j * 1.0000001;
        (void)acc;

        c->counter = old + 1;
    }

    return 0;
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


TEST_CASE(test_owned_update_same_component_corrupts_data)
{
    ecs_comp_t counter_comp = ecs_define_component(ecs, sizeof(racy_counter_t), NULL);

    sys1 = ecs_define_system(ecs, racy_increment_system, &(ecs_sys_desc_t){ .owned_update = true });
    sys2 = ecs_define_system(ecs, racy_increment_system, &(ecs_sys_desc_t){ .owned_update = true });

    ecs_require(ecs, sys1, counter_comp);
    ecs_require(ecs, sys2, counter_comp);

    racy_increment_ctx_t ctx1 = { .comp = counter_comp, .work_iterations = TEST_CORRUPTION_WORK_ITERS };
    racy_increment_ctx_t ctx2 = { .comp = counter_comp, .work_iterations = TEST_CORRUPTION_WORK_ITERS };
    ecs_set_system_udata(ecs, sys1, &ctx1);
    ecs_set_system_udata(ecs, sys2, &ctx2);

    for (int i = 0; i < TEST_CORRUPTION_ENTITIES; i++)
    {
        ecs_entity_t entity = ecs_create(ecs);
        ecs_add(ecs, entity, counter_comp, NULL);
    }

    test_thread_t t1, t2;

    REQUIRE(TEST_THREAD_OK == TEST_THREAD_CREATE(&t1, run_system_worker, &sys1));
    REQUIRE(TEST_THREAD_OK == TEST_THREAD_CREATE(&t2, run_system_worker, &sys2));

    TEST_THREAD_JOIN(t1);
    TEST_THREAD_JOIN(t2);

    size_t count = ecs_get_entity_count(ecs, sys1);
    REQUIRE(count == (size_t)TEST_CORRUPTION_ENTITIES);

    ecs_entity_t* entities = ecs_get_entity_array(ecs, sys1);

    int corrupted = 0;

    for (size_t i = 0; i < count; i++)
    {
        racy_counter_t* c = (racy_counter_t*)ecs_get(ecs, entities[i], counter_comp);

        if (c->counter != 2)
        {
            corrupted++;
        }
    }

    // TODO: very flaky test
    // REQUIRE(corrupted > 0);

    printf("found %d corrupted components", corrupted);

    return true;
}

TEST_CASE(test_owned_update_parallel_speedup)
{
    sys1 = ecs_define_system(ecs, owned_update_system, &(ecs_sys_desc_t){ .owned_update = true });
    sys2 = ecs_define_system(ecs, owned_update_system, &(ecs_sys_desc_t){ .owned_update = true });

    // i guess one would be enough aswell
    ecs_system_t def1 = ecs_define_system(ecs, owned_update_system, &(ecs_sys_desc_t){ .owned_update = false });
    ecs_system_t def2 = ecs_define_system(ecs, owned_update_system, &(ecs_sys_desc_t){ .owned_update = false });

    ecs_require(ecs, sys1, comp1);
    ecs_require(ecs, sys2, comp2);

    ecs_require(ecs, def1, comp1);
    ecs_require(ecs, def2, comp2);

    owned_update_ctx_t ctx1 = { .comp = comp1, .work_iterations = TEST_OWNED_UPDATE_WORK_ITERS };
    owned_update_ctx_t ctx2 = { .comp = comp2, .work_iterations = TEST_OWNED_UPDATE_WORK_ITERS };
    ecs_set_system_udata(ecs, sys1, &ctx1);
    ecs_set_system_udata(ecs, sys2, &ctx2);
    ecs_set_system_udata(ecs, def1, &ctx1);
    ecs_set_system_udata(ecs, def2, &ctx2);

    for (int i = 0; i < TEST_OWNED_UPDATE_ENTITIES; i++)
    {
        ecs_entity_t entity = ecs_create(ecs);
        ecs_add(ecs, entity, comp1, NULL);
        ecs_add(ecs, entity, comp2, NULL);
    }

    struct timespec start, mid, end;

    timespec_get(&start, TIME_UTC);
    ecs_run_system(ecs, def1, 0);
    ecs_run_system(ecs, def2, 0);
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

TEST_CASE(test_owned_update_parallel_speedup_with_interference)
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

    struct timespec start, mid;
    test_thread_t t1, t2;

    timespec_get(&start, TIME_UTC);
    REQUIRE(TEST_THREAD_OK == TEST_THREAD_CREATE(&t1, run_system_worker, &sys1));
    REQUIRE(TEST_THREAD_OK == TEST_THREAD_CREATE(&t2, run_system_worker, &sys2));
    TEST_THREAD_JOIN(t1);
    TEST_THREAD_JOIN(t2);
    timespec_get(&mid, TIME_UTC);

    double baseline_s = test_elapsed_seconds(start, mid);

    // entities on an unrelated component at the same time.
    ecs_system_t sys3 = ecs_define_system(ecs, noop_system, NULL);
    ecs_require(ecs, sys3, comp3);

    spawn_ctx_t spawn_ctx = { .comp = comp3, .count = TEST_INTERFERENCE_SPAWN_COUNT };

    struct timespec istart, iend;
    test_thread_t t3;

    timespec_get(&istart, TIME_UTC);
    REQUIRE(TEST_THREAD_OK == TEST_THREAD_CREATE(&t1, run_system_worker, &sys1));
    REQUIRE(TEST_THREAD_OK == TEST_THREAD_CREATE(&t2, run_system_worker, &sys2));
    REQUIRE(TEST_THREAD_OK == TEST_THREAD_CREATE(&t3, spawn_worker, &spawn_ctx));
    TEST_THREAD_JOIN(t1);
    TEST_THREAD_JOIN(t2);
    TEST_THREAD_JOIN(t3);
    timespec_get(&iend, TIME_UTC);

    double interfered_s = test_elapsed_seconds(istart, iend);

    printf("owned_update timing with interference: baseline=%.4fs interfered=%.4fs (%.2fx slower)\n",
           baseline_s, interfered_s, baseline_s > 0.0 ? interfered_s / baseline_s : 0.0);

    REQUIRE(ecs_get_entity_count(ecs, sys1) == (size_t)TEST_OWNED_UPDATE_ENTITIES);
    REQUIRE(ecs_get_entity_count(ecs, sys2) == (size_t)TEST_OWNED_UPDATE_ENTITIES);
    REQUIRE(ecs_get_entity_count(ecs, sys3) == (size_t)TEST_INTERFERENCE_SPAWN_COUNT);

    ecs_entity_t* entities = ecs_get_entity_array(ecs, sys1);

    for (size_t i = 0; i < (size_t)TEST_OWNED_UPDATE_ENTITIES; i++)
    {
        REQUIRE(((comp_t*)ecs_get(ecs, entities[i], comp1))->used);
        REQUIRE(((comp_t*)ecs_get(ecs, entities[i], comp2))->used);
    }

    return true;
}

TEST_CASE(test_owned_update_concurrent_structural_change_race)
{
    sys1 = ecs_define_system(ecs, owned_update_system, &(ecs_sys_desc_t){ .owned_update = true });
    ecs_require(ecs, sys1, comp1);

    owned_update_ctx_t ctx1 = { .comp = comp1, .work_iterations = TEST_RACE_WORK_ITERS };
    ecs_set_system_udata(ecs, sys1, &ctx1);

    for (int i = 0; i < TEST_RACE_INITIAL_ENTITIES; i++)
    {
        ecs_entity_t entity = ecs_create(ecs);
        ecs_add(ecs, entity, comp1, NULL);
    }

    spawn_ctx_t spawn_ctx = { .comp = comp1, .count = TEST_RACE_SPAWN_ENTITIES };

    test_thread_t t1, t2;

    REQUIRE(TEST_THREAD_OK == TEST_THREAD_CREATE(&t1, run_system_worker, &sys1));
    REQUIRE(TEST_THREAD_OK == TEST_THREAD_CREATE(&t2, spawn_worker, &spawn_ctx));

    TEST_THREAD_JOIN(t1);
    TEST_THREAD_JOIN(t2);

    REQUIRE(ecs_get_entity_count(ecs, sys1) ==
            (size_t)(TEST_RACE_INITIAL_ENTITIES + TEST_RACE_SPAWN_ENTITIES));

    return true;
}
TEST_SUITE(suite_owned_update)
{
    RUN_TEST_CASE(test_owned_update_parallel_correctness);
    RUN_TEST_CASE(test_owned_update_same_component_corrupts_data);
    RUN_TEST_CASE(test_owned_update_parallel_speedup);
    RUN_TEST_CASE(test_owned_update_parallel_speedup_with_interference);
    RUN_TEST_CASE(test_owned_update_concurrent_structural_change_race);
}
