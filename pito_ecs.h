/**
    @file pito_ecs.h
    @brief A pure and simple ECS

    ----------------------------------------------------------------------------
    Licensing information at end of header
    ----------------------------------------------------------------------------

    Features:
    ---------
    - Written in C11
    - Single header library for easy build system integration
    - Excellent performance
    - Pure ECS design (strict separation between data and logic)
    - Simple and concise API
    - Permissive license (zlib or public domain)

    Summary:
    --------

    This library implements an ECS (Entity-Component-System). Entities
    (sometimes called game objects) are defined by their components. For
    example, an entity might have position, sprite, and physics components.
    Systems operate on the components of entities that match the system's
    requirements. Entities are matched to systems based upon which components
    they have and also the system's matching crieria.

    In the above example, a sprite renderer system would match entities having
    poition and sprite components. The system would send the appropriate
    geometry and texture ID to the game's graphics API.

    Traditional game engines tightly couple state with logic. For example, in
    C++ a game object typically has its own update method that operates
    on that state. They also usually rely heavily on inheritance.

    If the state specified by the class changes this could ripple through the
    class, and perhaps subclasses as well. It could well be that the class no
    longer belongs in the existing class hierarchy forcing even more revisions.
    It could even be true that a class doesn't neatly fit into the inheritance
    tree at all.

    An ECS solves these problems while also granting more flexibility in
    general. In an ECS there is a clear separation between data (components) and
    logic (systems), which makes it possible to build complex simulations with
    fewer assumptions about how that data will be used. In an ECS it is
    effortless to change functionality, by either adding or removing components
    from entities, and/or by changing system requirements. Adding new logic is
    also simple as well, just defining a new system!

    Please see the examples and unit tests for more details.

    Masks:
    ------

    Masks are a new feature in 3.0. Systems to are assigned to categories (using
    a bitmask) at definition and then can selectively invoke those systems
    at runtime (also using a bitmask).

    Note that passing 0 into `ecs_define_system` means the system matches
    all categories.

    If `ecs_system_t sys = ecs_define_system(ecs, (1 << 0) | (1 << 1), ...)` Then,

    This will run `sys`:

    `ecs_run_system(ecs, sys, (1 << 0) | (1 << 1));`

    And so will this,

    `ecs_run_system(ecs, sys, (1 << 1));`

    But this will not,

    `ecs_run_system(ecs, sys, (1 << 3));`

    Nor will this:

    `ecs_run_system(ecs, sys, 0);`

    Revision History:
    -----------------

    - 3.0 (2025/10/22):
        - Typesafe entity, component, and system handles
        - System category masks
        - More descriptive names for some functions in the public API
        - Invalid entity ID is now 0
        - The 'dt' parameter has been removed
        - Optimizations
        - Some function name changes
        - Significant internal refactoring

    - 3.1 (2026/02/01):
        - Fixed sparse set related bugs
        - More sophisticated logic regarding adding entities to/from systems
        - Functions ecs_queue_remove and ecs_queue_destroy have been removed.
          (they can be replaced directly by ecs_remove and ecs_destroy
          respectively)
        - Improved unit test quality and coverage

    - 3.2 (2026/03/09):
        - Capacity overflow detection
        - Invalid entity ID value has been reverted to max ID value

    - 3.3 (2026/06/05):
        - New command queue that ensures add/remove/set/destroy operations are
          performed in the order the corresponding functions were called.
        - Pointers obtained by ecs_get are now stable.
        - The ecs_add constructor/destructor callbacks have been replaced by
          on_add/on_remove callbacks.
        - A new function 'ecs_set' has been added. If the entity does not have
          the component it is added first, then the component's value is set.
          If called during system iteration, then setting the value is deferred
          until after the system completes. This function effectively replaces
          the old ecs_add/constructor functionality.
        - ecs_require_component and ecs_exclude_component have been renamed to
          ecs_require and ecs_exclude.
        - Renamed ecs_get_system_entity_count to ecs_get_entity_count.
        - Added ecs_get_entity_array that return the entities associated with a
          system.

    - 3.4 (2026/06/18):
        - ecs_add now accepts an optional 'args' pointer that is forwarded to the
          on_add callback. When the component is defined with a non-zero
          args_size, the args are copied into the command arena so the caller
          need not keep them alive (relevant for deferred adds during system
          iteration).
        - Components may now specify a default_value that is copied into the
          component on add.

    - 3.5 (2026/07/06):
        - ecs_t and its API are now safe to call concurrently from multiple
          threads

    Usage:
    ------

    To use this library in your project, add the following

    > #define PITO_ECS_IMPLEMENTATION
    > #include "pito_ecs.h"

    to a source file (once), then simply include the header normally.

    Macros:
    --------

    - ECS_MALLOC(size, ctx)       (default: malloc)
    - ECS_REALLOC(ptr, size, ctx) (default: realloc)
    - ECS_FREE(ptr, ctx)          (default: free)
    - ECS_MEMSET                  (default: memset)
    - ECS_MEMCPY                  (default: memcpy)

    The ctx parameter is sometimes used by custom allocators

    Constants:
    --------

    - PITO_ECS_MAX_COMPONENTS        (default: 32)
    - PITO_ECS_MAX_SYSTEMS           (default: 16)
    - PITO_ECS_COMP_BLOCK_SIZE       (default: 64)
    - PITO_ECS_INITIALIZE_SHARD_SIZE (default: 1024)
    - PITO_ECS_MAX_OWNED_LOCALS      (default: 16)

    Must be defined before PITO_ECS_IMPLEMENTATION
*/


#if !defined(_POSIX_C_SOURCE)
    #define _POSIX_C_SOURCE 200809L
#endif

#ifndef PITO_ECS_H
#define PITO_ECS_H

#include <stdbool.h> // bool, true, false
#include <stddef.h>  // size_t
#include <stdint.h>  // uint32_t, uint64_t
#include <limits.h>  // SIZE_MAX

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief ECS context
 */
typedef struct ecs_s ecs_t;

/**
 * @brief Determine ID type. It should be unsigned.
 */
#ifndef ECS_ID_TYPE
#define ECS_ID_TYPE uint64_t
#endif

/**
 * @brief ID used for entity and components
 */
typedef ECS_ID_TYPE ecs_id_t;

/**
 * @brief Determine mask type
 */
#ifndef ECS_MASK_TYPE
#define ECS_MASK_TYPE uint64_t
#endif

/**
 * @brief Type for value used in system matching
 */
typedef ECS_MASK_TYPE ecs_mask_t;

/**
 * @brief Return code for system callback and calling functions
 */
typedef int32_t ecs_ret_t;

/**
 * @brief An entity handle
 */
typedef struct ecs_entity_t { ecs_id_t id; } ecs_entity_t;

/**
 * @brief A component handle
 */
typedef struct ecs_comp_t { ecs_id_t id; } ecs_comp_t;

/**
 * @brief A system handle
 */
typedef struct ecs_system_t { ecs_id_t id; } ecs_system_t;

/**
 * @brief An invalid ID
 */
#define ECS_INVALID_ID ((ecs_id_t)-1)

/**
 * @brief True if the argument (entity/system/component) is invalid
 */
#define ECS_IS_INVALID(obj) ((obj.id) == ECS_INVALID_ID)

/**
 * @brief Creates an ECS context.
 *
 * @param entity_count The inital number of entities to pre-allocated
 * @param mem_ctx A context for a custom allocator
 *
 * @returns An ECS context or NULL if out of memory
 */
ecs_t* ecs_new(size_t entity_capacity, void* mem_ctx);

/**
 * @brief Destroys an ECS context
 *
 * @param ecs The ECS context
 */
void ecs_free(ecs_t* ecs);

/**
 * @brief Removes all entities from the ECS, preserving systems and components.
 */
void ecs_reset(ecs_t* ecs);

/**
 * @brief Called when a component is created (via ecs_add)
 *
 * @param ecs    The ECS context
 * @param entity The entity being constructed
 * @param ptr    The pointer to the component
 */
typedef void (*ecs_on_add_fn)(ecs_t* ecs,
                              ecs_entity_t entity,
                              ecs_comp_t comp,
                              const void* args,
                              void* udata);

/**
 * @brief Called when a component is destroyed (via ecs_remove or ecs_destroy)
 *
 * @param ecs    The ECS context
 * @param entity The entity being destoryed
 * @param ptr    The pointer to the component
 */
typedef void (*ecs_on_remove_fn)(ecs_t* ecs,
                                 ecs_entity_t entity,
                                 ecs_comp_t comp,
                                 void* udata);


/**
 * @brief Called when a component's data is set (via ecs_set)
 *
 * @param ecs    The ECS context
 * @param entity The entity whose component was set
 * @param comp   The component that was set
 * @param udata  The user data passed to the callback
 */
typedef void (*ecs_on_set_fn)(ecs_t* ecs,
                              ecs_entity_t entity,
                              ecs_comp_t comp,
                              void* udata);

/**
 * @brief Optional parameters for component definition
 *
 * @param on_add_cb    Called when a component is added to an entity (can be NULL)
 * @param on_remove_cb Called when a component is removed from an entity (can be NULL)
 * @param on_set_cb    Called when a component's data is set via ecs_set (can be NULL)
 * @param default_value Optional initial component value, copied on add (can be NULL)
 * @param args_size    Size, in bytes, of the args buffer passed to ecs_add. When
 *                     non-zero and an add is deferred (issued from a running
 *                     system), this many bytes are copied and stored so the
 *                     caller need not keep the args alive. Leave 0 if the
 *                     component takes no args.
 * @param udata        User data passed to callbacks (can be NULL)
 */
typedef struct
{
    ecs_on_add_fn on_add_cb;
    ecs_on_remove_fn on_remove_cb;
    ecs_on_set_fn on_set_cb;
    void* default_value;
    size_t args_size;
    void* udata;
} ecs_comp_desc_t;

/**
 * @brief Defines a component
 *
 * Defines a component with the specified size in bytes. Components define the
 * game state (usually contained within structs) and are manipulated by systems.
 *
 * @param ecs  The ECS context
 * @param size The number of bytes to allocate for each component instance
 * @param def   Optional parameters for callbacks and user data (can be NULL)
 * @returns     A component handle
 */
ecs_comp_t ecs_define_component(ecs_t* ecs,
                                size_t size,
                                const ecs_comp_desc_t* desc);

/**
 * @brief System callback
 *
 * Systems implement the core logic of an ECS by manipulating entities
 * and components.
 *
 * @param ecs          The ECS context
 * @param entities     An array of entities managed by the system
 * @param entity_count The number of entities in the array
 * @param udata        The user data associated with the system
 */
typedef ecs_ret_t (*ecs_system_fn)(ecs_t* ecs,
                                   ecs_entity_t* entities,
                                   size_t entity_count,
                                   void* udata);

/**
 * @brief Called when an entity is added to a system
 *
 * @param ecs    The ECS context
 * @param entity The entity being added
 * @param udata  The user data passed to the callback
 */
typedef void (*ecs_on_join_fn)(ecs_t* ecs, ecs_entity_t entity, void* udata);

/**
 * @brief Called when an entity is removed from a system
 *
 * @param ecs    The ECS context
 * @param entity The enitty being removed
 * @param udata  The user data passed to the callback
 */
typedef void (*ecs_on_leave_fn)(ecs_t* ecs, ecs_entity_t entity, void* udata);

/**
 * @brief Optional parameters for system definition
 *
 * @param mask        Bitmask that assigns the system to one or more categories.
 *                    A value of 0 means the system matches all categories.
 * @param on_join_cb  Called when an entity is added to the system (can be NULL)
 * @param on_leave_cb Called when an entity is removed from the system (can be NULL)
 * @param udata       User data passed to callbacks (can be NULL)
 * @param owned_update
 * @param owned_initialize
 * @param reads_owned Reads thread local entities in seprate thread. Each thread
 * creates an addition run of the system //TODO: Sideeffects?
 * @param owned_publish_at_end true if all thread local entities should be visibale after the system is run, instead of every entity on creation
 */
typedef struct
{
    ecs_mask_t mask;
    ecs_on_join_fn on_join_cb;
    ecs_on_leave_fn on_leave_cb;
    void* udata;
    bool owned_update;
    bool owned_initialize;
    bool reads_owned;
    bool owned_publish_at_end;
} ecs_sys_desc_t;

/**
 * @brief Defines a system
 *
 * Defines a system with the specified parameters. Systems contain the
 * core logic of a game by manipulating game state as defined by components.
 *
 * @param ecs       The ECS context
 * @param system_cb Callback that is fired every update
 * @param def       Optional parameters for mask, join/leave callbacks, and user data (can be NULL)
 * @returns         A system handle
 */
ecs_system_t ecs_define_system(ecs_t* ecs,
                               ecs_system_fn system_cb,
                               const ecs_sys_desc_t* desc);


/**
 * @brief Entities are processed by the target system if they have all of the
 * the components required by the system
 *
 * @param ecs  The ECS context
 * @param sys  The target system
 * @param comp A component to require
 */
void ecs_require(ecs_t* ecs, ecs_system_t sys, ecs_comp_t comp);

