#include "bench_picto.h"

static ecs_ret_t initialize_system(ecs_t* ecs,
                                   ecs_entity_t* entities,
                                   size_t entity_count,
                                   void* udata)
{
    (void)entities;
    (void)entity_count;

    bench_ctx_t* ctx = (bench_ctx_t*)udata;

    for (size_t i = 0; i < ctx->entity_count; i++)
    {
        value_t v = { bench_work(1.0, ctx->work_iterations) };

#ifdef BENCH_PICO
        ecs_entity_t entity = ecs_create(ecs);
        ecs_add(ecs, entity, ctx->comp, NULL);
#else
        ecs_entity_t entity =
            ctx->variant == 1 ? ecs_create_owned_sharded(ecs) : ecs_create_owned(ecs);
        ecs_add_owned(ecs, entity, ctx->comp, NULL);
        ctx->created[i] = entity;
#endif

        *(value_t*)ecs_get(ecs, entity, ctx->comp) = v;
    }

    return 0;
}

static void setup(bench_t* b)
{
    ecs_comp_t unused = ecs_define_component(ecs, sizeof(value_t), NULL);

    for (int i = 0; i < b->system_count; i++)
    {
        ctx[i].comp = ecs_define_component(ecs, sizeof(value_t), NULL);

        if (!ctx[i].created)
            ctx[i].created = malloc(b->entity_count * sizeof(ecs_entity_t));

        ecs_sys_desc_t desc = { .udata = &ctx[i] };
#ifndef BENCH_PICO
        desc.owned_initialize = true;
#endif
        systems[i] = ecs_define_system(ecs, initialize_system, &desc);
        ecs_require(ecs, systems[i], unused);
    }
}

static void finish(bench_t* b)
{
#ifndef BENCH_PICO
    for (int i = 0; i < b->system_count; i++)
        ecs_sync_owned(ecs, ctx[i].created, b->entity_count);
#else
    (void)b;
#endif
}

int main(int argc, char** argv)
{
    bench_def_t def =
        BENCH_PICTO_DEF("owned_initialize", "ecs_create_owned", "ecs_create_owned_sharded");
    return bench_main(argc, argv, &def);
}
