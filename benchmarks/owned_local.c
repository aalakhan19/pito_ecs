#include "bench_picto.h"

static ecs_system_t reader;

static ecs_ret_t create_system(ecs_t* ecs, ecs_entity_t* entities, size_t entity_count, void* udata)
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
        *(value_t*)ecs_get(ecs, entity, ctx->comp) = v;
#else
        ecs_entity_t entity;

        if (ctx->variant != 0)
        {
            entity = ecs_create_owned_local(ecs);
        }
        else
        {
            entity          = ecs_create_owned_sharded(ecs);
            ctx->created[i] = entity;
        }

        *(value_t*)ecs_add_owned(ecs, entity, ctx->comp, NULL) = v;
#endif
    }

    return 0;
}

static ecs_ret_t read_system(ecs_t* ecs, ecs_entity_t* entities, size_t entity_count, void* udata)
{
    bench_ctx_t* ctx = (bench_ctx_t*)udata;

    for (size_t i = 0; i < entity_count; i++)
    {
        value_t* v = (value_t*)ecs_get(ecs, entities[i], ctx->comp);
        v->value += 1.0;
    }

    return 0;
}

static void setup(bench_t* b)
{
    ecs_comp_t unused = ecs_define_component(ecs, sizeof(value_t), NULL);

    // one component for all creators: a thread's local store holds one archetype
    ecs_comp_t comp = ecs_define_component(ecs, sizeof(value_t), NULL);

    for (int i = 0; i < b->system_count; i++)
    {
        ctx[i].comp = comp;

        if (!ctx[i].created)
            ctx[i].created = malloc(b->entity_count * sizeof(ecs_entity_t));

        ecs_sys_desc_t desc = { .udata = &ctx[i] };
#ifndef BENCH_PICO
        desc.owned_initialize     = true;
        desc.owned_publish_at_end = true;
#endif
        systems[i] = ecs_define_system(ecs, create_system, &desc);
        ecs_require(ecs, systems[i], unused);
    }

    ecs_sys_desc_t desc = { .udata = &ctx[0] };
#ifndef BENCH_PICO
    desc.owned_update = true;
    desc.reads_owned  = true;
#endif
    reader = ecs_define_system(ecs, read_system, &desc);
    ecs_require(ecs, reader, comp);
}

static void finish(bench_t* b)
{
#ifndef BENCH_PICO
    if (b->variant == 0)
    {
        for (int i = 0; i < b->system_count; i++)
            ecs_sync_owned(ecs, ctx[i].created, b->entity_count);
    }
    else if (b->variant == 2)
    {
        ecs_flush_owned(ecs);
    }
#endif

    ecs_run_system(ecs, reader, 0);
}

int main(int argc, char** argv)
{
    bench_def_t def = BENCH_PICTO_DEF(
        "owned_local", "ecs_sync_owned", "ecs_create_owned_local", "ecs_flush_owned");
    return bench_main(argc, argv, &def);
}
