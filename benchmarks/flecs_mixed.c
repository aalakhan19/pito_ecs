#include "bench_flecs.h"

static void write_system(ecs_iter_t* it)
{
    bench_ctx_t* c = (bench_ctx_t*)it->ctx;

    for (size_t i = 0; i < c->entity_count; i++)
    {
        value_t v           = { bench_work(1.0, c->work_iterations) };
        ecs_entity_t entity = ecs_new(it->world);

        ecs_set_id(it->world, entity, c->comp, sizeof(value_t), &v);
    }
}

static void update_system(ecs_iter_t* it)
{
    bench_ctx_t* c = (bench_ctx_t*)it->ctx;
    value_t* v     = ecs_field(it, value_t, 0);

    for (int i = 0; i < it->count; i++)
        v[i].value = bench_work(v[i].value + 1.0, c->work_iterations);
}

static void setup(bench_t* b)
{
    ecs_entity_t systems[MAX_SYSTEMS];
    ecs_bulk_desc_t desc = { .count = (int32_t)b->entity_count };

    ctx[0].comp = bench_component();
    systems[0] =
        bench_system(write_system,
                     &ctx[0],
                     false,
                     (ecs_term_t){ .id = ctx[0].comp, .src.id = EcsIsEntity, .inout = EcsOut },
                     (ecs_term_t){ 0 });

    for (int i = 1; i < b->system_count; i++)
    {
        ctx[i].comp = bench_component();
        systems[i]  = bench_system(
            update_system, &ctx[i], true, (ecs_term_t){ .id = ctx[i].comp }, (ecs_term_t){ 0 });
        desc.ids[i - 1] = ctx[i].comp;
    }

    ecs_bulk_init(world, &desc);

    if (b->variant == 1)
        ecs_enable(world, systems[0], false);

    for (int i = 1; b->variant == 2 && i < b->system_count; i++)
        ecs_enable(world, systems[i], false);
}

int main(int argc, char** argv)
{
    bench_def_t def = { "owned_mixed", "flecs",        { "mixed", "updaters only", "writer only" },
                        false,         flecs_run_once, flecs_cleanup };
    return bench_main(argc, argv, &def);
}
