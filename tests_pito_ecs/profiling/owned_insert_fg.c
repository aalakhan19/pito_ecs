#define PITO_ECS_IMPLEMENTATION
#include "../../pito_ecs.h"

#include <pthread.h>

#define ENTITIES 20000000
#define SYSTEMS  4

typedef struct
{
    bool used;
} comp_t;

static ecs_t* ecs;
static ecs_comp_t base_comp;
static ecs_comp_t comps[SYSTEMS];
static ecs_system_t systems[SYSTEMS];

static ecs_ret_t owned_insert_system(ecs_t* e, ecs_entity_t* entities,
                                     size_t entity_count, void* udata)
{
    ecs_comp_t comp = *(ecs_comp_t*)udata;

    for (size_t i = 0; i < entity_count; i++)
        ecs_insert_owned(e, entities[i], comp, NULL);

    return 0;
}

static void* worker(void* arg)
{
    int index = *(int*)arg;
    ecs_run_system(ecs, systems[index], 0);
    return NULL;
}

int main(void)
{
    ecs = ecs_new(ENTITIES, NULL);

    base_comp = ecs_define_component(ecs, sizeof(comp_t), NULL);

    for (int i = 0; i < SYSTEMS; i++)
    {
        comps[i] = ecs_define_component(ecs, sizeof(comp_t), NULL);

        systems[i] = ecs_define_system(ecs, owned_insert_system, &(ecs_sys_desc_t){ .owned_insert = true, .udata = &comps[i] });
        ecs_require(ecs, systems[i], base_comp);
        ecs_exclude(ecs, systems[i], comps[i]);
    }

    for (int i = 0; i < ENTITIES; i++)
    {
        ecs_entity_t entity = ecs_create(ecs);
        ecs_add(ecs, entity, base_comp, NULL);
    }

    pthread_t threads[SYSTEMS];
    int indices[SYSTEMS];

    for (int i = 0; i < SYSTEMS; i++)
    {
        indices[i] = i;
        pthread_create(&threads[i], NULL, worker, &indices[i]);
    }

    for (int i = 0; i < SYSTEMS; i++)
        pthread_join(threads[i], NULL);

    ecs_free(ecs);
    return 0;
}
