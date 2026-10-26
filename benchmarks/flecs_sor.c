#include "bench_flecs.h"

typedef struct
{
    double value;
    size_t index;
} cell_t;

static ecs_entity_t cur;
static ecs_entity_t* grid;
static size_t grid_size;
static size_t grid_width;

static double cur_at(ecs_world_t* world, size_t index)
{
    return ((const value_t*)ecs_get_id(world, grid[index], cur))->value;
}

static void sor_system(ecs_iter_t* it)
{
    bench_ctx_t* c = (bench_ctx_t*)it->ctx;
    cell_t* cells  = ecs_field(it, cell_t, 0);

    for (int i = 0; i < it->count; i++)
    {
        size_t index = cells[i].index;
        size_t col   = index % grid_width;

        double sum = cur_at(it->world, col > 0 ? index - 1 : index) +
                     cur_at(it->world, col < grid_width - 1 ? index + 1 : index) +
                     cur_at(it->world, index >= grid_width ? index - grid_width : index) +
                     cur_at(it->world, index + grid_width < grid_size ? index + grid_width : index);

        cells[i].value = bench_work(0.25 * sum, c->work_iterations);
    }
}

static void setup(bench_t* b)
{
    grid_width  = b->param < b->entity_count ? b->param : b->entity_count;
    size_t rows = b->entity_count / grid_width;
    size_t band = rows * grid_width;

    grid_size = band * b->system_count;
    grid      = realloc(grid, grid_size * sizeof(ecs_entity_t));
    cur       = bench_component();

    for (int i = 0; i < b->system_count; i++)
    {
        ctx[i].comp =
            ecs_component_init(world,
                               &(ecs_component_desc_t){ .type.size      = sizeof(cell_t),
                                                        .type.alignment = _Alignof(cell_t) });
        bench_system(sor_system,
                     &ctx[i],
                     true,
                     (ecs_term_t){ .id = ctx[i].comp },
                     (ecs_term_t){ .id = cur, .inout = EcsIn });

        const ecs_entity_t* created = ecs_bulk_init(
            world, &(ecs_bulk_desc_t){ .count = (int32_t)band, .ids = { cur, ctx[i].comp } });
        memcpy(grid + i * band, created, band * sizeof(ecs_entity_t));
    }

    for (size_t index = 0; index < grid_size; index++)
    {
        int system = (int)(index / band);
        ecs_set_id(world, grid[index], cur, sizeof(value_t), &(value_t){ (double)(index % 7) });
        ecs_set_id(world, grid[index], ctx[system].comp, sizeof(cell_t), &(cell_t){ 0, index });
    }
}

int main(int argc, char** argv)
{
    bench_def_t def = BENCH_FLECS_DEF("owned_sor", false);
    BENCH_PARAMS(def, "width", 100, 1000, 10000);
    return bench_main(argc, argv, &def);
}
