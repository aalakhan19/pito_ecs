#include "bench_picto.h"

static ecs_comp_t shared;

static ecs_ret_t shared_system(ecs_t* ecs, ecs_entity_t* entities, size_t entity_count, void* udata)
{
    bench_ctx_t* ctx = (bench_ctx_t*)udata;

    for (size_t i = 0; i < entity_count; i++)
    {
#ifdef BENCH_PICO
        const value_t* s = (const value_t*)ecs_get(ecs, entities[i], shared);
        value_t* v       = (value_t*)ecs_get(ecs, entities[i], ctx->comp);
#else
        const value_t* s = ctx->variant == 1
                               ? (const value_t*)ecs_get_raw(&ecs->comp_blocks[shared.id],
                                                             entities[i].id)
                               : (const value_t*)ecs_get(ecs, entities[i], shared);
        value_t* v       = ctx->variant == 0
                               ? (value_t*)ecs_get(ecs, entities[i], ctx->comp)
                               : (value_t*)ecs_get_raw(&ecs->comp_blocks[ctx->comp.id],
                                                       entities[i].id);
#endif

        v->value = bench_work(v->value + s->value, ctx->work_iterations);
    }

    return 0;
}

static void setup(bench_t* b)
{
    for (int i = 0; i < b->system_count; i++)
        ctx[i].comp = ecs_define_component(ecs, sizeof(value_t), NULL);

    shared = ecs_define_component(ecs, sizeof(value_t), NULL);

    for (int i = 0; i < b->system_count; i++)
    {
        ecs_sys_desc_t desc = { .udata = &ctx[i] };
#ifndef BENCH_PICO
        desc.owned_update = true;
#endif
        systems[i] = ecs_define_system(ecs, shared_system, &desc);
        ecs_require(ecs, systems[i], shared);
        ecs_require(ecs, systems[i], ctx[i].comp);
    }

    for (size_t e = 0; e < b->entity_count; e++)
    {
        ecs_entity_t entity = ecs_create(ecs);
        ecs_add(ecs, entity, shared, NULL);

        for (int i = 0; i < b->system_count; i++)
            ecs_add(ecs, entity, ctx[i].comp, NULL);
    }
}

static void finish(bench_t* b)
{
    (void)b;
}

int main(int argc, char** argv)
{
    bench_def_t def =
        BENCH_PICTO_DEF("owned_shared", "ecs_get", "ecs_get_raw", "raw own, ecs_get shared");
    return bench_main(argc, argv, &def);
}
