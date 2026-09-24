#include "threads.h"

#define TEST_MIXED_ENTITIES 2000
#define TEST_MIXED_CAPACITY 32768

#define TEST_MIXED_SPAWN_ROUNDS    200
#define TEST_MIXED_SPAWN_PER_ROUND 20

#define TEST_MIXED_WORK_ITERS 2000

static inline void mixed_busy_work(size_t iterations)
{
    volatile double acc = 0.0;
    for (size_t j = 0; j < iterations; j++)
        acc += (double)j * 1.0000001;
    (void)acc;
}

// owned_insert running with normal system

typedef struct
{
    ecs_comp_t comp;
    size_t work_iterations;
} mixed_insert_ctx_t;

static ecs_ret_t mixed_insert_system(ecs_t* ecs,
                                     ecs_entity_t* entities,
                                     size_t entity_count,
                                     void* udata)
{
    mixed_insert_ctx_t* ctx = (mixed_insert_ctx_t*)udata;

    for (size_t i = 0; i < entity_count; i++)
    {
        mixed_busy_work(ctx->work_iterations);
        ecs_insert_owned(ecs, entities[i], ctx->comp, NULL);
    }

    return 0;
}

typedef struct
{
    ecs_comp_t comp;
    int rounds;
    int per_round;
} mixed_spawn_loop_ctx_t;

static int mixed_locked_spawn_loop_worker(void* arg)
{
    mixed_spawn_loop_ctx_t* ctx = (mixed_spawn_loop_ctx_t*)arg;

    for (int r = 0; r < ctx->rounds; r++)
    {
        for (int i = 0; i < ctx->per_round; i++)
        {
            ecs_entity_t entity = ecs_create(ecs);
            ecs_add(ecs, entity, ctx->comp, NULL);
        }

        // for AUTO_FLUSH
        ecs_get_entity_count(ecs, sys2);
    }

    return 0;
}

TEST_CASE(test_owned_insert_concurrent_with_locked_tier)
{
    ecs_free(ecs);
    ecs = ecs_new(TEST_MIXED_CAPACITY, NULL);

    ecs_comp_t base     = ecs_define_component(ecs, sizeof(comp_t), NULL);
    ecs_comp_t inserted = ecs_define_component(ecs, sizeof(comp_t), NULL);
    ecs_comp_t other    = ecs_define_component(ecs, sizeof(comp_t), NULL);

    static ecs_entity_t entities[TEST_MIXED_ENTITIES];

    mixed_insert_ctx_t ins_ctx = { .comp = inserted, .work_iterations = 0 };

    sys1 = ecs_define_system(
        ecs, mixed_insert_system, &(ecs_sys_desc_t){ .owned_insert = true, .udata = &ins_ctx });
    ecs_require(ecs, sys1, base);
    ecs_exclude(ecs, sys1, inserted);

    ecs_system_t consumer = ecs_define_system(ecs, noop_system, NULL);
    ecs_require(ecs, consumer, inserted);

    sys2 = ecs_define_system(ecs, noop_system, NULL);
    ecs_require(ecs, sys2, other);

    for (int i = 0; i < TEST_MIXED_ENTITIES; i++)
    {
        entities[i] = ecs_create(ecs);
        ecs_add(ecs, entities[i], base, NULL);
    }

    mixed_spawn_loop_ctx_t spawn_ctx = { .comp      = other,
                                         .rounds    = TEST_MIXED_SPAWN_ROUNDS,
                                         .per_round = TEST_MIXED_SPAWN_PER_ROUND };

    test_thread_t t1, t2;

    REQUIRE(TEST_THREAD_OK == TEST_THREAD_CREATE(&t1, run_system_worker, &sys1));
    REQUIRE(TEST_THREAD_OK == TEST_THREAD_CREATE(&t2, mixed_locked_spawn_loop_worker, &spawn_ctx));

    TEST_THREAD_JOIN(t1);
    TEST_THREAD_JOIN(t2);

    ecs_sync_owned_delete(ecs);

    REQUIRE(0 == ecs_get_entity_count(ecs, sys1));
    REQUIRE(ecs_get_entity_count(ecs, consumer) == (size_t)TEST_MIXED_ENTITIES);
    REQUIRE(ecs_get_entity_count(ecs, sys2) ==
            (size_t)(TEST_MIXED_SPAWN_ROUNDS * TEST_MIXED_SPAWN_PER_ROUND));

    for (int i = 0; i < TEST_MIXED_ENTITIES; i++)
        REQUIRE(ecs_has(ecs, entities[i], inserted));

    static bool seen[TEST_MIXED_CAPACITY];
    memset(seen, 0, sizeof(seen));

    ecs_entity_t* consumer_entities = ecs_get_entity_array(ecs, consumer);
    size_t consumer_count           = ecs_get_entity_count(ecs, consumer);

    for (size_t i = 0; i < consumer_count; i++)
    {
        ecs_id_t id = consumer_entities[i].id;
        REQUIRE(!seen[id]);
        seen[id] = true;
    }

    return true;
}