/**
 * @brief Excludes entities having the specified component from being added to
 * the target system.
 *
 * @param ecs  The ECS context
 * @param sys  The target system
 * @param comp A component to exclude
 */
void ecs_exclude(ecs_t* ecs, ecs_system_t sys, ecs_comp_t comp);

/**
 * @brief Enables a system
 *
 * @param ecs    The ECS context
 * @param sys_id The specified system
 */
void ecs_enable_system(ecs_t* ecs, ecs_system_t sys);

/**
 * @brief Disables a system
 *
 * @param ecs The ECS context
 * @param sys The specified system
 */
void ecs_disable_system(ecs_t* ecs, ecs_system_t sys);

/**
 * @brief Updates the callbacks for an existing system
 *
 * @param ecs       The ECS context
 * @param sys       The system
 * @param system_cb Callback that is fired every update
 * @param add_cb    Called when an entity is added to the system (can be NULL)
 * @param remove_cb Called when an entity is removed from the system (can be NULL)
 */
void ecs_set_system_callbacks(ecs_t* ecs,
                              ecs_system_t sys,
                              ecs_system_fn system_cb,
                              ecs_on_join_fn on_join,
                              ecs_on_leave_fn on_leave);

/**
 * @brief Sets the user data for a system
 *
 * @param ecs   The ECS context
 * @param sys   The system
 * @param udata The user data to set
 */
void ecs_set_system_udata(ecs_t* ecs, ecs_system_t sys, void* udata);

/**
 * @brief Gets the user data from a system
 *
 * @param ecs The ECS context
 * @param sys The system
 * @return    The system's user data
 */
void* ecs_get_system_udata(ecs_t* ecs, ecs_system_t sys);

/**
 * @brief Sets the system's mask
 *
 * @param ecs  The ECS context
 * @param sys  The system
 * @param mask The mask to set
 */
void ecs_set_system_mask(ecs_t* ecs, ecs_system_t sys, ecs_mask_t mask);

/**
 * @brief Returns the system mask
 *
 * @param ecs The ECS context
 * @param sys The system
 * @return    The system's mask
 */
ecs_mask_t ecs_get_system_mask(ecs_t* ecs, ecs_system_t sys);

/**
 * @brief Returns the entities associated with the specified system
 */
ecs_entity_t* ecs_get_entity_array(ecs_t* ecs, ecs_system_t sys);

/**
 * @brief Returns the number of entities assigned to the specified system
 */
size_t ecs_get_entity_count(ecs_t* ecs, ecs_system_t sys);

/**
 * @brief Creates an entity
 *
 * @param ecs The ECS context
 *
 * @returns The new entity
 */
ecs_entity_t ecs_create(ecs_t* ecs);

/**
 * @brief Creates an entity, safe to call concurrently from an owned_initialize
 * system
 *
 * @param ecs The ECS context
 *
 * @returns The new entity
 */
ecs_entity_t ecs_create_owned(ecs_t* ecs);


/**
 * @brief Creates an entity, safe to call concurrently from an owned_initialize
 * system
 *
 * @param ecs The ECS context
 *
 * @returns The new entity
 */
ecs_entity_t ecs_create_owned_sharded(ecs_t* ecs);

/**
 * @brief Creates an entity, safe to call concurrently from an owned_initialize
 * system
 *
 * @param ecs The ECS context
 *
 * @returns The new entity
 */
ecs_entity_t ecs_create_owned_sharded_n(ecs_t* ecs, size_t shard_size);

/**
 * @brief Returns true if the entity is currently active and has not been queued
 * for destruction
 *
 * @param ecs The ECS context
 * @param entity The target entity
 */
bool ecs_is_ready(ecs_t* ecs, ecs_entity_t entity);

/**
 * @brief Test if entity has the specified component
 *
 * @param ecs    The ECS context
 * @param entity The entity
 * @param comp   The component
 *
 * @returns True if the entity has the component
 */
bool ecs_has(ecs_t* ecs, ecs_entity_t entity, ecs_comp_t comp);

/**
 * @brief Adds a component instance to an entity
 *
 * @param ecs    The ECS context
 * @param entity The entity
 * @param comp   The component
 * @param args   Optional arguments passed to the component constructor. When
 *               the add is deferred (called from within a running system) and
 *               the component was defined with a non-zero args_size, args_size
 *               bytes are copied and stored until the command queue is flushed,
 *               so the caller need not keep the args alive. If args_size is 0,
 *               no copy is made and NULL is forwarded to the constructor.
 *
 * @returns The component data
 */
void ecs_add(ecs_t* ecs, ecs_entity_t entity, ecs_comp_t comp, void* args);

/**
 * @brief Attaches a component to an entity, safe to call concurrently from an
 * owned_initialize system
 *
 * @param ecs    The ECS context
 * @param entity The entity, created by this thread via ecs_create_owned*
 * @param comp   The component
 * @param args   Optional arguments passed to the component constructor.
 *
 * @returns The component data
 */
void* ecs_add_owned(ecs_t* ecs, ecs_entity_t entity, ecs_comp_t comp, void* args);

/**
 * @brief Makes entities built by owned_initialize systems visible to systems
 *
 * @param ecs      The ECS context
 * @param entities The entities to match against systems
 * @param count    The number of entities
 */
void ecs_sync_owned(ecs_t* ecs, const ecs_entity_t* entities, size_t count);

/**
 * @brief Creates an entity and records it in this threads local store
 *
 * @param ecs The ECS context
 *
 * @returns The new entity
 */
ecs_entity_t ecs_create_owned_local(ecs_t* ecs);

/**
 * @brief Creates an entity and records it in this threads local store
 *
 * @param ecs        The ECS context
 * @param shard_size The shard size
 *
 * @returns The new entity
 */
ecs_entity_t ecs_create_owned_local_n(ecs_t* ecs, size_t shard_size);

/**
 * @brief Moves local entities into the global container
 *
 * @param ecs The ECS context
 */
void ecs_flush_owned(ecs_t* ecs);

/**
 * @brief Gets a component instance associated with an entity
 *
 * @param ecs    The ECS context
 * @param entity The entity
 * @param comp   The component
 *
 * @returns The component data
 */
void* ecs_get(ecs_t* ecs, ecs_entity_t entity, ecs_comp_t comp);

/**
 * @brief Copies data into a component instance associated with an entity
 *
 * If the entity does not have the component this adds the component to the
 * enity. If called during system iteration the operation is deferred until
 * after the system completes.
 *
 * @param ecs    The ECS context
 * @param entity The entity
 * @param comp   The component
 * @param data   Pointer to the data to copy into the component
 */
void ecs_set(ecs_t* ecs, ecs_entity_t entity, ecs_comp_t comp, void* data);

/**
 * @brief Destroys an entity
 *
 * Destroys an entity, releasing resources and returning it to the pool.
 *
 * @param ecs    The ECS context
 * @param entity The entity to destroy
 */
void ecs_destroy(ecs_t* ecs, ecs_entity_t entity);

/**
 * @brief Removes a component instance from an entity
 *
 * @param ecs    The ECS context
 * @param entity The entity
 * @param comp   The component
 */
void ecs_remove(ecs_t* ecs, ecs_entity_t entity, ecs_comp_t comp);

/**
 * @brief Update an individual system
 *
 * Calls system logic on required components, but not excluded ones.
 *
 * @param ecs The ECS context
 * @param sys The system to update
 * @param mask Bitmask that determines which systems run based on category.
 */
ecs_ret_t ecs_run_system(ecs_t* ecs, ecs_system_t sys, ecs_mask_t mask);

/**
 * @brief Updates all systems
 *
 * Calls {@link ecs_run_system} on all components in order of system
 * definition. In many cases it is better to call {@link ecs_run_system} as
 * needed.
 *
 * @param ecs The ECS context
 * @param mask Bitmask that determines which systems run based on category.
 */
ecs_ret_t ecs_run_systems(ecs_t* ecs, ecs_mask_t mask);

#ifdef __cplusplus
}
#endif

#endif // PITO_ECS_H

#ifdef PITO_ECS_IMPLEMENTATION // Define once

#ifndef PITO_ECS_MAX_COMPONENTS
#define PITO_ECS_MAX_COMPONENTS 32
#endif

#ifndef PITO_ECS_MAX_SYSTEMS
#define PITO_ECS_MAX_SYSTEMS 16
#endif

#ifndef PITO_ECS_COMP_BLOCK_SIZE
#define PITO_ECS_COMP_BLOCK_SIZE 64
#endif

#ifndef PITO_ECS_MAX_OWNED_LOCALS
#define PITO_ECS_MAX_OWNED_LOCALS 16
#endif

#ifndef PITO_ECS_INITIALIZE_SHARD_SIZE
#define PITO_ECS_INITIALIZE_SHARD_SIZE 1024
#endif

#ifdef NDEBUG
    #define PITO_ECS_ASSERT(expr) ((void)0)
#else
    #ifndef PITO_ECS_ASSERT
        #include <assert.h>
        #define PITO_ECS_ASSERT(expr) (assert(expr))
    #endif
#endif

#if !defined(PITO_ECS_MALLOC) || !defined(PITO_ECS_REALLOC) || !defined(PITO_ECS_FREE)
#include <stdlib.h>
#define PITO_ECS_MALLOC(size, ctx)       (malloc(size))
#define PITO_ECS_REALLOC(ptr, size, ctx) (realloc(ptr, size))
#define PITO_ECS_FREE(ptr, ctx)          (free(ptr))
#endif

#ifndef PITO_ECS_MEMSET
    #include <string.h>
    #define PITO_ECS_MEMSET memset
#endif

#ifndef PITO_ECS_MEMCPY
    #include <string.h>
    #define PITO_ECS_MEMCPY memcpy
#endif

#include <stdalign.h>

/*=============================================================================
 *  Aliases>
 *============================================================================*/

#define ECS_ASSERT                 PITO_ECS_ASSERT
#define ECS_MAX_COMPONENTS         PITO_ECS_MAX_COMPONENTS
#define ECS_MAX_SYSTEMS            PITO_ECS_MAX_SYSTEMS
#define ECS_COMP_BLOCK_SIZE        PITO_ECS_COMP_BLOCK_SIZE
#define ECS_INITIALIZE_SHARD_SIZE  PITO_ECS_INITIALIZE_SHARD_SIZE
#define ECS_MAX_OWNED_LOCALS       PITO_ECS_MAX_OWNED_LOCALS

#define ECS_MALLOC                 PITO_ECS_MALLOC
#define ECS_REALLOC                PITO_ECS_REALLOC
#define ECS_FREE                   PITO_ECS_FREE
#define ECS_MEMSET                 PITO_ECS_MEMSET
#define ECS_MEMCPY                 PITO_ECS_MEMCPY

#include <pthread.h>

typedef pthread_mutex_t ecs_mtx_t;

static void ecs_mtx_init_recursive(pthread_mutex_t* m)
{
    pthread_mutexattr_t attr;
    pthread_mutexattr_init(&attr);
    pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
    pthread_mutex_init(m, &attr);
    pthread_mutexattr_destroy(&attr);
}

#define ECS_MTX_INIT(m)    ecs_mtx_init_recursive(m)
#define ECS_MTX_LOCK(m)    pthread_mutex_lock(m)
#define ECS_MTX_UNLOCK(m)  pthread_mutex_unlock(m)
#define ECS_MTX_DESTROY(m) pthread_mutex_destroy(m)

#if defined(__cplusplus)

#include <atomic>

typedef std::atomic<size_t>* ecs_atomic_size_t;

#define ECS_ATOMIC_INIT(a, v)      (*(a) = new std::atomic<size_t>(v))
#define ECS_ATOMIC_DESTROY(a)      (delete *(a))
#define ECS_ATOMIC_STORE(a, v)     ((*(a))->store((v)))
#define ECS_ATOMIC_FETCH_ADD(a, v) ((*(a))->fetch_add((v)))

#define ECS_ATOMIC_STORE_RELEASE(a, v) ((*(a))->store((v), std::memory_order_release))
#define ECS_ATOMIC_LOAD_ACQUIRE(a)     ((*(a))->load(std::memory_order_acquire))

#else

#include <stdatomic.h>

typedef atomic_size_t ecs_atomic_size_t;

#define ECS_ATOMIC_INIT(a, v)      (atomic_init((a), (v)))
#define ECS_ATOMIC_DESTROY(a)      ((void)0)
#define ECS_ATOMIC_STORE(a, v)     (atomic_store((a), (v)))
#define ECS_ATOMIC_FETCH_ADD(a, v) (atomic_fetch_add((a), (v)))

#define ECS_ATOMIC_STORE_RELEASE(a, v) (atomic_store_explicit((a), (v), memory_order_release))
#define ECS_ATOMIC_LOAD_ACQUIRE(a)     (atomic_load_explicit((a), memory_order_acquire))

#endif

#if defined(__cplusplus)
    #define ECS_THREAD_LOCAL thread_local
#else
    #define ECS_THREAD_LOCAL _Thread_local
#endif

/*=============================================================================
 *  Data structures
 *============================================================================*/

