#include "bench_flecs.h"

static void initialize_system(ecs_iter_t* it)
{
    bench_ctx_t* c = (bench_ctx_t*)it->ctx;

    for (size_t i = 0; i < c->entity_count; i++)
    {
        value_t v           = { bench_work(1.0, c->work_iterations) };
        ecs_entity_t entity = ecs_new(it->world);

        ecs_set_id(it->world, entity, c->comp, sizeof(value_t), &v);
    }
}

static void setup(bench_t* b)
{
    for (int i = 0; i < b->system_count; i++)
    {
        ctx[i].comp = bench_component();
        bench_system(initialize_system, &ctx[i], false, (ecs_term_t){ 0 }, (ecs_term_t){ 0 });
    }
}

int main(int argc, char** argv)
{
    bench_def_t def = BENCH_FLECS_DEF("owned_initialize", true);
    return bench_main(argc, argv, &def);
}