// 2 owned_insert systems mit 1 consumer

typedef struct
{
    ecs_comp_t comp;
} mixed_insert_plain_ctx_t;

static ecs_ret_t mixed_insert_plain_system(ecs_t* ecs,
                                           ecs_entity_t* entities,
                                           size_t entity_count,
                                           void* udata)
{
    mixed_insert_plain_ctx_t* ctx = (mixed_insert_plain_ctx_t*)udata;

    for (size_t i = 0; i < entity_count; i++)
        ecs_insert_owned(ecs, entities[i], ctx->comp, NULL);

    return 0;
}

TEST_CASE(test_owned_insert_disjoint_shared_consumer)
{
    ecs_free(ecs);
    ecs = ecs_new(TEST_MIXED_CAPACITY, NULL);

    ecs_comp_t base  = ecs_define_component(ecs, sizeof(comp_t), NULL);
    ecs_comp_t left  = ecs_define_component(ecs, sizeof(comp_t), NULL);
    ecs_comp_t right = ecs_define_component(ecs, sizeof(comp_t), NULL);

    static ecs_entity_t entities[TEST_MIXED_ENTITIES];

    mixed_insert_plain_ctx_t ctx1 = { .comp = left };
    mixed_insert_plain_ctx_t ctx2 = { .comp = right };

    sys1 = ecs_define_system(
        ecs, mixed_insert_plain_system, &(ecs_sys_desc_t){ .owned_insert = true, .udata = &ctx1 });
    ecs_require(ecs, sys1, base);
    ecs_exclude(ecs, sys1, left);

    sys2 = ecs_define_system(
        ecs, mixed_insert_plain_system, &(ecs_sys_desc_t){ .owned_insert = true, .udata = &ctx2 });
    ecs_require(ecs, sys2, base);
    ecs_exclude(ecs, sys2, right);

    ecs_system_t consumer = ecs_define_system(ecs, noop_system, NULL);
    ecs_require(ecs, consumer, left);
    ecs_require(ecs, consumer, right);

    for (int i = 0; i < TEST_MIXED_ENTITIES; i++)
    {
        entities[i] = ecs_create(ecs);
        ecs_add(ecs, entities[i], base, NULL);
    }

    REQUIRE(ecs_get_entity_count(ecs, sys1) == (size_t)TEST_MIXED_ENTITIES);
    REQUIRE(ecs_get_entity_count(ecs, sys2) == (size_t)TEST_MIXED_ENTITIES);

    test_thread_t t1, t2;

    REQUIRE(TEST_THREAD_OK == TEST_THREAD_CREATE(&t1, run_system_worker, &sys1));
    REQUIRE(TEST_THREAD_OK == TEST_THREAD_CREATE(&t2, run_system_worker, &sys2));

    TEST_THREAD_JOIN(t1);
    TEST_THREAD_JOIN(t2);

    ecs_sync_owned_delete(ecs);

    REQUIRE(0 == ecs_get_entity_count(ecs, sys1));
    REQUIRE(0 == ecs_get_entity_count(ecs, sys2));
    REQUIRE(ecs_get_entity_count(ecs, consumer) == (size_t)TEST_MIXED_ENTITIES);

    static bool seen[TEST_MIXED_CAPACITY];
    memset(seen, 0, sizeof(seen));

    ecs_entity_t* consumer_entities = ecs_get_entity_array(ecs, consumer);
    size_t consumer_count           = ecs_get_entity_count(ecs, consumer);

    REQUIRE(consumer_count == (size_t)TEST_MIXED_ENTITIES);

    for (size_t i = 0; i < consumer_count; i++)
    {
        ecs_id_t id = consumer_entities[i].id;
        REQUIRE(!seen[id]);
        seen[id] = true;
    }

    for (int i = 0; i < TEST_MIXED_ENTITIES; i++)
    {
        REQUIRE(ecs_has(ecs, entities[i], left));
        REQUIRE(ecs_has(ecs, entities[i], right));
    }

    return true;
}

// owned_insert and owned_delete concurrently

typedef struct
{
    ecs_comp_t comp;
} mixed_delete_ctx_t;