#if ECS_MAX_COMPONENTS <= 32
typedef uint32_t ecs_bitset_t;
#elif ECS_MAX_COMPONENTS <= 64
typedef uint64_t ecs_bitset_t;
#else
#define ECS_BITSET_WIDTH 64
#define ECS_BITSET_SIZE (((ECS_MAX_COMPONENTS - 1) / ECS_BITSET_WIDTH) + 1)

typedef struct
{
    uint64_t array[ECS_BITSET_SIZE];
} ecs_bitset_t;

#endif // ECS_MAX_COMPONENTS

typedef struct ecs_arena_block_s
{
    uint8_t*              memory;
    size_t                size;
    size_t                offset;
    struct ecs_arena_block_s* next;
} ecs_arena_block_t;

typedef struct ecs_arena_s
{
    ecs_t* ecs;
    ecs_arena_block_t* first;
    ecs_arena_block_t* current;
    size_t block_size;
} ecs_arena_t;

// Data-structure for a packed array implementation that provides O(1) functions
// for adding, removing, and accessing entity IDs
typedef struct
{
    size_t        capacity;
    size_t        size;
    size_t*       sparse;
    ecs_entity_t* dense;
} ecs_sparse_set_t;

// A data-structure for providing O(1) operations for working with IDs
typedef struct
{
    size_t    capacity;
    size_t    size; // array size
    ecs_id_t* data;
} ecs_id_array_t;

typedef struct
{
    size_t  block_count;    // number of allocated blocks
    size_t  block_capacity; // capacity of the blocks[] pointer array
    size_t  comp_size;      // size of one component in bytes
    void**  blocks;         // array of pointers to fixed-size blocks
} ecs_comp_blocks_t;

typedef struct
{
    ecs_bitset_t comp_bits;
    bool         active;
    bool         ready;
    bool         pending; // sits in a local store, not yet flushed
} ecs_entity_data_t;

// Entities created by one thread via ecs_create_owned_local
typedef struct
{
    ecs_atomic_size_t count;
    size_t            size;
    size_t            capacity;
    ecs_entity_t*     entities;
    ecs_bitset_t      comp_bits; // archetype, all entities must be the same for one thread
    bool              comp_bits_set;
} ecs_owned_local_t;

typedef struct
{
    ecs_on_add_fn on_add;
    ecs_on_remove_fn on_remove;
    ecs_on_set_fn on_set;
    size_t size;
    void* default_value;
    size_t args_size;
    void* udata;
} ecs_comp_data_t;

typedef struct
{
    bool             active;
    ecs_sparse_set_t entity_ids;
    ecs_mask_t       mask;
    ecs_system_fn    system_cb;
    ecs_on_join_fn   on_join;
    ecs_on_leave_fn  on_leave;
    ecs_bitset_t     require_bits;
    ecs_bitset_t     exclude_bits;
    void*            udata;
    bool             owned_update;
    bool             owned_initialize;
    bool             reads_owned;
    bool             owned_publish_at_end;
} ecs_sys_data_t;

typedef enum
{
    ECS_CMD_ADD,
    ECS_CMD_REMOVE,
    ECS_CMD_SET,
    ECS_CMD_DESTROY,
} ecs_cmd_type_t;

typedef struct
{
    ecs_cmd_type_t type;
    ecs_entity_t   entity;
    ecs_comp_t     comp;
    void*          data;
    void*          args;
} ecs_cmd_t;

typedef struct
{
    ecs_cmd_t* data;
    size_t     size;
    size_t     capacity;
} ecs_cmd_array_t;

struct ecs_s
{
    ecs_id_array_t     entity_pool;
    ecs_entity_data_t* entities;
    size_t             entity_capacity;
    ecs_atomic_size_t  next_entity_id;
    size_t             generation;
    ecs_comp_data_t    comps[ECS_MAX_COMPONENTS];
    ecs_comp_blocks_t  comp_blocks[ECS_MAX_COMPONENTS];
    size_t             comp_count;
    ecs_sys_data_t     systems[ECS_MAX_SYSTEMS];
    size_t             system_count;
    bool               system_active;
    ecs_cmd_array_t    cmd_queue;
    ecs_owned_local_t* owned_locals[ECS_MAX_OWNED_LOCALS];
    ecs_atomic_size_t  owned_local_count;
    ecs_arena_t        arena;
    void*              mem_ctx;
    ecs_mtx_t          lock;
    ecs_mtx_t          comp_lock[ECS_MAX_COMPONENTS]; // guards comp_blocks[i]->blocks growth/lookup
    ecs_mtx_t          entity_lock;                   // guards the entities array (data + growth)
};

/*=============================================================================
 * Handle constructors
 *============================================================================*/
static inline ecs_entity_t ecs_make_entity(ecs_id_t id);
static inline ecs_comp_t ecs_make_comp(ecs_id_t id);
static inline ecs_system_t ecs_make_system(ecs_id_t id);

/*=============================================================================
 * Realloc wrapper
 *============================================================================*/
static void* ecs_realloc_zero(ecs_t* ecs, void* ptr, size_t old_size, size_t new_size);

/*=============================================================================
 * Tests if entity is active (created)
 *============================================================================*/
static inline bool ecs_is_active(ecs_t* ecs, ecs_id_t entity_id);

/*=============================================================================
 * Command queue functions
 *============================================================================*/
static void  ecs_cmd_array_init(ecs_t* ecs, ecs_cmd_array_t* queue, size_t capacity);
static void  ecs_cmd_array_free(ecs_t* ecs, ecs_cmd_array_t* queue);
static ecs_cmd_t* ecs_cmd_array_push(ecs_t* ecs, ecs_cmd_array_t* queue);
static void  ecs_cmd_flush_queue(ecs_t* ecs);

/*=============================================================================
 * Bitset functions
 *============================================================================*/
static inline void ecs_bitset_flip(ecs_bitset_t* set, int bit, bool on);
static inline bool ecs_bitset_is_zero(ecs_bitset_t* set);
static inline bool ecs_bitset_test(ecs_bitset_t* set, int bit);
static inline ecs_bitset_t ecs_bitset_and(ecs_bitset_t* set1, ecs_bitset_t* set2);
static inline ecs_bitset_t ecs_bitset_or(ecs_bitset_t* set1, ecs_bitset_t* set2);
static inline ecs_bitset_t ecs_bitset_not(ecs_bitset_t* set);
static inline bool ecs_bitset_equal(ecs_bitset_t* set1, ecs_bitset_t* set2);
static inline bool ecs_bitset_true(ecs_bitset_t* set);

/*=============================================================================
 * Arena functions
 *============================================================================*/
static ecs_arena_block_t* ecs_arena_block_create(ecs_t* ecs, size_t size);
static bool ecs_arena_init(ecs_t* ecs, ecs_arena_t* arena, size_t initial_block_size);
static bool ecs_arena_grow(ecs_t* ecs, ecs_arena_t* arena, size_t min_size);
static uintptr_t ecs_arena_align_forward(uintptr_t ptr, size_t align);
static void* ecs_arena_alloc_align(ecs_t* ecs, ecs_arena_t* arena, size_t size, size_t align);
static void* ecs_arena_alloc(ecs_t* ecs, ecs_arena_t* arena, size_t size);
static void ecs_arena_reset(ecs_t* ecs, ecs_arena_t* arena);
static void ecs_arena_destroy(ecs_t* ecs, ecs_arena_t* arena);

/*=============================================================================
 * Sparse set functions
 *============================================================================*/
static void ecs_sparse_set_init(ecs_t* ecs, ecs_sparse_set_t* set, size_t capacity);
static void ecs_sparse_set_free(ecs_t* ecs, ecs_sparse_set_t* set);
static bool ecs_sparse_set_add(ecs_t* ecs, ecs_sparse_set_t* set, ecs_id_t id);
static inline bool ecs_sparse_set_find(ecs_sparse_set_t* set, ecs_id_t id, size_t* found);
static inline bool ecs_sparse_set_remove(ecs_sparse_set_t* set, ecs_id_t id);

/*=============================================================================
 * System entity add/remove functions
 *============================================================================*/

static bool ecs_entity_system_test(ecs_bitset_t require_bits,
                                   ecs_bitset_t exclude_bits,
                                   ecs_bitset_t entity_bits);

static void ecs_sync_add_remove(ecs_t* ecs, ecs_id_t entity_id, ecs_id_t comp_id);
static void ecs_sync_destroy(ecs_t* ecs, ecs_id_t entity_id);

/*=============================================================================
 * Owned local functions
 *============================================================================*/

static ECS_THREAD_LOCAL ecs_owned_local_t* ecs_tl_local = NULL;
static ECS_THREAD_LOCAL size_t ecs_tl_local_generation = (size_t)-1;

static ECS_THREAD_LOCAL bool ecs_tl_publish_at_end = false;

static ecs_owned_local_t* ecs_owned_local_get(ecs_t* ecs);
static void ecs_owned_local_publish(ecs_t* ecs, ecs_owned_local_t* local);
static void ecs_owned_local_publish_tail(ecs_t* ecs);
static void ecs_owned_local_free_all(ecs_t* ecs);
static bool ecs_owned_local_matches(ecs_sys_data_t* sys_data, ecs_bitset_t comp_bits);
static ecs_ret_t ecs_run_owned_locals(ecs_t* ecs, ecs_sys_data_t* sys_data);

/*=============================================================================
 * ID array functions
 *============================================================================*/
static void   ecs_id_array_init(ecs_t* ecs, ecs_id_array_t* pool, size_t capacity);
static void   ecs_id_array_free(ecs_t* ecs, ecs_id_array_t* pool);
static inline void  ecs_id_array_push(ecs_t* ecs, ecs_id_array_t* pool, ecs_id_t id);
static inline ecs_id_t ecs_id_array_pop(ecs_id_array_t* pool);
static inline size_t ecs_id_array_size(ecs_id_array_t* pool);

/*=============================================================================
 * Component array functions
 *============================================================================*/
static void ecs_comp_blocks_init(ecs_t* ecs, ecs_comp_blocks_t* array, size_t size, size_t capacity);
static void ecs_comp_blocks_free(ecs_t* ecs, ecs_comp_blocks_t* array);
static void ecs_comp_blocks_resize(ecs_t* ecs, ecs_comp_blocks_t* array, ecs_id_t id);

/*=============================================================================
 * Validation functions
 *============================================================================*/
#ifndef NDEBUG
static bool ecs_is_not_null(void* ptr);
static bool ecs_is_valid_component_id(ecs_id_t id);
static bool ecs_is_valid_system_id(ecs_id_t id);
static bool ecs_is_valid_id(ecs_id_t id);
static bool ecs_is_valid_capacity(size_t capacity, size_t elem_size);
static bool ecs_is_entity_ready(ecs_t* ecs, ecs_id_t entity_id);
static bool ecs_is_component_ready(ecs_t* ecs, ecs_id_t comp_id);
static bool ecs_is_system_ready(ecs_t* ecs, ecs_id_t sys_id);
static bool ecs_is_not_pending(ecs_t* ecs, ecs_id_t entity_id);
#endif // NDEBUG

/*=============================================================================
 * Public API implementation
 *============================================================================*/

static size_t ecs_next_generation = 0;

ecs_t* ecs_new(size_t entity_capacity, void* mem_ctx)
{
    ECS_ASSERT(entity_capacity > 0);
    ECS_ASSERT(!ecs_is_valid_id(ECS_INVALID_ID) && "ecs_id_t is signed");

    ecs_t* ecs = (ecs_t*)ECS_MALLOC(sizeof(ecs_t), mem_ctx);

    // Out of memory
    if (NULL == ecs)
        return NULL;

    ECS_MEMSET(ecs, 0, sizeof(ecs_t));

    ecs->entity_capacity = (entity_capacity > 0) ? entity_capacity : 32;
    ECS_ATOMIC_INIT(&ecs->next_entity_id, 0);
    ECS_ATOMIC_INIT(&ecs->owned_local_count, 0);
    ecs->generation      = ecs_next_generation++;
    ecs->system_active   = false;
    ecs->mem_ctx         = mem_ctx;

    ECS_MTX_INIT(&ecs->lock);
    ECS_MTX_INIT(&ecs->entity_lock);

    for (size_t i = 0; i < ECS_MAX_COMPONENTS; i++)
        ECS_MTX_INIT(&ecs->comp_lock[i]);

    // Initialize entity pool and queues
    ecs_id_array_init(ecs, &ecs->entity_pool, entity_capacity);

    // Initialize deferred command queue
    ecs_cmd_array_init(ecs, &ecs->cmd_queue, entity_capacity);

    // Allocate entity array
    ECS_ASSERT(ecs_is_valid_capacity(ecs->entity_capacity, sizeof(ecs_entity_data_t)));
    ecs->entities = (ecs_entity_data_t*)ECS_MALLOC(ecs->entity_capacity * sizeof(ecs_entity_data_t),
                                                   ecs->mem_ctx);

    // Zero entity array
    ECS_MEMSET(ecs->entities, 0, ecs->entity_capacity * sizeof(ecs_entity_data_t));

    ecs_arena_init(ecs, &ecs->arena, 512);

    return ecs;
}

