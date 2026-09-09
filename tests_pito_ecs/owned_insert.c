#include "threads.h"

#define TEST_OWNED_INSERT_COUNT    4000
#define TEST_OWNED_INSERT_CAPACITY 16384

typedef struct
{
    ecs_comp_t comp;
    size_t     work_iterations;
} owned_insert_ctx_t;

static ecs_ret_t owned_insert_system(ecs_t* ecs,
                                     ecs_entity_t* entities,
                                     size_t entity_count,
                                     void* udata)
{
    owned_insert_ctx_t* ctx = (owned_insert_ctx_t*)udata;

    for (size_t i = 0; i < entity_count; i++)
    {
        volatile double acc = 0.0;
        for (size_t j = 0; j < ctx->work_iterations; j++)
            acc += (double)j * 1.0000001;
        (void)acc;

        ecs_insert_owned(ecs, entities[i], ctx->comp, NULL);
    }

    return 0;
}

TEST_CASE(test_owned_insert_parallel_correctness)
{
    ecs_free(ecs);
    ecs = ecs_new(TEST_OWNED_INSERT_CAPACITY, NULL);

    comp1 = ecs_define_component(ecs, sizeof(comp_t), NULL);
    comp2 = ecs_define_component(ecs, sizeof(comp_t), NULL);

    static ecs_entity_t entities[TEST_OWNED_INSERT_COUNT];

    owned_insert_ctx_t ctx = { .comp = comp2, .work_iterations = 0 };

    sys1 = ecs_define_system(ecs, owned_insert_system,
                             &(ecs_sys_desc_t){ .owned_insert = true, .udata = &ctx });
    ecs_require(ecs, sys1, comp1);
    ecs_exclude(ecs, sys1, comp2);

    for (int i = 0; i < TEST_OWNED_INSERT_COUNT; i++)
    {
        entities[i] = ecs_create(ecs);
        ecs_add(ecs, entities[i], comp1, NULL);
    }

    REQUIRE(TEST_OWNED_INSERT_COUNT == (int)ecs_get_entity_count(ecs, sys1));

    ecs_run_system(ecs, sys1, 0);

    for (int i = 0; i < TEST_OWNED_INSERT_COUNT; i++)
        REQUIRE(ecs_has(ecs, entities[i], comp2));

#if !PITO_ECS_OWNED_DELETE_AUTO_FLUSH
    ecs_sync_owned_delete(ecs);
#endif

    REQUIRE(0 == (int)ecs_get_entity_count(ecs, sys1));

    ecs_run_system(ecs, sys1, 0);

#if !PITO_ECS_OWNED_DELETE_AUTO_FLUSH
    ecs_sync_owned_delete(ecs);
#endif

    REQUIRE(0 == (int)ecs_get_entity_count(ecs, sys1));

    return true;
}

TEST_CASE(test_owned_insert_join_is_deferred)
{
    ecs_free(ecs);
    ecs = ecs_new(TEST_OWNED_INSERT_CAPACITY, NULL);

    comp1 = ecs_define_component(ecs, sizeof(comp_t), NULL);
    comp2 = ecs_define_component(ecs, sizeof(comp_t), NULL);

    static ecs_entity_t entities[TEST_OWNED_INSERT_COUNT];

    owned_insert_ctx_t ctx = { .comp = comp2, .work_iterations = 0 };

    sys1 = ecs_define_system(ecs, owned_insert_system,
                             &(ecs_sys_desc_t){ .owned_insert = true, .udata = &ctx });
    ecs_require(ecs, sys1, comp1);
    ecs_exclude(ecs, sys1, comp2);

    sys2 = ecs_define_system(ecs, noop_system, NULL);
    ecs_require(ecs, sys2, comp1);
    ecs_require(ecs, sys2, comp2);

    for (int i = 0; i < TEST_OWNED_INSERT_COUNT; i++)
    {
        entities[i] = ecs_create(ecs);
        ecs_add(ecs, entities[i], comp1, NULL);

        if (0 == i % 2)
            ecs_add(ecs, entities[i], comp2, NULL);
    }

    REQUIRE(TEST_OWNED_INSERT_COUNT / 2 == (int)ecs_get_entity_count(ecs, sys2));

    ecs_run_system(ecs, sys1, 0);

    REQUIRE(ecs_has(ecs, entities[1], comp2));

#if PITO_ECS_OWNED_DELETE_AUTO_FLUSH
    REQUIRE(TEST_OWNED_INSERT_COUNT == (int)ecs_get_entity_count(ecs, sys2));
#else
    REQUIRE(TEST_OWNED_INSERT_COUNT / 2 == (int)ecs_get_entity_count(ecs, sys2));
#endif

    ecs_sync_owned_delete(ecs);

    REQUIRE(0 == (int)ecs_get_entity_count(ecs, sys1));
    REQUIRE(TEST_OWNED_INSERT_COUNT == (int)ecs_get_entity_count(ecs, sys2));

    return true;
}

TEST_CASE(test_owned_insert_parallel_disjoint_components)
{
    ecs_free(ecs);
    ecs = ecs_new(TEST_OWNED_INSERT_CAPACITY, NULL);

    comp1 = ecs_define_component(ecs, sizeof(comp_t), NULL);
    comp2 = ecs_define_component(ecs, sizeof(comp_t), NULL);
    comp3 = ecs_define_component(ecs, sizeof(comp_t), NULL);

    static ecs_entity_t entities[TEST_OWNED_INSERT_COUNT];

    owned_insert_ctx_t ctx1 = { .comp = comp2, .work_iterations = 0 };
    owned_insert_ctx_t ctx2 = { .comp = comp3, .work_iterations = 0 };

    sys1 = ecs_define_system(ecs, owned_insert_system,
                             &(ecs_sys_desc_t){ .owned_insert = true, .udata = &ctx1 });
    ecs_require(ecs, sys1, comp1);
    ecs_exclude(ecs, sys1, comp2);

    sys2 = ecs_define_system(ecs, owned_insert_system,
                             &(ecs_sys_desc_t){ .owned_insert = true, .udata = &ctx2 });
    ecs_require(ecs, sys2, comp1);
    ecs_exclude(ecs, sys2, comp3);

    for (int i = 0; i < TEST_OWNED_INSERT_COUNT; i++)
    {
        entities[i] = ecs_create(ecs);
        ecs_add(ecs, entities[i], comp1, NULL);
    }

    test_thread_t t1, t2;

    REQUIRE(TEST_THREAD_OK == TEST_THREAD_CREATE(&t1, run_system_worker, &sys1));
    REQUIRE(TEST_THREAD_OK == TEST_THREAD_CREATE(&t2, run_system_worker, &sys2));

    TEST_THREAD_JOIN(t1);
    TEST_THREAD_JOIN(t2);

    ecs_sync_owned_delete(ecs);

    REQUIRE(0 == ecs_get_entity_count(ecs, sys1));
    REQUIRE(0 == ecs_get_entity_count(ecs, sys2));

    for (int i = 0; i < TEST_OWNED_INSERT_COUNT; i++)
    {
        REQUIRE(ecs_has(ecs, entities[i], comp2));
        REQUIRE(ecs_has(ecs, entities[i], comp3));
    }

    return true;
}

TEST_SUITE(suite_owned_insert)
{
    RUN_TEST_CASE(test_owned_insert_parallel_correctness);
    RUN_TEST_CASE(test_owned_insert_join_is_deferred);
    RUN_TEST_CASE(test_owned_insert_parallel_disjoint_components);
}