static ecs_ret_t mixed_delete_system(ecs_t* ecs,
                                     ecs_entity_t* entities,
                                     size_t entity_count,
                                     void* udata)
{
    mixed_delete_ctx_t* ctx = (mixed_delete_ctx_t*)udata;

    for (size_t i = 0; i < entity_count; i++)
        ecs_remove_owned(ecs, entities[i], ctx->comp);

    return 0;
}

TEST_CASE(test_owned_insert_and_delete_concurrent_disjoint)
{
    ecs_free(ecs);
    ecs = ecs_new(TEST_MIXED_CAPACITY, NULL);

    ecs_comp_t base     = ecs_define_component(ecs, sizeof(comp_t), NULL);
    ecs_comp_t inserted = ecs_define_component(ecs, sizeof(comp_t), NULL);
    ecs_comp_t removed  = ecs_define_component(ecs, sizeof(comp_t), NULL);

    static ecs_entity_t entities[TEST_MIXED_ENTITIES];

    mixed_insert_plain_ctx_t ins_ctx = { .comp = inserted };
    mixed_delete_ctx_t del_ctx       = { .comp = removed };

    sys1 = ecs_define_system(ecs,
                             mixed_insert_plain_system,
                             &(ecs_sys_desc_t){ .owned_insert = true, .udata = &ins_ctx });
    ecs_require(ecs, sys1, base);
    ecs_exclude(ecs, sys1, inserted);

    sys2 = ecs_define_system(
        ecs, mixed_delete_system, &(ecs_sys_desc_t){ .owned_delete = true, .udata = &del_ctx });
    ecs_require(ecs, sys2, removed);

    ecs_system_t untouched = ecs_define_system(ecs, noop_system, NULL);
    ecs_require(ecs, untouched, base);

    ecs_system_t consumer_ins = ecs_define_system(ecs, noop_system, NULL);
    ecs_require(ecs, consumer_ins, inserted);

    for (int i = 0; i < TEST_MIXED_ENTITIES; i++)
    {
        entities[i] = ecs_create(ecs);
        ecs_add(ecs, entities[i], base, NULL);
        ecs_add(ecs, entities[i], removed, NULL);
    }

    REQUIRE(ecs_get_entity_count(ecs, untouched) == (size_t)TEST_MIXED_ENTITIES);
    REQUIRE(ecs_get_entity_count(ecs, sys2) == (size_t)TEST_MIXED_ENTITIES);

    test_thread_t t1, t2;

    REQUIRE(TEST_THREAD_OK == TEST_THREAD_CREATE(&t1, run_system_worker, &sys1));
    REQUIRE(TEST_THREAD_OK == TEST_THREAD_CREATE(&t2, run_system_worker, &sys2));

    TEST_THREAD_JOIN(t1);
    TEST_THREAD_JOIN(t2);

    ecs_sync_owned_delete(ecs);

    REQUIRE(0 == ecs_get_entity_count(ecs, sys1));
    REQUIRE(0 == ecs_get_entity_count(ecs, sys2));
    REQUIRE(ecs_get_entity_count(ecs, consumer_ins) == (size_t)TEST_MIXED_ENTITIES);
    REQUIRE(ecs_get_entity_count(ecs, untouched) == (size_t)TEST_MIXED_ENTITIES);

    for (int i = 0; i < TEST_MIXED_ENTITIES; i++)
    {
        REQUIRE(ecs_has(ecs, entities[i], inserted));
        REQUIRE(!ecs_has(ecs, entities[i], removed));
        REQUIRE(ecs_has(ecs, entities[i], base));
    }

    return true;
}

// owned_update while owned_insert attaches a disjoint component

typedef struct
{
    ecs_comp_t comp;
    size_t work_iterations;
} mixed_update_ctx_t;

static ecs_ret_t mixed_update_system(ecs_t* ecs,
                                     ecs_entity_t* entities,
                                     size_t entity_count,
                                     void* udata)
{
    mixed_update_ctx_t* ctx = (mixed_update_ctx_t*)udata;

    for (size_t i = 0; i < entity_count; i++)
    {
        comp_t* c = (comp_t*)ecs_get(ecs, entities[i], ctx->comp);
        mixed_busy_work(ctx->work_iterations);
        c->used = true;
    }

    return 0;
}

