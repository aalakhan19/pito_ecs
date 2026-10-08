#include "bench_flecs.h"

static void shared_system(ecs_iter_t* it)
{
    bench_ctx_t* c   = (bench_ctx_t*)it->ctx;
    const value_t* s = ecs_field(it, value_t, 0);
    value_t* v       = ecs_field(it, value_t, 1);

    for (int i = 0; i < it->count; i++)
        v[i].value = bench_work(v[i].value + s[i].value, c->work_iterations);
}

static void setup(bench_t* b)
{
    ecs_entity_t shared  = bench_component();
    ecs_bulk_desc_t desc = { .count = (int32_t)b->entity_count };

    for (int i = 0; i < b->system_count; i++)
    {
        ctx[i].comp = bench_component();
        bench_system(shared_system,
                     &ctx[i],
                     true,
                     (ecs_term_t){ .id = shared, .inout = EcsIn },
                     (ecs_term_t){ .id = ctx[i].comp });
        desc.ids[i] = ctx[i].comp;
    }

    const ecs_entity_t* entities = ecs_bulk_init(world, &desc);

    for (size_t e = 0; e < b->entity_count; e++)
        ecs_add_id(world, entities[e], shared);
}

int main(int argc, char** argv)
{
    bench_def_t def = BENCH_FLECS_DEF("owned_shared", false);
    return bench_main(argc, argv, &def);
}
