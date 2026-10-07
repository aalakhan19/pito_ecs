#include "bench_flecs.h"

static void create_system(ecs_iter_t* it)
{
    bench_ctx_t* c = (bench_ctx_t*)it->ctx;

    for (size_t i = 0; i < c->entity_count; i++)
    {
        value_t v           = { bench_work(1.0, c->work_iterations) };
        ecs_entity_t entity = ecs_new(it->world);

        ecs_set_id(it->world, entity, c->comp, sizeof(value_t), &v);
    }
}

static void read_system(ecs_iter_t* it)
{
    value_t* v = ecs_field(it, value_t, 0);

    for (int i = 0; i < it->count; i++)
        v[i].value += 1.0;
}

static void setup(bench_t* b)
{
    ecs_entity_t comp = bench_component();

    for (int i = 0; i < b->system_count; i++)
    {
        ctx[i].comp = comp;

        bench_system(create_system,
                     &ctx[i],
                     false,
                     (ecs_term_t){ .id = comp, .src.id = EcsIsEntity, .inout = EcsOut },
                     (ecs_term_t){ 0 });
    }

    bench_system(read_system, &ctx[0], true, (ecs_term_t){ .id = comp }, (ecs_term_t){ 0 });
}

int main(int argc, char** argv)
{
    bench_def_t def = BENCH_FLECS_DEF("owned_local", true);
    return bench_main(argc, argv, &def);
}
