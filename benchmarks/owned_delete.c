#include "bench_picto.h"

static ecs_ret_t delete_system(ecs_t* ecs, ecs_entity_t* entities, size_t entity_count, void* udata)
{
    bench_ctx_t* ctx = (bench_ctx_t*)udata;

    for (size_t i = 0; i < entity_count; i++)
    {
        value_t* v = (value_t*)ecs_get(ecs, entities[i], ctx->comp);
        v->value   = bench_work(v->value + 1.0, ctx->work_iterations);

#ifdef BENCH_PICO
        ecs_remove(ecs, entities[i], ctx->comp);
#else
        ecs_remove_owned(ecs, entities[i], ctx->comp);
#endif
    }

    return 0;
}

static void setup(bench_t* b)
{
    for (int i = 0; i < b->system_count; i++)
    {
        ctx[i].comp = ecs_define_component(ecs, sizeof(value_t), NULL);

        ecs_sys_desc_t desc = { .udata = &ctx[i] };
#ifndef BENCH_PICO
        desc.owned_delete = true;
#endif
        systems[i] = ecs_define_system(ecs, delete_system, &desc);
        ecs_require(ecs, systems[i], ctx[i].comp);
    }

    for (size_t e = 0; e < b->entity_count; e++)
    {
        ecs_entity_t entity = ecs_create(ecs);

        for (int i = 0; i < b->system_count; i++)
            ecs_add(ecs, entity, ctx[i].comp, NULL);
    }
}

static void finish(bench_t* b)
{
    (void)b;
#ifndef BENCH_PICO
    ecs_sync_owned_delete(ecs);
#endif
}

int main(int argc, char** argv)
{
    bench_def_t def = BENCH_PICTO_DEF("owned_delete", "ecs_remove_owned", NULL);
    return bench_main(argc, argv, &def);
}
