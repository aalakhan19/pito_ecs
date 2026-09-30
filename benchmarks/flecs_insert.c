#include "bench_flecs.h"

static void insert_system(ecs_iter_t* it)
{
    bench_ctx_t* c = (bench_ctx_t*)it->ctx;

    for (int i = 0; i < it->count; i++)
    {
        value_t v = { bench_work(1.0, c->work_iterations) };

        ecs_set_id(it->world, it->entities[i], c->comp, sizeof(value_t), &v);
    }
}

static void setup(bench_t* b)
{
    ecs_entity_t base = bench_component();

    for (int i = 0; i < b->system_count; i++)
    {
        ctx[i].comp = bench_component();
        bench_system(insert_system,
                     &ctx[i],
                     true,
                     (ecs_term_t){ .id = base },
                     (ecs_term_t){ .id = ctx[i].comp, .oper = EcsNot });
    }

    ecs_bulk_init(world, &(ecs_bulk_desc_t){ .count = (int32_t)b->entity_count, .ids = { base } });
}

int main(int argc, char** argv)
{
    bench_def_t def = BENCH_FLECS_DEF("owned_insert", false);
    return bench_main(argc, argv, &def);
}