void ecs_free(ecs_t* ecs)
{
    ECS_ASSERT(ecs_is_not_null(ecs));

    ecs_id_array_free(ecs, &ecs->entity_pool);
    ecs_cmd_array_free(ecs, &ecs->cmd_queue);
    ecs_owned_local_free_all(ecs);
    ecs_arena_destroy(ecs, &ecs->arena);

    for (ecs_id_t comp_id = 0; comp_id < ecs->comp_count; comp_id++)
    {
        ecs_comp_blocks_t* comp_blocks = &ecs->comp_blocks[comp_id];
        ecs_comp_blocks_free(ecs, comp_blocks);
    }

    for (ecs_id_t sys_id = 0; sys_id < ecs->system_count; sys_id++)
    {
        ecs_sys_data_t* sys = &ecs->systems[sys_id];
        ecs_sparse_set_free(ecs, &sys->entity_ids);
    }

    for (ecs_id_t comp_id = 0; comp_id < ecs->comp_count; comp_id++)
    {
        ecs_comp_data_t* comp_data = &ecs->comps[comp_id];

        if (comp_data->default_value)
        {
            ECS_FREE(comp_data->default_value, ecs->mem_ctx);
        }
    }

    ECS_FREE(ecs->entities, ecs->mem_ctx);

    ECS_ATOMIC_DESTROY(&ecs->next_entity_id);
    ECS_ATOMIC_DESTROY(&ecs->owned_local_count);

    ECS_MTX_DESTROY(&ecs->lock);
    ECS_MTX_DESTROY(&ecs->entity_lock);

    for (size_t i = 0; i < ECS_MAX_COMPONENTS; i++)
        ECS_MTX_DESTROY(&ecs->comp_lock[i]);

    ECS_FREE(ecs, ecs->mem_ctx);
}

void ecs_reset(ecs_t* ecs)
{
    ECS_ASSERT(ecs_is_not_null(ecs));

    ECS_MTX_LOCK(&ecs->lock);

    ecs->entity_pool.size = 0;

    ECS_MEMSET(ecs->entities, 0, ecs->entity_capacity * sizeof(ecs_entity_data_t));

    ecs_owned_local_free_all(ecs);

    ECS_ATOMIC_STORE(&ecs->next_entity_id, 0);
    ecs->generation = ecs_next_generation++;

    for (ecs_id_t sys_id = 0; sys_id < ecs->system_count; sys_id++)
    {
        ecs->systems[sys_id].entity_ids.size = 0;
    }

    ECS_MTX_UNLOCK(&ecs->lock);
}

ecs_comp_t ecs_define_component(ecs_t* ecs,
                                size_t size,
                                const ecs_comp_desc_t* desc)
{
    ECS_ASSERT(ecs_is_not_null(ecs));
    ECS_ASSERT(size > 0);

    ECS_MTX_LOCK(&ecs->lock);

    ECS_ASSERT(ecs->comp_count < ECS_MAX_COMPONENTS);

    ecs_comp_t comp = ecs_make_comp(ecs->comp_count);

    ecs_comp_blocks_t* comp_blocks = &ecs->comp_blocks[comp.id];
    ecs_comp_blocks_init(ecs, comp_blocks, size, ecs->entity_capacity);

    ecs_comp_data_t* comp_data = &ecs->comps[comp.id];

    ECS_MEMSET(comp_data, 0, sizeof(ecs_comp_data_t));
    comp_data->size = size;

    if (desc)
    {
        comp_data->on_add = desc->on_add_cb;
        comp_data->on_remove = desc->on_remove_cb;
        comp_data->on_set = desc->on_set_cb;
        comp_data->args_size = desc->args_size;
        comp_data->udata = desc->udata;

        if (desc->default_value)
        {
            comp_data->default_value = ECS_MALLOC(size, ecs->mem_ctx);
            ECS_MEMCPY(comp_data->default_value, desc->default_value, size);
        }
    }

    ecs->comp_count++;

    ECS_MTX_UNLOCK(&ecs->lock);

    return comp;
}

ecs_system_t ecs_define_system(ecs_t* ecs,
                               ecs_system_fn system_cb,
                               const ecs_sys_desc_t* desc)
{
    ECS_ASSERT(ecs_is_not_null(ecs));
    ECS_ASSERT(NULL != system_cb);

    ECS_MTX_LOCK(&ecs->lock);

    ECS_ASSERT(ecs->system_count < ECS_MAX_SYSTEMS);

    ecs_system_t sys = ecs_make_system(ecs->system_count);
    ecs_sys_data_t* sys_data = &ecs->systems[sys.id];

    ECS_MEMSET(sys_data, 0, sizeof(ecs_sys_data_t));

    ecs_sparse_set_init(ecs, &sys_data->entity_ids, ecs->entity_capacity);

    sys_data->system_cb = system_cb;
    sys_data->active = true;

    if (desc)
    {
        sys_data->mask = desc->mask;
        sys_data->on_join = desc->on_join_cb;
        sys_data->on_leave = desc->on_leave_cb;
        sys_data->udata = desc->udata;
        sys_data->owned_update = desc->owned_update;
        sys_data->owned_initialize = desc->owned_initialize;
        sys_data->reads_owned = desc->reads_owned;
        sys_data->owned_publish_at_end = desc->owned_publish_at_end;
    }

    ecs->system_count++;

    ECS_MTX_UNLOCK(&ecs->lock);

    return sys;
}

void ecs_require(ecs_t* ecs, ecs_system_t sys, ecs_comp_t comp)
{
    ECS_ASSERT(ecs_is_not_null(ecs));
    ECS_ASSERT(ecs_is_valid_system_id(sys.id));
    ECS_ASSERT(ecs_is_valid_component_id(comp.id));

    ECS_MTX_LOCK(&ecs->lock);

    ECS_ASSERT(ecs_is_system_ready(ecs, sys.id));
    ECS_ASSERT(ecs_is_component_ready(ecs, comp.id));

    // Set system component bit for the specified component
    ecs_sys_data_t* sys_data = &ecs->systems[sys.id];
    ecs_bitset_flip(&sys_data->require_bits, comp.id, true);

    ECS_MTX_UNLOCK(&ecs->lock);
}

void ecs_exclude(ecs_t* ecs, ecs_system_t sys, ecs_comp_t comp)
{
    ECS_ASSERT(ecs_is_not_null(ecs));
    ECS_ASSERT(ecs_is_valid_system_id(sys.id));
    ECS_ASSERT(ecs_is_valid_component_id(comp.id));

    ECS_MTX_LOCK(&ecs->lock);

    ECS_ASSERT(ecs_is_system_ready(ecs, sys.id));
    ECS_ASSERT(ecs_is_component_ready(ecs, comp.id));

    // Set system component bit for the specified component
    ecs_sys_data_t* sys_data = &ecs->systems[sys.id];
    ecs_bitset_flip(&sys_data->exclude_bits, comp.id, true);

    ECS_MTX_UNLOCK(&ecs->lock);
}

void ecs_enable_system(ecs_t* ecs, ecs_system_t sys)
{
    ECS_ASSERT(ecs_is_not_null(ecs));
    ECS_ASSERT(ecs_is_valid_system_id(sys.id));

    ECS_MTX_LOCK(&ecs->lock);

    ECS_ASSERT(ecs_is_system_ready(ecs, sys.id));

    ecs_sys_data_t* sys_data = &ecs->systems[sys.id];
    sys_data->active = true;

    ECS_MTX_UNLOCK(&ecs->lock);
}

void ecs_disable_system(ecs_t* ecs, ecs_system_t sys)
{
    ECS_ASSERT(ecs_is_not_null(ecs));
    ECS_ASSERT(ecs_is_valid_system_id(sys.id));

    ECS_MTX_LOCK(&ecs->lock);

    ECS_ASSERT(ecs_is_system_ready(ecs, sys.id));

    ecs_sys_data_t* sys_data = &ecs->systems[sys.id];
    sys_data->active = false;

    ECS_MTX_UNLOCK(&ecs->lock);
}

void ecs_set_system_callbacks(ecs_t* ecs,
                              ecs_system_t sys,
                              ecs_system_fn system_cb,
                              ecs_on_join_fn on_join,
                              ecs_on_leave_fn on_leave)
{
    ECS_ASSERT(ecs_is_not_null(ecs));
    ECS_ASSERT(ecs_is_valid_system_id(sys.id));
    ECS_ASSERT(NULL != system_cb);

    ECS_MTX_LOCK(&ecs->lock);

    ECS_ASSERT(ecs_is_system_ready(ecs, sys.id));

    ecs_sys_data_t* sys_data = &ecs->systems[sys.id];
    sys_data->system_cb = system_cb;
    sys_data->on_join = on_join;
    sys_data->on_leave = on_leave;

    ECS_MTX_UNLOCK(&ecs->lock);
}

void ecs_set_system_udata(ecs_t* ecs, ecs_system_t sys, void* udata)
{
    ECS_ASSERT(ecs_is_not_null(ecs));
    ECS_ASSERT(ecs_is_valid_system_id(sys.id));

    ECS_MTX_LOCK(&ecs->lock);

    ECS_ASSERT(ecs_is_system_ready(ecs, sys.id));
    ecs->systems[sys.id].udata = udata;
    ECS_MTX_UNLOCK(&ecs->lock);
}

void* ecs_get_system_udata(ecs_t* ecs, ecs_system_t sys)
{
    ECS_ASSERT(ecs_is_not_null(ecs));
    ECS_ASSERT(ecs_is_valid_system_id(sys.id));

    ECS_MTX_LOCK(&ecs->lock);

    ECS_ASSERT(ecs_is_system_ready(ecs, sys.id));
    void* udata = ecs->systems[sys.id].udata;
    ECS_MTX_UNLOCK(&ecs->lock);

    return udata;
}

void ecs_set_system_mask(ecs_t* ecs, ecs_system_t sys, ecs_mask_t mask)
{
    ECS_ASSERT(ecs_is_not_null(ecs));
    ECS_ASSERT(ecs_is_valid_system_id(sys.id));

    ECS_MTX_LOCK(&ecs->lock);

    ECS_ASSERT(ecs_is_system_ready(ecs, sys.id));
    ecs->systems[sys.id].mask = mask;
    ECS_MTX_UNLOCK(&ecs->lock);
}

ecs_mask_t ecs_get_system_mask(ecs_t* ecs, ecs_system_t sys)
{
    ECS_ASSERT(ecs_is_not_null(ecs));
    ECS_ASSERT(ecs_is_valid_system_id(sys.id));

    ECS_MTX_LOCK(&ecs->lock);

    ECS_ASSERT(ecs_is_system_ready(ecs, sys.id));
    ecs_mask_t mask = ecs->systems[sys.id].mask;
    ECS_MTX_UNLOCK(&ecs->lock);

    return mask;
}

ecs_entity_t* ecs_get_entity_array(ecs_t* ecs, ecs_system_t sys)
{
    ECS_ASSERT(ecs_is_not_null(ecs));
    ECS_ASSERT(ecs_is_valid_system_id(sys.id));

    ECS_MTX_LOCK(&ecs->lock);

    ECS_ASSERT(ecs_is_system_ready(ecs, sys.id));
    ecs_entity_t* dense = ecs->systems[sys.id].entity_ids.dense;
    ECS_MTX_UNLOCK(&ecs->lock);

    return dense;
}

//TODO: maybe add thread local here too?
size_t ecs_get_entity_count(ecs_t* ecs, ecs_system_t sys)
{
    ECS_ASSERT(ecs_is_not_null(ecs));
    ECS_ASSERT(ecs_is_valid_system_id(sys.id));

    ECS_MTX_LOCK(&ecs->lock);

    ECS_ASSERT(ecs_is_system_ready(ecs, sys.id));
    size_t count = ecs->systems[sys.id].entity_ids.size;
    ECS_MTX_UNLOCK(&ecs->lock);

    return count;
}

ecs_entity_t ecs_create(ecs_t* ecs)
{
    ECS_ASSERT(ecs_is_not_null(ecs));

    ECS_MTX_LOCK(&ecs->lock);

    ecs_id_t entity_id = 0;

    ECS_MTX_LOCK(&ecs->entity_lock);

    // If there is an ID in the pool, pop it
    ecs_id_array_t* pool = &ecs->entity_pool;

    if (0 != ecs_id_array_size(pool))
    {
        entity_id = ecs_id_array_pop(pool);
    }
    else
    {
        // Otherwise, issue a fresh ID
        entity_id = (ecs_id_t)ECS_ATOMIC_FETCH_ADD(&ecs->next_entity_id, 1);

        // Grow the entities array if necessary
        if (entity_id >= ecs->entity_capacity)
        {
            size_t old_capacity = ecs->entity_capacity;
            size_t new_capacity = 2 * old_capacity;

            ECS_ASSERT(ecs_is_valid_capacity(new_capacity, sizeof(ecs_entity_data_t)));
            ecs->entities = (ecs_entity_data_t*)ecs_realloc_zero(ecs, ecs->entities,
                                                                 old_capacity * sizeof(ecs_entity_data_t),
                                                                 new_capacity * sizeof(ecs_entity_data_t));

            ecs->entity_capacity = new_capacity;
        }
    }

    // Activate the entity and return a handle
    ecs->entities[entity_id].active = true;
    ecs->entities[entity_id].ready  = true;

    ECS_MTX_UNLOCK(&ecs->entity_lock);

    ECS_MTX_UNLOCK(&ecs->lock);

    return ecs_make_entity(entity_id);
}

