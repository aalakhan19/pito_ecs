#include "threads.h"


#define TEST_THREAD_COUNT     4
#define TEST_ENTITIES_PER_RUN 1000

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

TEST_SUITE(suite_threads)
{
    RUN_TEST_CASE(test_concurrent_create_and_add);
    RUN_TEST_CASE(test_concurrent_run_system);
}
