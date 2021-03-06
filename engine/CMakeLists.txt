project( engine )

file( GLOB_RECURSE SOURCE *.cpp *.hpp *.c )
file( GLOB_RECURSE HEADERS *.h )

add_library( engine
    config.cpp
    config.h
    core.cpp
    core.h
    game.h
    input.cpp
    input.h
    light_manager.cpp
    light_manager.h
    random.h
    scene.cpp
    scene.h
    types.h
    worker_pool.cpp
    worker_pool.h
    worker_thread.cpp
    worker_thread.h
    work_item.cpp
    work_item.h
)

target_include_directories(engine PUBLIC "../" )
target_include_directories(engine PUBLIC "../external/vma" )

target_link_libraries(engine $<$<PLATFORM_ID:Linux>:-ldl>)
target_link_libraries(engine Vulkan::Vulkan SDL::SDL2 GLM::GLM)

add_subdirectory(entity)
add_subdirectory(renderer)
add_subdirectory(task)