//TODO: entities array growing not implemented yet
ecs_entity_t ecs_create_owned(ecs_t* ecs)
{
    ECS_ASSERT(ecs_is_not_null(ecs));

    ecs_id_t entity_id = (ecs_id_t)ECS_ATOMIC_FETCH_ADD(&ecs->next_entity_id, 1);

    ECS_ASSERT((size_t)entity_id < ecs->entity_capacity);

    ecs->entities[entity_id].active = true;
    ecs->entities[entity_id].ready  = true;

    return ecs_make_entity(entity_id);
}

typedef struct
{
    size_t generation;
    ecs_id_t next;
    ecs_id_t end;
} ecs_id_shard_t;

//TODO: entities array growing not implemented yet
ecs_entity_t ecs_create_owned_sharded_n(ecs_t* ecs, size_t shard_size)
{
    ECS_ASSERT(ecs_is_not_null(ecs));
    ECS_ASSERT(shard_size > 0);

    static ECS_THREAD_LOCAL ecs_id_shard_t shard = { (size_t)-1, 0, 0 };

    if (shard.generation != ecs->generation || shard.next >= shard.end)
    {
        ecs_id_t base = (ecs_id_t)ECS_ATOMIC_FETCH_ADD(&ecs->next_entity_id, shard_size);
        shard.generation = ecs->generation;
        shard.next       = base;
        shard.end        = base + (ecs_id_t)shard_size;
    }

    ecs_id_t entity_id = shard.next++;

    ECS_ASSERT((size_t)entity_id < ecs->entity_capacity);

    ecs->entities[entity_id].active = true;
    ecs->entities[entity_id].ready  = true;

    return ecs_make_entity(entity_id);
}

ecs_entity_t ecs_create_owned_sharded(ecs_t* ecs)
{
    return ecs_create_owned_sharded_n(ecs, ECS_INITIALIZE_SHARD_SIZE);
}

ecs_entity_t ecs_create_owned_local_n(ecs_t* ecs, size_t shard_size)
{
    ECS_ASSERT(ecs_is_not_null(ecs));

    ecs_owned_local_t* local = ecs_owned_local_get(ecs);

    // Publishes the previous entity
    if (!ecs_tl_publish_at_end)
        ecs_owned_local_publish(ecs, local);

    ecs_entity_t entity = ecs_create_owned_sharded_n(ecs, shard_size);

    //TODO: growing not implemented yet
    ECS_ASSERT(local->size < local->capacity);

    local->entities[local->size++] = entity;
    ecs->entities[entity.id].pending = true;

    return entity;
}

ecs_entity_t ecs_create_owned_local(ecs_t* ecs)
{
    return ecs_create_owned_local_n(ecs, ECS_INITIALIZE_SHARD_SIZE);
}

void ecs_flush_owned(ecs_t* ecs)
{
    ECS_ASSERT(ecs_is_not_null(ecs));

    ECS_MTX_LOCK(&ecs->lock);

    size_t local_count = ECS_ATOMIC_LOAD_ACQUIRE(&ecs->owned_local_count);

    for (size_t i = 0; i < local_count; i++)
    {
        ecs_owned_local_t* local = ecs->owned_locals[i];

        size_t count = ECS_ATOMIC_LOAD_ACQUIRE(&local->count);

        ECS_ASSERT(count == local->size);

        if (0 == count)
            continue;

        for (ecs_id_t sys_id = 0; sys_id < ecs->system_count; sys_id++)
        {
            ecs_sys_data_t* sys_data = &ecs->systems[sys_id];

            if (!ecs_owned_local_matches(sys_data, local->comp_bits))
                continue;

            for (size_t j = 0; j < count; j++)
            {
                if (ecs_sparse_set_add(ecs, &sys_data->entity_ids, local->entities[j].id) &&
                    sys_data->on_join)
                    sys_data->on_join(ecs, local->entities[j], sys_data->udata);
            }
        }

        for (size_t j = 0; j < count; j++)
            ecs->entities[local->entities[j].id].pending = false;

        ECS_ATOMIC_STORE_RELEASE(&local->count, 0);
        local->size = 0;
        local->comp_bits_set = false;
    }

    ECS_MTX_UNLOCK(&ecs->lock);
}

bool ecs_is_ready(ecs_t* ecs, ecs_entity_t entity)
{
    ECS_ASSERT(ecs_is_not_null(ecs));

    ECS_MTX_LOCK(&ecs->lock);
    bool ready = ecs->entities[entity.id].ready;
    ECS_MTX_UNLOCK(&ecs->lock);

    return ready;
}

void ecs_set(ecs_t* ecs, ecs_entity_t entity, ecs_comp_t comp, void* data)
{
    ECS_ASSERT(ecs_is_not_null(ecs));
    ECS_ASSERT(ecs_is_valid_id(entity.id));
    ECS_ASSERT(ecs_is_valid_component_id(comp.id));

    ECS_MTX_LOCK(&ecs->lock);

    ECS_ASSERT(ecs_is_component_ready(ecs, comp.id));
    ECS_ASSERT(ecs_is_entity_ready(ecs, entity.id));
    ECS_ASSERT(ecs_is_not_pending(ecs, entity.id));

    if (!ecs_has(ecs, entity, comp))
    {
        ecs_add(ecs, entity, comp, NULL);
    }

    ecs_comp_data_t* comp_data = &ecs->comps[comp.id];

    if (ecs->system_active)
    {
        ecs_cmd_t* cmd = ecs_cmd_array_push(ecs, &ecs->cmd_queue);
        cmd->type   = ECS_CMD_SET;
        cmd->entity = entity;
        cmd->comp   = comp;
        cmd->data   = ecs_arena_alloc(ecs, &ecs->arena, comp_data->size);
        ECS_MEMCPY(cmd->data, data, comp_data->size);
        ECS_MTX_UNLOCK(&ecs->lock);
        return;
    }

    void* comp_ptr = ecs_get(ecs, entity, comp);
    ECS_MEMCPY(comp_ptr, data, comp_data->size);

    // Callback
    if (comp_data->on_set)
        comp_data->on_set(ecs, entity, comp, comp_data->udata);

    ECS_MTX_UNLOCK(&ecs->lock);
}

void ecs_destroy(ecs_t* ecs, ecs_entity_t entity)
{
    ECS_ASSERT(ecs_is_not_null(ecs));
    ECS_ASSERT(ecs_is_valid_id(entity.id));

    ECS_MTX_LOCK(&ecs->lock);

    ECS_ASSERT(ecs_is_active(ecs, entity.id));
    ECS_ASSERT(ecs_is_not_pending(ecs, entity.id));

    if (!ecs_is_active(ecs, entity.id))
    {
        ECS_MTX_UNLOCK(&ecs->lock);
        return;
    }

    ECS_MTX_LOCK(&ecs->entity_lock);
    ecs_entity_data_t* entity_data = &ecs->entities[entity.id];

    ecs_bitset_t comp_bits = entity_data->comp_bits;
    ECS_MTX_UNLOCK(&ecs->entity_lock);

    if (ecs->system_active)
    {
        ecs_cmd_t* cmd = ecs_cmd_array_push(ecs, &ecs->cmd_queue);
        cmd->type   = ECS_CMD_DESTROY;
        cmd->entity = entity;
        ECS_MTX_LOCK(&ecs->entity_lock);
        entity_data->ready = false;
        ECS_MTX_UNLOCK(&ecs->entity_lock);
        ECS_MTX_UNLOCK(&ecs->lock);
        return;
    }

    for (ecs_id_t comp_id = 0; comp_id < ecs->comp_count; comp_id++)
    {
        if (ecs_bitset_test(&comp_bits, comp_id))
        {
            ecs_comp_data_t* comp_data = &ecs->comps[comp_id];

            if (comp_data->on_remove)
            {
                ecs_comp_t comp = ecs_make_comp(comp_id);
                comp_data->on_remove(ecs, entity, comp, comp_data->udata);
            }
        }
    }

    ecs_sync_destroy(ecs, entity.id);

    ecs_id_array_t* pool = &ecs->entity_pool;
    ecs_id_array_push(ecs, pool, entity.id);

    ECS_MTX_LOCK(&ecs->entity_lock);
    ECS_MEMSET(&entity_data->comp_bits, 0, sizeof(ecs_bitset_t));
    entity_data->active = false;
    entity_data->ready  = false;
    ECS_MTX_UNLOCK(&ecs->entity_lock);

    ECS_MTX_UNLOCK(&ecs->lock);
}

bool ecs_has(ecs_t* ecs, ecs_entity_t entity, ecs_comp_t comp)
{
    ECS_ASSERT(ecs_is_not_null(ecs));
    ECS_ASSERT(ecs_is_valid_id(entity.id));
    ECS_ASSERT(ecs_is_valid_component_id(comp.id));

    
    ECS_MTX_LOCK(&ecs->entity_lock);

    // Load entity data
    ecs_entity_data_t* entity_data = &ecs->entities[entity.id];

    bool result;

    if (!entity_data->ready)
    {
        result = false;
    }
    else
    {
        // The component belongs to the entity if the corresponding bit is set
        result = ecs_bitset_test(&entity_data->comp_bits, comp.id);
    }

    ECS_MTX_UNLOCK(&ecs->entity_lock);

    return result;
}

void* ecs_get(ecs_t* ecs, ecs_entity_t entity, ecs_comp_t comp)
{
    ECS_ASSERT(ecs_is_not_null(ecs));
    ECS_ASSERT(ecs_is_valid_id(entity.id));
    ECS_ASSERT(ecs_is_valid_component_id(comp.id));


    ECS_MTX_LOCK(&ecs->comp_lock[comp.id]);

    ECS_ASSERT(ecs_is_component_ready(ecs, comp.id));
    ECS_ASSERT(ecs_is_entity_ready(ecs, entity.id));

    // Map entity ID to block and slot within that block.
    // Blocks are never reallocated, so returned pointers remain stable.
    ecs_comp_blocks_t* comp_blocks = &ecs->comp_blocks[comp.id];

    size_t block = entity.id / ECS_COMP_BLOCK_SIZE;
    size_t slot  = entity.id % ECS_COMP_BLOCK_SIZE;

    void* ptr = (char*)comp_blocks->blocks[block] + (comp_blocks->comp_size * slot);

    ECS_MTX_UNLOCK(&ecs->comp_lock[comp.id]);

    return ptr;
}

void ecs_add(ecs_t* ecs, ecs_entity_t entity, ecs_comp_t comp, void* args)
{
    ECS_ASSERT(ecs_is_not_null(ecs));
    ECS_ASSERT(ecs_is_valid_id(entity.id));
    ECS_ASSERT(ecs_is_valid_component_id(comp.id));

    ECS_MTX_LOCK(&ecs->lock);

    ECS_ASSERT(ecs_is_entity_ready(ecs, entity.id));
    ECS_ASSERT(ecs_is_component_ready(ecs, comp.id));
    ECS_ASSERT(ecs_is_not_pending(ecs, entity.id));

    if (ecs_has(ecs, entity, comp))
    {
        ECS_MTX_UNLOCK(&ecs->lock);
        return;
    }

    ECS_MTX_LOCK(&ecs->entity_lock);
    ecs_entity_data_t* entity_data = &ecs->entities[entity.id];
    ecs_bitset_flip(&entity_data->comp_bits, comp.id, true);
    ECS_MTX_UNLOCK(&ecs->entity_lock);

    // Load component
    ecs_comp_blocks_t* comp_blocks = &ecs->comp_blocks[comp.id];

    // Grow the component array now (not deferred) so that ecs_get can safely
    // index into it immediately, since ecs_has already reports the component
    // as present as soon as the bit above is flipped. Guarded by
    // comp_lock[comp.id] so this stays correct against ecs_get reading
    // comp_blocks->blocks[block] under the same lock from another thread.
    ECS_MTX_LOCK(&ecs->comp_lock[comp.id]);
    ecs_comp_blocks_resize(ecs, comp_blocks, entity.id);
    ECS_MTX_UNLOCK(&ecs->comp_lock[comp.id]);

    ecs_comp_data_t* comp_data = &ecs->comps[comp.id];

    if (ecs->system_active)
    {
        ecs_cmd_t* cmd = ecs_cmd_array_push(ecs, &ecs->cmd_queue);
        cmd->type      = ECS_CMD_ADD;
        cmd->entity    = entity;
        cmd->comp      = comp;

        // Copy the constructor args into the command arena so the caller is
        // not required to keep them alive until the queue is flushed. The copy
        // is handed back to ecs_add (and thus the on_add constructor) when the
        // queue is flushed at the end of the system run.
        if (args && comp_data->args_size > 0)
        {
            cmd->args = ecs_arena_alloc(ecs, &ecs->arena, comp_data->args_size);
            ECS_MEMCPY(cmd->args, args, comp_data->args_size);
        }

        ECS_MTX_UNLOCK(&ecs->lock);
        return;
    }

    // Get pointer to component
    void* comp_ptr = ecs_get(ecs, entity, comp);

    // Set default value
    if (comp_data->default_value)
        ECS_MEMCPY(comp_ptr, comp_data->default_value, comp_data->size);
    else
        ECS_MEMSET(comp_ptr, 0, comp_blocks->comp_size);

    // Call constructor
    if (comp_data->on_add)
        comp_data->on_add(ecs, entity, comp, args, comp_data->udata);

    // Add/remove entity to/from systems based on matching criteria
    ecs_sync_add_remove(ecs, entity.id, comp.id);

    ECS_MTX_UNLOCK(&ecs->lock);
}


