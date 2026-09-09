#include "common.h"

#define PICO_UNIT_IMPLEMENTATION
#include "../pico_unit.h"

#define PITO_ECS_IMPLEMENTATION
#include "../pito_ecs.h"

ecs_t* ecs = NULL;
ecs_comp_t comp1;
ecs_comp_t comp2;
ecs_comp_t comp3;

ecs_system_t sys1;
ecs_system_t sys2;

void setup(void)
{
    ecs = ecs_new(MIN_ENTITIES, NULL);
    comp1 = ecs_define_component(ecs, sizeof(comp_t), NULL);
    comp2 = ecs_define_component(ecs, sizeof(comp_t), NULL);
    comp3 = ecs_define_component(ecs, sizeof(comp_t), NULL);
}

void teardown(void)
{
    ecs_free(ecs);
    ecs = NULL;
}

TEST_CASE(test_capacity_validation)
{
    // capacity with the high bit set is not valid
    REQUIRE(ecs_is_valid_capacity(SIZE_MAX >> 8, 1 << 7));
    REQUIRE(!ecs_is_valid_capacity((SIZE_MAX >> 8) + 1, 1 << 7));
    REQUIRE(!ecs_is_valid_capacity(SIZE_MAX >> 8, 1 << 8));

    // capacity with the high bit set is not valid
    REQUIRE(ecs_is_valid_capacity((SIZE_MAX >> 1), 1));
    REQUIRE(!ecs_is_valid_capacity((SIZE_MAX >> 1) + 1, 1));

    // zero capacity is invalid
    REQUIRE(!ecs_is_valid_capacity(0, 16));
    REQUIRE(!ecs_is_valid_capacity(16, 0));
    REQUIRE(!ecs_is_valid_capacity(0, 0));

    // normal cases
    REQUIRE(ecs_is_valid_capacity(500, 128));
    REQUIRE(ecs_is_valid_capacity(1000, 8));

    return true;
}

TEST_SUITE(suite_validation)
{
    RUN_TEST_CASE(test_capacity_validation);
}

TEST_SUITE(suite_entity);
TEST_SUITE(suite_components);
TEST_SUITE(suite_systems);
TEST_SUITE(suite_exclude);
TEST_SUITE(suite_deferred);
TEST_SUITE(suite_set);
TEST_SUITE(suite_validation);
TEST_SUITE(suite_threads);
TEST_SUITE(suite_owned_update);
TEST_SUITE(suite_owned_init);
TEST_SUITE(suite_owned_attach);
TEST_SUITE(suite_owned_local);
TEST_SUITE(suite_owned_delete);
TEST_SUITE(suite_owned_insert);

int main (void)
{
    pu_display_colors(true);
    pu_setup(setup, teardown);
    RUN_TEST_SUITE(suite_entity);
    RUN_TEST_SUITE(suite_components);
    RUN_TEST_SUITE(suite_systems);
    RUN_TEST_SUITE(suite_exclude);
    RUN_TEST_SUITE(suite_deferred);
    RUN_TEST_SUITE(suite_set);
    RUN_TEST_SUITE(suite_validation);
    RUN_TEST_SUITE(suite_threads);
    RUN_TEST_SUITE(suite_owned_update);
    RUN_TEST_SUITE(suite_owned_init);
    RUN_TEST_SUITE(suite_owned_attach);
    RUN_TEST_SUITE(suite_owned_local);
    RUN_TEST_SUITE(suite_owned_delete);
    RUN_TEST_SUITE(suite_owned_insert);
    pu_print_stats();
    return pu_test_failed();
}
