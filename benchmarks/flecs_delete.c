#include "bench_flecs.h"

static void delete_system(ecs_iter_t* it)
{
    bench_ctx_t* c = (bench_ctx_t*)it->ctx;
    value_t* v     = ecs_field(it, value_t, 0);

    for (int i = 0; i < it->count; i++)
    {
        v[i].value = bench_work(v[i].value + 1.0, c->work_iterations);

        ecs_remove_id(it->world, it->entities[i], c->comp);
    }
}

static void setup(bench_t* b)
{
    ecs_bulk_desc_t desc = { .count = (int32_t)b->entity_count };

    for (int i = 0; i < b->system_count; i++)
    {
        ctx[i].comp = bench_component();
        bench_system(
            delete_system, &ctx[i], true, (ecs_term_t){ .id = ctx[i].comp }, (ecs_term_t){ 0 });
        desc.ids[i] = ctx[i].comp;
    }

    ecs_bulk_init(world, &desc);
}

int main(int argc, char** argv)
{
    bench_def_t def = BENCH_FLECS_DEF("owned_delete", false);
    return bench_main(argc, argv, &def);
}
