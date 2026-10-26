#include "bench_picto.h"

typedef struct
{
    double value;
    size_t index;
} cell_t;

static ecs_comp_t cur;
static ecs_entity_t* grid;
static size_t grid_size;
static size_t grid_width;

static double cur_at(ecs_t* ecs, size_t index, int variant)
{
#ifdef BENCH_PICO
    (void)variant;
    return ((value_t*)ecs_get(ecs, grid[index], cur))->value;
#else
    return variant == 1 ? ((value_t*)ecs_get_raw(&ecs->comp_blocks[cur.id], grid[index].id))->value
                        : ((value_t*)ecs_get(ecs, grid[index], cur))->value;
#endif
}

static ecs_ret_t sor_system(ecs_t* ecs, ecs_entity_t* entities, size_t entity_count, void* udata)
{
    bench_ctx_t* ctx = (bench_ctx_t*)udata;

    for (size_t i = 0; i < entity_count; i++)
    {
        cell_t* cell = (cell_t*)ecs_get(ecs, entities[i], ctx->comp);
        size_t index = cell->index;
        size_t col   = index % grid_width;

        double sum =
            cur_at(ecs, col > 0 ? index - 1 : index, ctx->variant) +
            cur_at(ecs, col < grid_width - 1 ? index + 1 : index, ctx->variant) +
            cur_at(ecs, index >= grid_width ? index - grid_width : index, ctx->variant) +
            cur_at(ecs, index + grid_width < grid_size ? index + grid_width : index, ctx->variant);

        cell->value = bench_work(0.25 * sum, ctx->work_iterations);
    }

    return 0;
}

static void setup(bench_t* b)
{
    grid_width  = b->param < b->entity_count ? b->param : b->entity_count;
    size_t rows = b->entity_count / grid_width;

    grid_size = rows * grid_width * b->system_count;
    grid      = realloc(grid, grid_size * sizeof(ecs_entity_t));
    cur       = ecs_define_component(ecs, sizeof(value_t), NULL);

    for (int i = 0; i < b->system_count; i++)
    {
        ctx[i].comp = ecs_define_component(ecs, sizeof(cell_t), NULL);

        ecs_sys_desc_t desc = { .udata = &ctx[i] };
#ifndef BENCH_PICO
        desc.owned_update = ctx[i].variant != 2;
#endif
        systems[i] = ecs_define_system(ecs, sor_system, &desc);
        ecs_require(ecs, systems[i], ctx[i].comp);

        for (size_t index = i * rows * grid_width; index < (i + 1) * rows * grid_width; index++)
        {
            grid[index] = ecs_create(ecs);
            ecs_add(ecs, grid[index], cur, NULL);
            ecs_add(ecs, grid[index], ctx[i].comp, NULL);

            ((value_t*)ecs_get(ecs, grid[index], cur))->value        = (double)(index % 7);
            ((cell_t*)ecs_get(ecs, grid[index], ctx[i].comp))->index = index;
        }
    }
}

static void finish(bench_t* b)
{
    (void)b;
}

int main(int argc, char** argv)
{
    bench_def_t def = BENCH_PICTO_DEF("owned_sor", "ecs_get", "ecs_get_raw", "locked");
    BENCH_PARAMS(def, "width", 100, 1000, 10000);
    return bench_main(argc, argv, &def);
}
