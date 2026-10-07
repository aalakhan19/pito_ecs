#include "bench_picto.h"

static ecs_ret_t insert_system(ecs_t* ecs, ecs_entity_t* entities, size_t entity_count, void* udata)
{
    bench_ctx_t* ctx = (bench_ctx_t*)udata;

    for (size_t i = 0; i < entity_count; i++)
    {
        ecs_entity_t entity = entities[i];
        value_t v           = { bench_work(1.0, ctx->work_iterations) };

#ifdef BENCH_PICO
        ecs_add(ecs, entity, ctx->comp, NULL);
#else
        ecs_insert_owned(ecs, entity, ctx->comp, NULL);
#endif

        *(value_t*)ecs_get(ecs, entity, ctx->comp) = v;
    }

    return 0;
}

static void setup(bench_t* b)
{
    ecs_comp_t base = ecs_define_component(ecs, sizeof(value_t), NULL);

    for (int i = 0; i < b->system_count; i++)
    {
        ctx[i].comp = ecs_define_component(ecs, sizeof(value_t), NULL);

        ecs_sys_desc_t desc = { .udata = &ctx[i] };
#ifndef BENCH_PICO
        desc.owned_insert = true;
#endif
        systems[i] = ecs_define_system(ecs, insert_system, &desc);
        ecs_require(ecs, systems[i], base);
        ecs_exclude(ecs, systems[i], ctx[i].comp);

        // never run but joined by every insert
        if (b->variant == 1)
        {
            ecs_system_t follower = ecs_define_system(ecs, insert_system, &(ecs_sys_desc_t){ 0 });
            ecs_require(ecs, follower, ctx[i].comp);
        }
    }

    for (size_t e = 0; e < b->entity_count; e++)
    {
        ecs_entity_t entity = ecs_create(ecs);
        ecs_add(ecs, entity, base, NULL);
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
    bench_def_t def = BENCH_PICTO_DEF("owned_insert", "ecs_insert_owned", "ecs_insert_owned + join");
    return bench_main(argc, argv, &def);
}