void* ecs_add_owned(ecs_t* ecs, ecs_entity_t entity, ecs_comp_t comp, void* args)
{
    ECS_ASSERT(ecs_is_not_null(ecs));
    ECS_ASSERT(ecs_is_valid_id(entity.id));
    ECS_ASSERT(ecs_is_valid_component_id(comp.id));
    ECS_ASSERT(ecs_is_entity_ready(ecs, entity.id));
    ECS_ASSERT(ecs_is_component_ready(ecs, comp.id));

    ecs_comp_blocks_t* comp_blocks = &ecs->comp_blocks[comp.id];

    size_t block = (size_t)entity.id / ECS_COMP_BLOCK_SIZE;
    size_t slot  = (size_t)entity.id % ECS_COMP_BLOCK_SIZE;

    // TODO: need to grow comp_blocks
    ECS_ASSERT(block < comp_blocks->block_count);

    ecs_bitset_flip(&ecs->entities[entity.id].comp_bits, comp.id, true);

    // skip ecs_get to avoid locks for owned entity
    void* comp_ptr = (char*)comp_blocks->blocks[block] + (comp_blocks->comp_size * slot);

    ecs_comp_data_t* comp_data = &ecs->comps[comp.id];

    if (comp_data->default_value)
        ECS_MEMCPY(comp_ptr, comp_data->default_value, comp_data->size);
    else
        ECS_MEMSET(comp_ptr, 0, comp_blocks->comp_size);


    if (comp_data->on_add)
        comp_data->on_add(ecs, entity, comp, args, comp_data->udata);

    return comp_ptr;
}

void ecs_sync_owned(ecs_t* ecs, const ecs_entity_t* entities, size_t count)
{
    ECS_ASSERT(ecs_is_not_null(ecs));
    ECS_ASSERT(0 == count || NULL != entities);

    ECS_MTX_LOCK(&ecs->lock);

    for (size_t i = 0; i < count; i++)
    {
        ecs_id_t entity_id = entities[i].id;

        ECS_ASSERT(ecs_is_valid_id(entity_id));
        ECS_ASSERT(ecs_is_entity_ready(ecs, entity_id));

        ecs_bitset_t comp_bits = ecs->entities[entity_id].comp_bits;

        for (ecs_id_t comp_id = 0; comp_id < ecs->comp_count; comp_id++)
        {
            if (ecs_bitset_test(&comp_bits, comp_id))
                //TODO: maybe add shorter version of sync here
                ecs_sync_add_remove(ecs, entity_id, comp_id);
        }
    }

    ECS_MTX_UNLOCK(&ecs->lock);
}

void ecs_remove(ecs_t* ecs, ecs_entity_t entity, ecs_comp_t comp)
{
    ECS_ASSERT(ecs_is_not_null(ecs));
    ECS_ASSERT(ecs_is_valid_id(entity.id));
    ECS_ASSERT(ecs_is_valid_component_id(comp.id));

    ECS_MTX_LOCK(&ecs->lock);

    ECS_ASSERT(ecs_is_component_ready(ecs, comp.id));
    ECS_ASSERT(ecs_is_entity_ready(ecs, entity.id));
    ECS_ASSERT(ecs_is_not_pending(ecs, entity.id));

    if (!ecs_has(ecs, entity, comp))
    {
        ECS_MTX_UNLOCK(&ecs->lock);
        return;
    }

    // Set entity component bit that determines which systems this entity
    // belongs to.
    ECS_MTX_LOCK(&ecs->entity_lock);
    ecs_bitset_flip(&ecs->entities[entity.id].comp_bits, comp.id, false);
    ECS_MTX_UNLOCK(&ecs->entity_lock);

    if (ecs->system_active)
    {
        ecs_cmd_t* cmd = ecs_cmd_array_push(ecs, &ecs->cmd_queue);
        cmd->type   = ECS_CMD_REMOVE;
        cmd->entity = entity;
        cmd->comp   = comp;
        ECS_MTX_UNLOCK(&ecs->lock);
        return;
    }

    // Fire callback
    ecs_comp_data_t* comp_data = &ecs->comps[comp.id];

    if (comp_data->on_remove)
        comp_data->on_remove(ecs, entity, comp, comp_data->udata);

    // Add/remove entity to/from systems based on matching criteria
    ecs_sync_add_remove(ecs, entity.id, comp.id);

    ECS_MTX_UNLOCK(&ecs->lock);
}

ecs_ret_t ecs_run_system(ecs_t* ecs, ecs_system_t sys, ecs_mask_t mask)
{
    ECS_ASSERT(ecs_is_not_null(ecs));
    ECS_ASSERT(ecs_is_valid_system_id(sys.id));


    ecs_sys_data_t* sys_data = &ecs->systems[sys.id];

    if (sys_data->owned_update || sys_data->owned_initialize)
    {
        ECS_ASSERT(ecs_is_system_ready(ecs, sys.id));

        if (!sys_data->active)
            return 0;

        if (0 != sys_data->mask && !(sys_data->mask & mask))
            return 0;

        if (sys_data->owned_initialize)
            ecs_tl_publish_at_end = sys_data->owned_publish_at_end;

        ecs_ret_t code = sys_data->system_cb(ecs,
                                             sys_data->entity_ids.dense,
                                             sys_data->entity_ids.size,
                                             sys_data->udata);

        if (sys_data->owned_initialize)
            ecs_owned_local_publish_tail(ecs);

        if (0 == code && sys_data->reads_owned)
            code = ecs_run_owned_locals(ecs, sys_data);

        return code;
    }

    ECS_MTX_LOCK(&ecs->lock);

    ECS_ASSERT(ecs_is_system_ready(ecs, sys.id));

    if (!sys_data->active)
    {
        ECS_MTX_UNLOCK(&ecs->lock);
        return 0;
    }

    if (0 != sys_data->mask && !(sys_data->mask & mask))
    {
        ECS_MTX_UNLOCK(&ecs->lock);
        return 0;
    }

    ecs->system_active = true;

    ecs_ret_t code = sys_data->system_cb(ecs,
                     sys_data->entity_ids.dense,
                     sys_data->entity_ids.size,
                     sys_data->udata);

    if (0 == code && sys_data->reads_owned)
        code = ecs_run_owned_locals(ecs, sys_data);

    ecs->system_active = false;

    ecs_cmd_flush_queue(ecs);
    ecs_arena_reset(ecs, &ecs->arena);

    ECS_MTX_UNLOCK(&ecs->lock);

    return code;
}

ecs_ret_t ecs_run_systems(ecs_t* ecs, ecs_mask_t mask)
{
    ECS_ASSERT(ecs_is_not_null(ecs));

    for (ecs_id_t sys_id = 0; sys_id < ecs->system_count; sys_id++)
    {
        ecs_system_t sys = ecs_make_system(sys_id);
        ecs_ret_t code = ecs_run_system(ecs, sys, mask);

        if (0 != code)
            return code;
    }

    return 0;
}

/*=============================================================================
 * Handle constructors
 *============================================================================*/
static inline ecs_entity_t ecs_make_entity(ecs_id_t id)
{
    return (ecs_entity_t){ id };
}

static inline ecs_comp_t ecs_make_comp(ecs_id_t id)
{
    return (ecs_comp_t){ id };
}

static inline ecs_system_t ecs_make_system(ecs_id_t id)
{
    return (ecs_system_t){ id };
}

/*=============================================================================
 * Realloc wrapper
 *============================================================================*/
static void* ecs_realloc_zero(ecs_t* ecs, void* ptr, size_t old_size, size_t new_size)
{
    (void)ecs;

    ptr = ECS_REALLOC(ptr, new_size, ecs->mem_ctx);

    if (new_size > old_size && ptr) {
        size_t diff = new_size - old_size;
        void* start = ((char*)ptr)+ old_size;
        ECS_MEMSET(start, 0, diff);
    }

    return ptr;
}

/*=============================================================================
 * Tests if entity is active (created)
 *============================================================================*/
static inline bool ecs_is_active(ecs_t* ecs, ecs_id_t entity_id)
{
    ECS_ASSERT(ecs_is_not_null(ecs));
    return ecs->entities[entity_id].active;
}

/*=============================================================================
 * Command queue implementation
 *============================================================================*/
static void ecs_cmd_array_init(ecs_t* ecs, ecs_cmd_array_t* queue, size_t capacity)
{
    ECS_ASSERT(ecs_is_not_null(ecs));
    ECS_ASSERT(ecs_is_not_null(queue));
    ECS_ASSERT(capacity > 0);

    (void)ecs;

    queue->size     = 0;
    queue->capacity = capacity;
    queue->data     = (ecs_cmd_t*)ECS_MALLOC(capacity * sizeof(ecs_cmd_t), ecs->mem_ctx);

    ECS_ASSERT(ecs_is_not_null(queue->data));
}

static void ecs_cmd_array_free(ecs_t* ecs, ecs_cmd_array_t* queue)
{
    ECS_ASSERT(ecs_is_not_null(ecs));
    ECS_ASSERT(ecs_is_not_null(queue));
    ECS_FREE(queue->data, ecs->mem_ctx);
    (void)ecs;
    (void)queue;
}

static ecs_cmd_t* ecs_cmd_array_push(ecs_t* ecs, ecs_cmd_array_t* queue)
{
    ECS_ASSERT(ecs_is_not_null(ecs));
    ECS_ASSERT(ecs_is_not_null(queue));

    (void)ecs;

    if (queue->size == queue->capacity)
    {
        size_t new_capacity = queue->capacity * 2;

        ECS_ASSERT(ecs_is_valid_capacity(new_capacity, sizeof(ecs_cmd_t)));
        queue->data = (ecs_cmd_t*)ECS_REALLOC(queue->data,
                                              new_capacity * sizeof(ecs_cmd_t),
                                              ecs->mem_ctx);
        queue->capacity = new_capacity;
    }

    ecs_cmd_t* cmd = &queue->data[queue->size++];
    ECS_MEMSET(cmd, 0, sizeof(ecs_cmd_t));
    return cmd;
}

static void ecs_cmd_flush_queue(ecs_t* ecs)
{
    ECS_ASSERT(ecs_is_not_null(ecs));

    ecs_cmd_array_t* queue = &ecs->cmd_queue;

    for (size_t i = 0; i < queue->size; ++i)
    {
        ecs_cmd_t* cmd = &queue->data[i];

        switch (cmd->type)
        {
            case ECS_CMD_SET:
                if (ecs_is_ready(ecs, cmd->entity))
                {
                    ecs_set(ecs, cmd->entity, cmd->comp, cmd->data);
                }
                break;

            case ECS_CMD_ADD:
                if (ecs_is_ready(ecs, cmd->entity))
                {
                    ecs_bitset_flip(&ecs->entities[cmd->entity.id].comp_bits, cmd->comp.id, false);
                    ecs_add(ecs, cmd->entity, cmd->comp, cmd->args);
                }
                break;

            case ECS_CMD_REMOVE:
                if (ecs_is_ready(ecs, cmd->entity))
                {
                    ecs_bitset_flip(&ecs->entities[cmd->entity.id].comp_bits, cmd->comp.id, true);
                    ecs_remove(ecs, cmd->entity, cmd->comp);
                }
                break;

            case ECS_CMD_DESTROY:
                if (ecs_is_active(ecs, cmd->entity.id))
                {
                    ecs->entities[cmd->entity.id].ready = true;
                    ecs_destroy(ecs, cmd->entity);
                }
                break;
        }
    }

    queue->size = 0;
}

/*=============================================================================
 * Bitset functions
 *============================================================================*/

#if ECS_MAX_COMPONENTS <= 64

static inline bool ecs_bitset_is_zero(ecs_bitset_t* set)
{
    return *set == 0;
}

static inline void ecs_bitset_flip(ecs_bitset_t* set, int bit, bool on)
{
    if (on)
        *set |=  ((uint64_t)1 << bit);
    else
        *set &= ~((uint64_t)1 << bit);
}

static inline bool ecs_bitset_test(ecs_bitset_t* set, int bit)
{
    return *set & ((uint64_t)1 << bit);
}

static inline ecs_bitset_t ecs_bitset_and(ecs_bitset_t* set1, ecs_bitset_t* set2)
{
    return *set1 & *set2;
}

static inline ecs_bitset_t ecs_bitset_or(ecs_bitset_t* set1, ecs_bitset_t* set2)
{
    return *set1 | *set2;
}

