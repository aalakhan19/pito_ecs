#include "bench_flecs.h"

static void produce_system(ecs_iter_t* it)
{
    bench_ctx_t* c = (bench_ctx_t*)it->ctx;

    for (size_t i = 0; i < c->entity_count; i++)
    {
        value_t v           = { bench_work((double)i, c->work_iterations) };
        ecs_entity_t entity = ecs_new(it->world);

        ecs_set_id(it->world, entity, c->comp, sizeof(value_t), &v);
    }
}

static void consume_system(ecs_iter_t* it)
{
    bench_ctx_t* c = (bench_ctx_t*)it->ctx;
    value_t* v     = ecs_field(it, value_t, 0);

    for (int i = 0; i < it->count; i++)
    {
        v[i].value = bench_work(v[i].value, c->work_iterations);

        ecs_remove_id(it->world, it->entities[i], c->comp);
    }
}

static void setup(bench_t* b)
{
    for (int i = 1; i < b->system_count; i += 2)
    {
        ctx[i].comp = ctx[i - 1].comp = bench_component();
        bench_system(
            consume_system, &ctx[i], true, (ecs_term_t){ .id = ctx[i].comp }, (ecs_term_t){ 0 });
        ecs_bulk_init(world,
                      &(ecs_bulk_desc_t){ .count = (int32_t)(b->entity_count * b->param / 100),
                                          .ids   = { ctx[i].comp } });
    }

    for (int i = 0; i + 1 < b->system_count; i += 2)
        bench_system(produce_system,
                     &ctx[i],
                     false,
                     (ecs_term_t){ .id = ctx[i].comp, .src.id = EcsIsEntity, .inout = EcsOut },
                     (ecs_term_t){ 0 });
}

int main(int argc, char** argv)
{
    bench_def_t def = BENCH_FLECS_DEF("owned_buffer", false);
    BENCH_PARAMS(def, "fill_pct", 25, 50, 100);
    return bench_main(argc, argv, &def);
}
