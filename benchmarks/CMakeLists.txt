cmake_minimum_required(VERSION 3.16)
project(pito_ecs_benchmarks C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

find_package(Threads REQUIRED)

function(download url dest)
    if(NOT EXISTS ${dest})
        file(DOWNLOAD ${url} ${dest} STATUS status)
        list(GET status 0 code)
        if(NOT code EQUAL 0)
            file(REMOVE ${dest})
            message(FATAL_ERROR "Downloading ${url} failed: ${status}")
        endif()
    endif()
endfunction()

set(PICO_COMMIT 2db8b9be4ec5c0b3e135e2895f9d7eaa01e4e10e)
set(PICO_DIR ${CMAKE_BINARY_DIR}/pico)
download(https://raw.githubusercontent.com/empyreanx/pico_headers/${PICO_COMMIT}/pico_ecs.h
         ${PICO_DIR}/pico_ecs.h)

set(FLECS_VERSION v4.1.6)
set(FLECS_DIR ${CMAKE_BINARY_DIR}/flecs)
download(https://raw.githubusercontent.com/SanderMertens/flecs/${FLECS_VERSION}/distr/flecs.h
         ${FLECS_DIR}/flecs.h)
download(https://raw.githubusercontent.com/SanderMertens/flecs/${FLECS_VERSION}/distr/flecs.c
         ${FLECS_DIR}/flecs.c)

add_library(flecs STATIC ${FLECS_DIR}/flecs.c)
target_include_directories(flecs PUBLIC ${FLECS_DIR})
target_link_libraries(flecs PUBLIC Threads::Threads)

string(TOUPPER ${CMAKE_BUILD_TYPE} BUILD_TYPE)
set(BENCH_FLAGS "${CMAKE_C_FLAGS} ${CMAKE_C_FLAGS_${BUILD_TYPE}}")

file(MAKE_DIRECTORY ${CMAKE_SOURCE_DIR}/results)

function(add_bench_executable target source flags)
    add_executable(${target} ${source})
    target_compile_definitions(${target} PRIVATE "BENCH_FLAGS=\"${flags}\""
                                                 "BENCH_RESULTS_DIR=\"${CMAKE_SOURCE_DIR}/results\"")
    target_link_libraries(${target} PRIVATE Threads::Threads m)
endfunction()

function(add_bench name)
    add_bench_executable(bench_${name}_pico owned_${name}.c "${BENCH_FLAGS}")
    target_compile_definitions(bench_${name}_pico PRIVATE BENCH_PICO)
    target_include_directories(bench_${name}_pico PRIVATE ${PICO_DIR})

    add_bench_executable(bench_${name}_flecs flecs_${name}.c "${BENCH_FLAGS}")
    target_link_libraries(bench_${name}_flecs PRIVATE flecs)

    add_bench_executable(bench_${name}_pito owned_${name}.c "${BENCH_FLAGS}")
    target_include_directories(bench_${name}_pito PRIVATE ${CMAKE_SOURCE_DIR}/..)

    add_bench_executable(bench_${name}_pito_atomic owned_${name}.c
                         "${BENCH_FLAGS} -DPITO_ECS_OWNED_DELETE_ATOMIC=1")
    target_compile_definitions(bench_${name}_pito_atomic PRIVATE PITO_ECS_OWNED_DELETE_ATOMIC=1)
    target_include_directories(bench_${name}_pito_atomic PRIVATE ${CMAKE_SOURCE_DIR}/..)

    add_custom_target(run_${name}
                      COMMAND sh ${CMAKE_SOURCE_DIR}/run.sh --types ${name}
                      WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
                      DEPENDS bench_${name}_pico bench_${name}_flecs bench_${name}_pito
                              bench_${name}_pito_atomic
                      USES_TERMINAL)
    list(APPEND RUN_ALL_DEPS bench_${name}_pico bench_${name}_flecs bench_${name}_pito
                             bench_${name}_pito_atomic)
    set(RUN_ALL_DEPS ${RUN_ALL_DEPS} PARENT_SCOPE)
endfunction()

add_bench(update)
add_bench(insert)
add_bench(delete)
add_bench(initialize)
add_bench(local)
add_bench(shared)

add_custom_target(run_all
                  COMMAND sh ${CMAKE_SOURCE_DIR}/run.sh
                  WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
                  DEPENDS ${RUN_ALL_DEPS}
                  USES_TERMINAL)