static inline ecs_bitset_t ecs_bitset_not(ecs_bitset_t* set)
{
    return ~(*set);
}

static inline bool ecs_bitset_equal(ecs_bitset_t* set1, ecs_bitset_t* set2)
{
    return *set1 == *set2;
}

static inline bool ecs_bitset_true(ecs_bitset_t* set)
{
    return *set;
}

#else // ECS_MAX_COMPONENTS

static inline bool ecs_bitset_is_zero(ecs_bitset_t* set)
{
    for (int i = 0; i < ECS_BITSET_SIZE; i++)
    {
        if (set->array[i] != 0)
            return false;
    }

    return true;
}

static inline void ecs_bitset_flip(ecs_bitset_t* set, int bit, bool on)
{
    int index = bit / ECS_BITSET_WIDTH;

    if (on)
        set->array[index] |=  ((uint64_t)1 << bit % ECS_BITSET_WIDTH);
    else
        set->array[index] &= ~((uint64_t)1 << bit % ECS_BITSET_WIDTH);
}

static inline bool ecs_bitset_test(ecs_bitset_t* set, int bit)
{
    int index = bit / ECS_BITSET_WIDTH;
    return set->array[index] & ((uint64_t)1 << bit % ECS_BITSET_WIDTH);
}

static inline ecs_bitset_t ecs_bitset_and(ecs_bitset_t* set1,
                                          ecs_bitset_t* set2)
{
    ecs_bitset_t set;

    for (int i = 0; i < ECS_BITSET_SIZE; i++)
    {
        set.array[i] = set1->array[i] & set2->array[i];
    }

    return set;
}

static inline ecs_bitset_t ecs_bitset_or(ecs_bitset_t* set1,
                                         ecs_bitset_t* set2)
{
    ecs_bitset_t set;

    for (int i = 0; i < ECS_BITSET_SIZE; i++)
    {
        set.array[i] = set1->array[i] | set2->array[i];
    }

    return set;
}

static inline ecs_bitset_t ecs_bitset_not(ecs_bitset_t* set)
{
    ecs_bitset_t out;

    for (int i = 0; i < ECS_BITSET_SIZE; i++)
    {
        out.array[i] = ~set->array[i];
    }

    return out;
}

static inline bool ecs_bitset_equal(ecs_bitset_t* set1, ecs_bitset_t* set2)
{
    for (int i = 0; i < ECS_BITSET_SIZE; i++)
    {
        if (set1->array[i] != set2->array[i])
        {
            return false;
        }
    }

    return true;
}

static inline bool ecs_bitset_true(ecs_bitset_t* set)
{
    for (int i = 0; i < ECS_BITSET_SIZE; i++)
    {
        if (set->array[i])
            return true;
    }

    return false;
}

#endif // ECS_MAX_COMPONENTS

static ecs_arena_block_t* ecs_arena_block_create(ecs_t* ecs, size_t size)
{
    (void)ecs;

    ecs_arena_block_t* block = (ecs_arena_block_t*)ECS_MALLOC(sizeof(ecs_arena_block_t), ecs->mem_ctx);

    if (!block)
        return NULL;

    block->memory = (uint8_t*)ECS_MALLOC(size, ecs->mem_ctx);

    if (!block->memory)
    {
        ECS_FREE(block, ecs->mem_ctx);
        return NULL;
    }

    block->size   = size;
    block->offset = 0;
    block->next   = NULL;

    return block;
}

static bool ecs_arena_init(ecs_t* ecs, ecs_arena_t* arena, size_t initial_block_size)
{
    ecs_arena_block_t* block = ecs_arena_block_create(ecs, initial_block_size);

    if (!block)
        return false;

    arena->first      = block;
    arena->current    = block;
    arena->block_size = initial_block_size;

    return true;
}

static bool ecs_arena_grow(ecs_t* ecs, ecs_arena_t* arena, size_t min_size)
{
    size_t new_size = arena->block_size;

    while (new_size < min_size)
    {
        new_size *= 2;
    }

    ecs_arena_block_t* block = ecs_arena_block_create(ecs, new_size);

    if (!block)
        return false;

    arena->current->next = block;
    arena->current       = block;

    return true;
}

static uintptr_t ecs_arena_align_forward(uintptr_t ptr, size_t align)
{
    uintptr_t mask = (uintptr_t)align - 1;
    return (ptr + mask) & ~mask;
}

static void* ecs_arena_alloc_align(ecs_t* ecs, ecs_arena_t* arena, size_t size, size_t align)
{
    if ((align & (align - 1)) != 0)
        return NULL; // align must be a power of two

    ecs_arena_block_t* block = arena->current;

    uintptr_t base      = (uintptr_t)block->memory;
    uintptr_t ptr       = base + block->offset;
    uintptr_t aligned   = ecs_arena_align_forward(ptr, align);
    size_t    new_offset = (aligned - base) + size;

    if (new_offset > block->size)
    {
        size_t required = size + align;

        if (!ecs_arena_grow(ecs, arena, required))
            return NULL;

        block      = arena->current;
        base       = (uintptr_t)block->memory;
        aligned    = ecs_arena_align_forward(base, align);
        new_offset = (aligned - base) + size;
    }

    block->offset = new_offset;

    return (void*)aligned;
}

static void* ecs_arena_alloc(ecs_t* ecs, ecs_arena_t* arena, size_t size)
{
    return ecs_arena_alloc_align(ecs, arena, size, alignof(max_align_t));
}

static void ecs_arena_reset(ecs_t* ecs, ecs_arena_t* arena)
{
    (void)ecs;

    ecs_arena_block_t* block = arena->first->next;

    while (block)
    {
        ecs_arena_block_t* next = block->next;
        ECS_FREE(block->memory, ecs->mem_ctx);
        ECS_FREE(block, ecs->mem_ctx);
        block = next;
    }

    arena->first->next   = NULL;
    arena->first->offset = 0;
    arena->current       = arena->first;
}

static void ecs_arena_destroy(ecs_t* ecs, ecs_arena_t* arena)
{
    (void)ecs;

    ecs_arena_block_t* block = arena->first;

    while (block)
    {
        ecs_arena_block_t* next = block->next;
        ECS_FREE(block->memory, ecs->mem_ctx);
        ECS_FREE(block, ecs->mem_ctx);
        block = next;
    }

    arena->first   = NULL;
    arena->current = NULL;
}

/*=============================================================================
 * Sparse set functions
 *============================================================================*/

static void ecs_sparse_set_init(ecs_t* ecs, ecs_sparse_set_t* set, size_t capacity)
{
    ECS_ASSERT(ecs_is_not_null(ecs));
    ECS_ASSERT(ecs_is_not_null(set));

    (void)ecs;

    set->capacity = capacity;
    set->size = 0;

    ECS_ASSERT(ecs_is_valid_capacity(capacity, sizeof(ecs_entity_t)));
    set->dense  = (ecs_entity_t*)ECS_MALLOC(capacity * sizeof(ecs_entity_t), ecs->mem_ctx);

    ECS_ASSERT(ecs_is_valid_capacity(capacity, sizeof(size_t)));
    set->sparse = (size_t*)      ECS_MALLOC(capacity * sizeof(size_t),   ecs->mem_ctx);

    ECS_MEMSET(set->sparse, 0, capacity * sizeof(size_t));
}

static void ecs_sparse_set_free(ecs_t* ecs, ecs_sparse_set_t* set)
{
    ECS_ASSERT(ecs_is_not_null(ecs));
    ECS_ASSERT(ecs_is_not_null(set));

    (void)ecs;

    ECS_FREE(set->dense,  ecs->mem_ctx);
    ECS_FREE(set->sparse, ecs->mem_ctx);
}

static bool ecs_sparse_set_add(ecs_t* ecs, ecs_sparse_set_t* set, ecs_id_t id)
{
    ECS_ASSERT(ecs_is_not_null(ecs));
    ECS_ASSERT(ecs_is_not_null(set));
    ECS_ASSERT(ecs_is_valid_id(id));

    (void)ecs;

    // Check if ID exists within the set
    if (ecs_sparse_set_find(set, id, NULL))
        return false;

    // Grow sparse set if necessary
    if (id >= set->capacity)
    {
        size_t old_capacity = set->capacity;
        size_t new_capacity = old_capacity;

        // Note that since a valid id doesn't have its high bit set, and
        // capacity is in terms of elements, doubling the capacity won't wrap
        do {
            new_capacity *= 2;
        } while (id >= new_capacity);


        // Grow dense array
        ECS_ASSERT(ecs_is_valid_capacity(set->capacity, sizeof(ecs_entity_t)));
        set->dense = (ecs_entity_t*)ecs_realloc_zero(ecs,
                                                set->dense,
                                                old_capacity * sizeof(ecs_entity_t),
                                                new_capacity * sizeof(ecs_entity_t));


        // Grow sparse array and zero it
        ECS_ASSERT(ecs_is_valid_capacity(set->capacity, sizeof(size_t)));
        set->sparse = (size_t*)ecs_realloc_zero(ecs,
                                                set->sparse,
                                                old_capacity * sizeof(size_t),
                                                new_capacity * sizeof(size_t));

        // Set the new capacity
        set->capacity = new_capacity;
    }

    // Add ID to set
    set->dense[set->size].id = id;
    set->sparse[id] = set->size;
    set->size++;

    return true;
}

static inline bool ecs_sparse_set_find(ecs_sparse_set_t* set, ecs_id_t id, size_t* found)
{
    ECS_ASSERT(ecs_is_not_null(set));
    ECS_ASSERT(ecs_is_valid_id(id));

    if (id < set->capacity && set->sparse[id] < set->size && set->dense[set->sparse[id]].id == id)
    {
        if (found) *found = set->sparse[id];
        return true;
    }
    else
    {
        if (found) *found = 0;
        return false;
    }
}

static inline bool ecs_sparse_set_remove(ecs_sparse_set_t* set, ecs_id_t id)
{
    ECS_ASSERT(ecs_is_not_null(set));
    ECS_ASSERT(ecs_is_valid_id(id));

    if (!ecs_sparse_set_find(set, id, NULL))
        return false;

    // Swap and remove (changes order of array)
    ecs_id_t tmp = set->dense[set->size - 1].id;
    set->dense[set->sparse[id]].id = tmp;
    set->sparse[tmp] = set->sparse[id];

    set->size--;

    return true;
}

/*=============================================================================
 * System entity add/remove functions
 *============================================================================*/
#if ECS_MAX_COMPONENTS <= 64

static inline bool ecs_entity_system_test(ecs_bitset_t require_bits,
                                          ecs_bitset_t exclude_bits,
                                          ecs_bitset_t entity_bits)
{
    if (entity_bits & exclude_bits)
        return false;

    if ((entity_bits & require_bits) != require_bits)
        return false;

    return true;
}

#else // ECS_MAX_COMPONENTS

static inline bool ecs_entity_system_test(ecs_bitset_t require_bits,
                                          ecs_bitset_t exclude_bits,
                                          ecs_bitset_t entity_bits)
{
    if (!ecs_bitset_is_zero(&exclude_bits))
    {
        ecs_bitset_t overlap = ecs_bitset_and(&entity_bits, &exclude_bits);

        if (ecs_bitset_true(&overlap))
        {
            return false;
        }
    }

    ecs_bitset_t entity_and_require = ecs_bitset_and(&entity_bits, &require_bits);
    return ecs_bitset_equal(&entity_and_require, &require_bits);
}
#endif // ECS_MAX_COMPONENTS

static void ecs_sync_add_remove(ecs_t* ecs, ecs_id_t entity_id, ecs_id_t comp_id)
{
    // Load entity data
    ecs_entity_data_t* entity_data = &ecs->entities[entity_id];

    // Add or remove entity from systems
    for (ecs_id_t sys_id = 0; sys_id < ecs->system_count; sys_id++)
    {
        ecs_sys_data_t* sys_data = &ecs->systems[sys_id];

        // Skip systems that don't reference the changed component --
        // their match result cannot have changed
        if (!ecs_bitset_test(&sys_data->require_bits, comp_id) &&
            !ecs_bitset_test(&sys_data->exclude_bits, comp_id))
            continue;

        // Test to see if entity's components matches the system
        if (ecs_entity_system_test(sys_data->require_bits,
                                   sys_data->exclude_bits,
                                   entity_data->comp_bits))
        {
            // Add the entity directly to the sparse set
            if (ecs_sparse_set_add(ecs, &sys_data->entity_ids, entity_id))
            {
                if (sys_data->on_join)
                    sys_data->on_join(ecs, ecs_make_entity(entity_id), sys_data->udata);
            }
        }
        else
        {
            // Just remove the entity from the sparse set if its components
            // no longer match
            if (ecs_sparse_set_remove(&sys_data->entity_ids, entity_id))
            {
                if (sys_data->on_leave)
                    sys_data->on_leave(ecs, ecs_make_entity(entity_id), sys_data->udata);
            }
        }
    }
}

/*=============================================================================
 * Owned local functions
 *============================================================================*/

