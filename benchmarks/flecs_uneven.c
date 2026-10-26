#include "bench_flecs.h"

static void update_system(ecs_iter_t* it)
{
    bench_ctx_t* c = (bench_ctx_t*)it->ctx;
    value_t* v     = ecs_field(it, value_t, 0);

    for (int i = 0; i < it->count; i++)
        v[i].value = bench_work(v[i].value + 1.0, c->work_iterations);
}

static void setup(bench_t* b)
{
    for (int i = 0; i < b->system_count; i++)
    {
        ctx[i].comp = bench_component();
        bench_system(
            update_system, &ctx[i], true, (ecs_term_t){ .id = ctx[i].comp }, (ecs_term_t){ 0 });

        size_t count = uneven_count(b, i);
        if (count > 0)
            ecs_bulk_init(world,
                          &(ecs_bulk_desc_t){ .count = (int32_t)count, .ids = { ctx[i].comp } });
    }
}

int main(int argc, char** argv)
{
    bench_def_t def = BENCH_FLECS_DEF("owned_uneven", false);
    BENCH_PARAMS(def, "system0_pct", 0, 25, 50, 75, 100);
    return bench_main(argc, argv, &def);
}
