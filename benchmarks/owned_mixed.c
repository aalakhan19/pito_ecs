#include "bench_picto.h"

static ecs_ret_t write_system(ecs_t* ecs, ecs_entity_t* entities, size_t entity_count, void* udata)
{
    (void)entities;
    (void)entity_count;

    bench_ctx_t* ctx = (bench_ctx_t*)udata;

    for (size_t i = 0; i < ctx->entity_count; i++)
    {
        value_t v           = { bench_work(1.0, ctx->work_iterations) };
        ecs_entity_t entity = ecs_create(ecs);
        ecs_set(ecs, entity, ctx->comp, &v);
    }

    return 0;
}

static ecs_ret_t update_system(ecs_t* ecs, ecs_entity_t* entities, size_t entity_count, void* udata)
{
    bench_ctx_t* ctx = (bench_ctx_t*)udata;

    for (size_t i = 0; i < entity_count; i++)
    {
        value_t* v = (value_t*)ecs_get(ecs, entities[i], ctx->comp);
        v->value   = bench_work(v->value + 1.0, ctx->work_iterations);
    }

    return 0;
}

static void setup(bench_t* b)
{
    ecs_comp_t unused = ecs_define_component(ecs, sizeof(value_t), NULL);

    ctx[0].comp = ecs_define_component(ecs, sizeof(value_t), NULL);
    systems[0]  = ecs_define_system(ecs, write_system, &(ecs_sys_desc_t){ .udata = &ctx[0] });
    ecs_require(ecs, systems[0], unused);

    for (int i = 1; i < b->system_count; i++)
    {
        ctx[i].comp = ecs_define_component(ecs, sizeof(value_t), NULL);

        ecs_sys_desc_t desc = { .udata = &ctx[i] };
#ifndef BENCH_PICO
        desc.owned_update = true;
#endif
        systems[i] = ecs_define_system(ecs, update_system, &desc);
        ecs_require(ecs, systems[i], ctx[i].comp);
    }

    for (size_t e = 0; e < b->entity_count; e++)
    {
        ecs_entity_t entity = ecs_create(ecs);

        for (int i = 1; i < b->system_count; i++)
            ecs_add(ecs, entity, ctx[i].comp, NULL);
    }

    if (b->variant == 1)
        ecs_disable_system(ecs, systems[0]);

    for (int i = 1; b->variant == 2 && i < b->system_count; i++)
        ecs_disable_system(ecs, systems[i]);
}

static void finish(bench_t* b)
{
    (void)b;
}

int main(int argc, char** argv)
{
    bench_def_t def = BENCH_PICTO_DEF("owned_mixed", "mixed", "updaters only", "writer only");
    return bench_main(argc, argv, &def);
}
