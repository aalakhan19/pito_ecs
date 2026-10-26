#include "bench_picto.h"

static ecs_comp_t balance;

static ecs_ret_t bank_system(ecs_t* ecs, ecs_entity_t* entities, size_t entity_count, void* udata)
{
    bench_ctx_t* c = (bench_ctx_t*)udata;
    uint64_t seed  = (uint64_t)(c - ctx);
    uint64_t state = bench_mix64(&seed);

    for (size_t i = 0; i < c->entity_count; i++)
    {
        size_t src_index = bench_next_random(&state) % entity_count;
        size_t dst_index = bench_next_random(&state) % entity_count;
        value_t* src     = (value_t*)ecs_get(ecs, entities[src_index], balance);
        value_t* dst     = (value_t*)ecs_get(ecs, entities[dst_index], balance);

        double amount = bench_work((double)(bench_next_random(&state) % 256), c->work_iterations);
        src->value -= amount;
        dst->value += amount;
    }

    return 0;
}

static void setup(bench_t* b)
{
    balance = ecs_define_component(ecs, sizeof(value_t), NULL);

    for (int i = 0; i < b->system_count; i++)
    {
        ctx[i].comp = ecs_define_component(ecs, sizeof(value_t), NULL);

        ecs_sys_desc_t desc = { .udata = &ctx[i] };
#ifndef BENCH_PICO
        desc.owned_update = ctx[i].variant == 2;
#endif
        systems[i] = ecs_define_system(ecs, bank_system, &desc);
        ecs_require(ecs, systems[i], balance);

        if (ctx[i].variant != 0)
            ecs_require(ecs, systems[i], ctx[i].comp);
    }

    for (int i = 0; i < b->system_count; i++)
    {
        for (size_t e = 0; e < bank_accounts(b); e++)
        {
            ecs_entity_t entity = ecs_create(ecs);
            ecs_add(ecs, entity, balance, NULL);
            ecs_add(ecs, entity, ctx[i].comp, NULL);
        }
    }
}

static void finish(bench_t* b)
{
    (void)b;
}

int main(int argc, char** argv)
{
    bench_def_t def = BENCH_PICTO_DEF(
        "owned_bank", "locked, any account", "locked, own partition", "owned, own partition");
    BENCH_PARAMS(def, "accounts_pct", 1, 10, 100);
    return bench_main(argc, argv, &def);
}