TEST_CASE(test_owned_update_and_insert_concurrent_disjoint)
{
    ecs_free(ecs);
    ecs = ecs_new(TEST_MIXED_CAPACITY, NULL);

    ecs_comp_t updated  = ecs_define_component(ecs, sizeof(comp_t), NULL);
    ecs_comp_t base     = ecs_define_component(ecs, sizeof(comp_t), NULL);
    ecs_comp_t inserted = ecs_define_component(ecs, sizeof(comp_t), NULL);

    static ecs_entity_t entities[TEST_MIXED_ENTITIES];

    mixed_update_ctx_t upd_ctx = { .comp = updated, .work_iterations = TEST_MIXED_WORK_ITERS };
    mixed_insert_plain_ctx_t ins_ctx = { .comp = inserted };

    sys1 = ecs_define_system(
        ecs, mixed_update_system, &(ecs_sys_desc_t){ .owned_update = true, .udata = &upd_ctx });
    ecs_require(ecs, sys1, updated);

    sys2 = ecs_define_system(ecs,
                             mixed_insert_plain_system,
                             &(ecs_sys_desc_t){ .owned_insert = true, .udata = &ins_ctx });
    ecs_require(ecs, sys2, base);
    ecs_exclude(ecs, sys2, inserted);

    for (int i = 0; i < TEST_MIXED_ENTITIES; i++)
    {
        entities[i] = ecs_create(ecs);
        ecs_add(ecs, entities[i], updated, NULL);
        ecs_add(ecs, entities[i], base, NULL);
    }

    REQUIRE(ecs_get_entity_count(ecs, sys1) == (size_t)TEST_MIXED_ENTITIES);
    REQUIRE(ecs_get_entity_count(ecs, sys2) == (size_t)TEST_MIXED_ENTITIES);

    test_thread_t t1, t2;

    REQUIRE(TEST_THREAD_OK == TEST_THREAD_CREATE(&t1, run_system_worker, &sys1));
    REQUIRE(TEST_THREAD_OK == TEST_THREAD_CREATE(&t2, run_system_worker, &sys2));

    TEST_THREAD_JOIN(t1);
    TEST_THREAD_JOIN(t2);

    ecs_sync_owned_delete(ecs);

    REQUIRE(ecs_get_entity_count(ecs, sys1) == (size_t)TEST_MIXED_ENTITIES);
    REQUIRE(0 == ecs_get_entity_count(ecs, sys2));

    for (int i = 0; i < TEST_MIXED_ENTITIES; i++)
    {
        REQUIRE(((comp_t*)ecs_get(ecs, entities[i], updated))->used);
        REQUIRE(ecs_has(ecs, entities[i], inserted));
    }

    return true;
}

// owned_initialize/attach with a normal spawner

typedef struct
{
    int count;
    ecs_comp_t comp;
    ecs_entity_t* out;
} mixed_attach_ctx_t;

static ecs_ret_t mixed_attach_system(ecs_t* ecs,
                                     ecs_entity_t* entities,
                                     size_t entity_count,
                                     void* udata)
{
    (void)entities;
    (void)entity_count;

    mixed_attach_ctx_t* ctx = (mixed_attach_ctx_t*)udata;

    for (int i = 0; i < ctx->count; i++)
    {
        ecs_entity_t entity = ecs_create_owned_sharded(ecs);

        comp_t* c = (comp_t*)ecs_add_owned(ecs, entity, ctx->comp, NULL);
        c->used   = true;

        ctx->out[i] = entity;
    }

    return 0;
}

