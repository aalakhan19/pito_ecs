#include "bench_picto.h"

static ecs_ret_t produce_system(ecs_t* ecs,
                                ecs_entity_t* entities,
                                size_t entity_count,
                                void* udata)
{
    (void)entities;
    (void)entity_count;

    bench_ctx_t* ctx = (bench_ctx_t*)udata;

    for (size_t i = 0; i < ctx->entity_count; i++)
    {
        value_t v = { bench_work((double)i, ctx->work_iterations) };

#ifndef BENCH_PICO
        if (ctx->variant == 0)
        {
            ecs_entity_t entity                                    = ecs_create_owned_sharded(ecs);
            *(value_t*)ecs_add_owned(ecs, entity, ctx->comp, NULL) = v;
            ctx->created[i]                                        = entity;
            continue;
        }
#endif
        ecs_entity_t entity = ecs_create(ecs);
        ecs_add(ecs, entity, ctx->comp, NULL);
        *(value_t*)ecs_get(ecs, entity, ctx->comp) = v;
    }

    return 0;
}

static ecs_ret_t consume_system(ecs_t* ecs,
                                ecs_entity_t* entities,
                                size_t entity_count,
                                void* udata)
{
    bench_ctx_t* ctx = (bench_ctx_t*)udata;

    for (size_t i = 0; i < entity_count && i < ctx->entity_count; i++)
    {
        value_t* v = (value_t*)ecs_get(ecs, entities[i], ctx->comp);
        v->value   = bench_work(v->value, ctx->work_iterations);

#ifndef BENCH_PICO
        if (ctx->variant == 0)
        {
            ecs_remove_owned(ecs, entities[i], ctx->comp);
            continue;
        }
#endif
        ecs_remove(ecs, entities[i], ctx->comp);
    }

    return 0;
}

static void setup(bench_t* b)
{
    ecs_comp_t unused = ecs_define_component(ecs, sizeof(value_t), NULL);

    for (int i = 0; i < b->system_count; i++)
    {
        bool producer = i % 2 == 0;

        if (producer)
        {
            ctx[i].comp = ecs_define_component(ecs, sizeof(value_t), NULL);
        }
        else
        {
            ctx[i].comp         = ctx[i - 1].comp;
            ctx[i].entity_count = b->entity_count * b->param / 100;
        }

        if (producer && !ctx[i].created)
            ctx[i].created = malloc(b->entity_count * sizeof(ecs_entity_t));

        ecs_sys_desc_t desc = { .udata = &ctx[i] };
#ifndef BENCH_PICO
        desc.owned_initialize = producer && ctx[i].variant == 0;
        desc.owned_delete     = !producer && ctx[i].variant == 0;
#endif
        systems[i] = ecs_define_system(ecs, producer ? produce_system : consume_system, &desc);
        ecs_require(ecs, systems[i], producer ? unused : ctx[i].comp);

        for (size_t e = 0; !producer && e < ctx[i].entity_count; e++)
            ecs_add(ecs, ecs_create(ecs), ctx[i].comp, NULL);
    }
}

static void finish(bench_t* b)
{
#ifndef BENCH_PICO
    if (b->variant != 0)
        return;

    for (int i = 0; i < b->system_count; i += 2)
        ecs_sync_owned(ecs, ctx[i].created, b->entity_count);

    ecs_sync_owned_delete(ecs);
#else
    (void)b;
#endif
}

int main(int argc, char** argv)
{
    bench_def_t def = BENCH_PICTO_DEF("owned_buffer", "owned", "locked");
    BENCH_PARAMS(def, "fill_pct", 25, 50, 100);
    return bench_main(argc, argv, &def);
}
