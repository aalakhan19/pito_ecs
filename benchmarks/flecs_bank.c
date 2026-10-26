#include "bench_flecs.h"

static ecs_entity_t balance;
static ecs_entity_t* accounts;
static size_t account_count;
static size_t accounts_per_system;
static int variant;

static void bank_system(ecs_iter_t* it)
{
    bench_ctx_t* c = (bench_ctx_t*)it->ctx;
    size_t system  = (size_t)(c - ctx);
    uint64_t seed  = system;
    uint64_t state = bench_mix64(&seed);

    size_t count             = variant == 0 ? account_count : accounts_per_system;
    const ecs_entity_t* pool = variant == 0 ? accounts : accounts + system * accounts_per_system;

    for (size_t i = 0; i < c->entity_count; i++)
    {
        size_t src_index = bench_next_random(&state) % count;
        size_t dst_index = bench_next_random(&state) % count;
        value_t* src     = ecs_get_mut_id(it->world, pool[src_index], balance);
        value_t* dst     = ecs_get_mut_id(it->world, pool[dst_index], balance);

        double amount = bench_work((double)(bench_next_random(&state) % 256), c->work_iterations);
        src->value -= amount;
        dst->value += amount;
    }
}

static void setup(bench_t* b)
{
    variant = b->variant;
    balance = bench_component();

    accounts_per_system = bank_accounts(b);
    account_count       = accounts_per_system * b->system_count;
    accounts            = realloc(accounts, account_count * sizeof(ecs_entity_t));

    for (int i = 0; i < b->system_count; i++)
    {
        ctx[i].comp = bench_component();
        bench_system(bank_system, &ctx[i], false, (ecs_term_t){ 0 }, (ecs_term_t){ 0 });

        ecs_bulk_desc_t desc        = { .count = (int32_t)accounts_per_system,
                                        .ids   = { balance, ctx[i].comp } };
        const ecs_entity_t* created = ecs_bulk_init(world, &desc);
        memcpy(accounts + i * accounts_per_system,
               created,
               accounts_per_system * sizeof(ecs_entity_t));
    }

    for (size_t e = 0; e < account_count; e++)
        ecs_set_id(world, accounts[e], balance, sizeof(value_t), &(value_t){ 0 });
}

int main(int argc, char** argv)
{
    bench_def_t def = { "owned_bank", "flecs",        { "any account", "own partition" },
                        true,         flecs_run_once, flecs_cleanup };
    BENCH_PARAMS(def, "accounts_pct", 1, 10, 100);
    return bench_main(argc, argv, &def);
}