TEST_CASE(test_owned_initialize_attach_concurrent_with_locked_tier)
{
    ecs_free(ecs);
    ecs = ecs_new(TEST_MIXED_CAPACITY, NULL);

    ecs_comp_t attached = ecs_define_component(ecs, sizeof(comp_t), NULL);
    ecs_comp_t other    = ecs_define_component(ecs, sizeof(comp_t), NULL);

    static ecs_entity_t out[TEST_MIXED_ENTITIES];

    mixed_attach_ctx_t attach_ctx = { .count = TEST_MIXED_ENTITIES, .comp = attached, .out = out };

    sys1 = ecs_define_system(ecs,
                             mixed_attach_system,
                             &(ecs_sys_desc_t){ .owned_initialize = true, .udata = &attach_ctx });

    ecs_system_t consumer = ecs_define_system(ecs, noop_system, NULL);
    ecs_require(ecs, consumer, attached);

    ecs_system_t spawned_consumer = ecs_define_system(ecs, noop_system, NULL);
    ecs_require(ecs, spawned_consumer, other);

    spawn_ctx_t spawn_ctx = { .comp  = other,
                              .count = TEST_MIXED_SPAWN_ROUNDS * TEST_MIXED_SPAWN_PER_ROUND };

    test_thread_t t1, t2;

    REQUIRE(TEST_THREAD_OK == TEST_THREAD_CREATE(&t1, run_system_worker, &sys1));
    REQUIRE(TEST_THREAD_OK == TEST_THREAD_CREATE(&t2, spawn_worker, &spawn_ctx));

    TEST_THREAD_JOIN(t1);
    TEST_THREAD_JOIN(t2);

    REQUIRE(0 == ecs_get_entity_count(ecs, consumer));

    ecs_sync_owned(ecs, out, TEST_MIXED_ENTITIES);

    REQUIRE(ecs_get_entity_count(ecs, consumer) == (size_t)TEST_MIXED_ENTITIES);
    REQUIRE(ecs_get_entity_count(ecs, spawned_consumer) == (size_t)spawn_ctx.count);

    static bool seen[TEST_MIXED_CAPACITY];
    memset(seen, 0, sizeof(seen));

    for (int i = 0; i < TEST_MIXED_ENTITIES; i++)
    {
        ecs_entity_t entity = out[i];

        REQUIRE(ecs_is_ready(ecs, entity));
        REQUIRE(ecs_has(ecs, entity, attached));

        comp_t* c = (comp_t*)ecs_get(ecs, entity, attached);
        REQUIRE(c->used);

        REQUIRE(!seen[entity.id]);
        seen[entity.id] = true;
    }

    return true;
}

#define TEST_GROWTH_RACE_CAPACITY 64
#define TEST_GROWTH_RACE_TARGETS  32
#define TEST_GROWTH_RACE_TOGGLES  20000
#define TEST_GROWTH_RACE_SPAWN    20000

typedef struct
{
    ecs_comp_t comp;
    ecs_entity_t* targets;
    int target_count;
    int toggles;
} growth_race_toggle_ctx_t;

static int growth_race_toggle_worker(void* arg)
{
    growth_race_toggle_ctx_t* ctx = (growth_race_toggle_ctx_t*)arg;

    for (int i = 0; i < ctx->toggles; i++)
    {
        ecs_entity_t entity = ctx->targets[i % ctx->target_count];
        ecs_insert_owned(ecs, entity, ctx->comp, NULL);
        ecs_remove_owned(ecs, entity, ctx->comp);
    }

    return 0;
}

TEST_CASE(test_entities_array_growth_races_owned_comp_bits)
{
    ecs_free(ecs);
    ecs = ecs_new(TEST_GROWTH_RACE_CAPACITY, NULL);

    ecs_comp_t base    = ecs_define_component(ecs, sizeof(comp_t), NULL);
    ecs_comp_t toggled = ecs_define_component(ecs, sizeof(comp_t), NULL);

    static ecs_entity_t targets[TEST_GROWTH_RACE_TARGETS];

    for (int i = 0; i < TEST_GROWTH_RACE_TARGETS; i++)
    {
        targets[i] = ecs_create(ecs);
        ecs_add(ecs, targets[i], base, NULL);
    }

    growth_race_toggle_ctx_t toggle_ctx = { .comp         = toggled,
                                            .targets      = targets,
                                            .target_count = TEST_GROWTH_RACE_TARGETS,
                                            .toggles      = TEST_GROWTH_RACE_TOGGLES };

    spawn_ctx_t spawn_ctx = { .comp = base, .count = TEST_GROWTH_RACE_SPAWN };

    test_thread_t t1, t2;

    REQUIRE(TEST_THREAD_OK == TEST_THREAD_CREATE(&t1, growth_race_toggle_worker, &toggle_ctx));
    REQUIRE(TEST_THREAD_OK == TEST_THREAD_CREATE(&t2, spawn_worker, &spawn_ctx));

    TEST_THREAD_JOIN(t1);
    TEST_THREAD_JOIN(t2);

    for (int i = 0; i < TEST_GROWTH_RACE_TARGETS; i++)
        REQUIRE(!ecs_has(ecs, targets[i], toggled));

    return true;
}

TEST_SUITE(suite_owned_mixed)
{
    RUN_TEST_CASE(test_owned_insert_concurrent_with_locked_tier);
    RUN_TEST_CASE(test_owned_insert_disjoint_shared_consumer);
    RUN_TEST_CASE(test_owned_insert_and_delete_concurrent_disjoint);
    RUN_TEST_CASE(test_owned_update_and_insert_concurrent_disjoint);
    RUN_TEST_CASE(test_owned_initialize_attach_concurrent_with_locked_tier);
    RUN_TEST_CASE(test_entities_array_growth_races_owned_comp_bits);
}