static ecs_owned_local_t* ecs_owned_local_get(ecs_t* ecs)
{
    if (ecs_tl_local_generation == ecs->generation)
        return ecs_tl_local;

    ecs_owned_local_t* local = (ecs_owned_local_t*)ECS_MALLOC(sizeof(ecs_owned_local_t),
                                                              ecs->mem_ctx);

    ECS_MEMSET(local, 0, sizeof(ecs_owned_local_t));

    ECS_ATOMIC_INIT(&local->count, 0);

    //TODO: growing not implemented yet
    local->capacity = ecs->entity_capacity;

    ECS_ASSERT(ecs_is_valid_capacity(local->capacity, sizeof(ecs_entity_t)));
    local->entities = (ecs_entity_t*)ECS_MALLOC(local->capacity * sizeof(ecs_entity_t),
                                                ecs->mem_ctx);

    ECS_MTX_LOCK(&ecs->lock);

    size_t index = ECS_ATOMIC_LOAD_ACQUIRE(&ecs->owned_local_count);

    ECS_ASSERT(index < ECS_MAX_OWNED_LOCALS);

    ecs->owned_locals[index] = local;
    ECS_ATOMIC_STORE_RELEASE(&ecs->owned_local_count, index + 1);

    ECS_MTX_UNLOCK(&ecs->lock);

    ecs_tl_local            = local;
    ecs_tl_local_generation = ecs->generation;

    return local;
}

static void ecs_owned_local_publish(ecs_t* ecs, ecs_owned_local_t* local)
{
    if (0 == local->size)
        return;

    ecs_bitset_t comp_bits = ecs->entities[local->entities[local->size - 1].id].comp_bits;

    if (!local->comp_bits_set)
    {
        local->comp_bits = comp_bits;
        local->comp_bits_set  = true;
    }
    else
    {
        ECS_ASSERT(ecs_bitset_equal(&local->comp_bits, &comp_bits));
    }

    ECS_ATOMIC_STORE_RELEASE(&local->count, local->size);
}

static void ecs_owned_local_publish_tail(ecs_t* ecs)
{
    if (NULL != ecs_tl_local && ecs_tl_local_generation == ecs->generation)
        ecs_owned_local_publish(ecs, ecs_tl_local);
}

static void ecs_owned_local_free_all(ecs_t* ecs)
{
    size_t local_count = ECS_ATOMIC_LOAD_ACQUIRE(&ecs->owned_local_count);

    for (size_t i = 0; i < local_count; i++)
    {
        ecs_owned_local_t* local = ecs->owned_locals[i];

        ECS_ATOMIC_DESTROY(&local->count);
        ECS_FREE(local->entities, ecs->mem_ctx);
        ECS_FREE(local, ecs->mem_ctx);

        ecs->owned_locals[i] = NULL;
    }

    ECS_ATOMIC_STORE(&ecs->owned_local_count, 0);
}

static bool ecs_owned_local_matches(ecs_sys_data_t* sys_data, ecs_bitset_t comp_bits)
{
    ecs_bitset_t referenced = ecs_bitset_or(&sys_data->require_bits, &sys_data->exclude_bits);
    ecs_bitset_t overlap = ecs_bitset_and(&referenced, &comp_bits);

    if (!ecs_bitset_true(&overlap))
        return false;

    return ecs_entity_system_test(sys_data->require_bits,
                                  sys_data->exclude_bits,
                                  comp_bits);
}

// Runs the system over the published part of every local store it matches
// TODO: Maybe sideeffect for running system multiple times for diffrent set of entites
static ecs_ret_t ecs_run_owned_locals(ecs_t* ecs, ecs_sys_data_t* sys_data)
{
    size_t local_count = ECS_ATOMIC_LOAD_ACQUIRE(&ecs->owned_local_count);

    for (size_t i = 0; i < local_count; i++)
    {
        ecs_owned_local_t* local = ecs->owned_locals[i];

        size_t count = ECS_ATOMIC_LOAD_ACQUIRE(&local->count);

        if (0 == count)
            continue;

        if (!ecs_owned_local_matches(sys_data, local->comp_bits))
            continue;

        ecs_ret_t code = sys_data->system_cb(ecs, local->entities, count, sys_data->udata);

        if (0 != code)
            return code;
    }

    return 0;
}

static void ecs_sync_destroy(ecs_t* ecs, ecs_id_t entity_id)
{
    // Remove entity from systems
    for (ecs_id_t sys_id = 0; sys_id < ecs->system_count; sys_id++)
    {
        ecs_sys_data_t* sys_data = &ecs->systems[sys_id];

        if (ecs_sparse_set_remove(&sys_data->entity_ids, entity_id))
        {
            if (sys_data->on_leave)
                sys_data->on_leave(ecs, ecs_make_entity(entity_id), sys_data->udata);
        }
    }
}

/*=============================================================================
 * ID array functions
 *============================================================================*/

static void ecs_id_array_init(ecs_t* ecs, ecs_id_array_t* array, size_t capacity)
{
    ECS_ASSERT(ecs_is_not_null(ecs));
    ECS_ASSERT(ecs_is_not_null(array));

    (void)ecs;

    array->size = 0;
    array->capacity = capacity;

    ECS_ASSERT(ecs_is_valid_capacity(capacity, sizeof(ecs_id_t)));
    array->data = (ecs_id_t*)ECS_MALLOC(capacity * sizeof(ecs_id_t), ecs->mem_ctx);
}

static void ecs_id_array_free(ecs_t* ecs, ecs_id_array_t* array)
{
    ECS_ASSERT(ecs_is_not_null(ecs));
    ECS_ASSERT(ecs_is_not_null(array));

    (void)ecs;

    ECS_FREE(array->data, ecs->mem_ctx);
}

static inline void ecs_id_array_push(ecs_t* ecs, ecs_id_array_t* array, ecs_id_t id)
{
    ECS_ASSERT(ecs_is_not_null(ecs));
    ECS_ASSERT(ecs_is_not_null(array));
    ECS_ASSERT(ecs_is_valid_id(id));

    (void)ecs;

    if (array->size == array->capacity)
    {

        // Note that since a valid id doesn't have its high bit set, and
        // capacity is in terms of elements, doubling the capacity won't wrap
        array->capacity *= 2;

        ECS_ASSERT(ecs_is_valid_capacity(array->capacity, sizeof(ecs_id_t)));
        array->data = (ecs_id_t*)ECS_REALLOC(array->data,
                                             array->capacity * sizeof(ecs_id_t),
                                             ecs->mem_ctx);
    }

    array->data[array->size++] = id;
}

static inline ecs_id_t ecs_id_array_pop(ecs_id_array_t* array)
{
    ECS_ASSERT(ecs_is_not_null(array));
    ECS_ASSERT(array->size > 0);

    return array->data[--array->size];
}

static inline size_t ecs_id_array_size(ecs_id_array_t* array)
{
    return array->size;
}

static void ecs_comp_blocks_init(ecs_t* ecs, ecs_comp_blocks_t* array, size_t size, size_t capacity)
{
    ECS_ASSERT(ecs_is_not_null(ecs));
    ECS_ASSERT(ecs_is_not_null(array));

    (void)ecs;

    ECS_MEMSET(array, 0, sizeof(ecs_comp_blocks_t));

    array->comp_size = size;

    size_t initial_blocks = (capacity + ECS_COMP_BLOCK_SIZE - 1) / ECS_COMP_BLOCK_SIZE;
    if (initial_blocks == 0) initial_blocks = 1;

    array->block_capacity = initial_blocks;
    array->block_count    = initial_blocks;

    ECS_ASSERT(ecs_is_valid_capacity(initial_blocks, sizeof(void*)));
    array->blocks = (void**)ECS_MALLOC(initial_blocks * sizeof(void*), ecs->mem_ctx);

    for (size_t i = 0; i < initial_blocks; i++)
    {
        ECS_ASSERT(ecs_is_valid_capacity(ECS_COMP_BLOCK_SIZE, size));
        array->blocks[i] = ECS_MALLOC(ECS_COMP_BLOCK_SIZE * size, ecs->mem_ctx);
        ECS_MEMSET(array->blocks[i], 0, ECS_COMP_BLOCK_SIZE * size);
    }
}

static void ecs_comp_blocks_free(ecs_t* ecs, ecs_comp_blocks_t* array)
{
    ECS_ASSERT(ecs_is_not_null(ecs));
    ECS_ASSERT(ecs_is_not_null(array));

    (void)ecs;

    for (size_t i = 0; i < array->block_count; i++)
    {
        ECS_FREE(array->blocks[i], ecs->mem_ctx);
    }

    ECS_FREE(array->blocks, ecs->mem_ctx);
}

static void ecs_comp_blocks_resize(ecs_t* ecs, ecs_comp_blocks_t* array, ecs_id_t id)
{
    ECS_ASSERT(ecs_is_not_null(ecs));
    ECS_ASSERT(ecs_is_not_null(array));
    ECS_ASSERT(ecs_is_valid_id(id));

    size_t required_block = id / ECS_COMP_BLOCK_SIZE;

    while (required_block >= array->block_count)
    {
        // Grow the block pointer array if necessary. This only moves pointers,
        // never the block data itself, so existing component pointers remain valid.
        if (array->block_count == array->block_capacity)
        {
            size_t old_capacity = array->block_capacity;
            size_t new_capacity = old_capacity * 2;

            ECS_ASSERT(ecs_is_valid_capacity(array->block_capacity, sizeof(void*)));
            array->blocks = (void**)ecs_realloc_zero(ecs,
                                                     array->blocks,
                                                     old_capacity * sizeof(void*),
                                                     new_capacity * sizeof(void*));


            array->block_capacity = new_capacity;
        }

        // Allocate and zero a new block
        ECS_ASSERT(ecs_is_valid_capacity(ECS_COMP_BLOCK_SIZE, array->comp_size));
        void* block = ECS_MALLOC(ECS_COMP_BLOCK_SIZE * array->comp_size, ecs->mem_ctx);
        ECS_MEMSET(block, 0, ECS_COMP_BLOCK_SIZE * array->comp_size);
        array->blocks[array->block_count++] = block;
    }
}

/*=============================================================================
 * Validation functions
 *============================================================================*/
#ifndef NDEBUG
static bool ecs_is_not_null(void* ptr)
{
    return NULL != ptr;
}

static bool ecs_is_valid_component_id(ecs_id_t id)
{
    return id < ECS_MAX_COMPONENTS;
}

static bool ecs_is_valid_system_id(ecs_id_t id)
{
    return id < ECS_MAX_SYSTEMS;
}

static bool ecs_is_valid_id(ecs_id_t id)
{
    // Ensures high bit is not set - works for any unsigned ecs_id_t
    return id == ((id << 1) >> 1);
}

static bool ecs_is_valid_capacity(size_t capacity, size_t elem_size)
{
    // Ensures any array allocations won't overflow a signed size_t and are
    // nonzero. This is not the most efficient implementation, but it is simple

    if (capacity == 0 || elem_size == 0)
    {
        return false;
    }

    size_t max_cap = (SIZE_MAX >> 1) / elem_size;
    return capacity <= max_cap;
}

static bool ecs_is_entity_ready(ecs_t* ecs, ecs_id_t entity_id)
{
    return ecs->entities[entity_id].ready;
}

static bool ecs_is_component_ready(ecs_t* ecs, ecs_id_t comp_id)
{
    return comp_id < ecs->comp_count;
}

static bool ecs_is_system_ready(ecs_t* ecs, ecs_id_t sys_id)
{
    return sys_id < ecs->system_count;
}

static bool ecs_is_not_pending(ecs_t* ecs, ecs_id_t entity_id)
{
    return !ecs->entities[entity_id].pending;
}

#endif // NDEBUG

#endif // PITO_ECS_IMPLEMENTATION

/*
    ----------------------------------------------------------------------------
    This software is available under two licenses (A) or (B). You may choose
    either one as you wish:
    ----------------------------------------------------------------------------

    (A) The zlib License

    Copyright (c) 2025 James McLean

    This software is provided 'as-is', without any express or implied warranty.
    In no event will the authors be held liable for any damages arising from the
    use of this software.

    Permission is granted to anyone to use this software for any purpose,
    including commercial applications, and to alter it and redistribute it
    freely, subject to the following restrictions:

    1. The origin of this software must not be misrepresented; you must not
    claim that you wrote the original software. If you use this software in a
    product, an acknowledgment in the product documentation would be appreciated
    but is not required.

    2. Altered source versions must be plainly marked as such, and must not be
    misrepresented as being the original software.

    3. This notice may not be removed or altered from any source distribution.

    ----------------------------------------------------------------------------

    (B) Public Domain (www.unlicense.org)

    This is free and unencumbered software released into the public domain.

    Anyone is free to copy, modify, publish, use, compile, sell, or distribute
    this software, either in source code form or as a compiled binary, for any
    purpose, commercial or non-commercial, and by any means.

    In jurisdictions that recognize copyright laws, the author or authors of
    this software dedicate any and all copyright interest in the software to the
    public domain. We make this dedication for the benefit of the public at
    large and to the detriment of our heirs and successors. We intend this
    dedication to be an overt act of relinquishment in perpetuity of all present
    and future rights to this software under copyright law.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
    AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
    ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
    WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/

// EoF
