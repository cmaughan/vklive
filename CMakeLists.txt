cmake_minimum_required(VERSION 3.15)

set(CMAKE_POLICY_DEFAULT_CMP0077 NEW)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

set(CMAKE_DEBUG_POSTFIX "-debug")
set(CMAKE_RELWITHDEBINFO_POSTFIX "-reldbg")
set_directory_properties(PROPERTIES COMPILE_DEFINITIONS_DEBUG "_DEBUG")

list(INSERT CMAKE_MODULE_PATH 0 ${CMAKE_CURRENT_LIST_DIR}/cmake)
include(${CMAKE_CURRENT_LIST_DIR}/cmake/utils.cmake)

if (WIN32)
    set(VCPKG_TARGET_TRIPLET x64-windows-static-md)
endif()

set(CMAKE_TOOLCHAIN_FILE ${CMAKE_CURRENT_SOURCE_DIR}/vcpkg/scripts/buildsystems/vcpkg.cmake CACHE STRING "Vcpkg toolchain file")

set(APP_NAME ${PROJECT_NAME})
set(REZONALITY_ROOT ${CMAKE_CURRENT_LIST_DIR})

project(Rezonality VERSION 0.1.0.0)

if(APPLE)
    option(VKLIVE_ENABLE_METAL "Build the Metal renderer" ON)
    option(VKLIVE_ENABLE_VULKAN "Build the Vulkan renderer" OFF)
else()
    option(VKLIVE_ENABLE_METAL "Build the Metal renderer" OFF)
    option(VKLIVE_ENABLE_VULKAN "Build the Vulkan renderer" ON)
endif()

if(VKLIVE_ENABLE_METAL AND NOT APPLE)
    message(FATAL_ERROR "VKLIVE_ENABLE_METAL is only supported on Apple platforms")
endif()

if(NOT VKLIVE_ENABLE_METAL AND NOT VKLIVE_ENABLE_VULKAN)
    message(FATAL_ERROR "At least one renderer backend must be enabled")
endif()

# Set additional project information
set(COMPANY "Rezonality")
set(COPYRIGHT "Copyright (c) 2022 Rezonality")
set(IDENTIFIER "com.rezonality.vklive")

find_package(unofficial-minizip CONFIG REQUIRED)
if(VKLIVE_ENABLE_VULKAN)
    find_package(Vulkan REQUIRED) # Vulkan API
endif()
find_package(assimp CONFIG REQUIRED) # Asset import for loading models
find_package(clipp CONFIG REQUIRED) # Commandline parser for command line arguments
find_package(fmt CONFIG REQUIRED) # String formatting library
find_package(reproc++ REQUIRED) # Process launch library for loading shader compiler binary
find_package(SDL2 CONFIG REQUIRED) # SDL UI library for creating windows, etc.
find_package(unofficial-concurrentqueue REQUIRED) # Concurrent lock/free queue for thread interactions
find_package(tinyfiledialogs CONFIG REQUIRED) # File dialogs that look OS specific and more standard
find_package(nfd CONFIG REQUIRED) # File dialogs that look OS specific and more standard
find_package(unofficial-spirv-reflect CONFIG REQUIRED)
#find_package(VulkanMemoryAllocator CONFIG REQUIRED)
find_package(gli CONFIG REQUIRED)
find_path(STB_INCLUDE_DIRS "stb_c_lexer.h")
find_package(range-v3 CONFIG REQUIRED)
find_path(TSL_ORDERED_MAP_INCLUDE_DIRS "tsl/ordered_hash.h")
find_package(lodepng CONFIG REQUIRED)

if(VKLIVE_ENABLE_METAL)
    enable_language(OBJCXX)
    set(CMAKE_OBJCXX_STANDARD 20)
    set(CMAKE_OBJCXX_STANDARD_REQUIRED ON)
    find_package(spirv_cross_core CONFIG REQUIRED)
    find_package(spirv_cross_glsl CONFIG REQUIRED)
    find_package(spirv_cross_msl CONFIG REQUIRED)
    find_package(spirv_cross_cpp CONFIG REQUIRED)
    find_library(METAL_FRAMEWORK Metal REQUIRED)
    find_library(METALKIT_FRAMEWORK MetalKit REQUIRED)
    find_library(QUARTZCORE_FRAMEWORK QuartzCore REQUIRED)
    find_library(FOUNDATION_FRAMEWORK Foundation REQUIRED)
    find_library(COCOA_FRAMEWORK Cocoa REQUIRED)
endif()

# Set this if we are sitting on SDL
add_definitions(-DZEP_USE_SDL -DGLM_ENABLE_EXPERIMENTAL)

# Vulkan can use dynamic dispatch extensions
if(VKLIVE_ENABLE_VULKAN)
    add_definitions(-DVULKAN_HPP_DISPATCH_LOADER_DYNAMIC=1)
endif()

# Set this to get the default file system
add_definitions(-DZEP_FEATURE_CPP_FILE_SYSTEM)
if(MSVC)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /MP")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /MP")
else()
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -D_DEBUG -DDEBUG")
    set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -D_DEBUG -DDEBUG" )
endif()

set(ZING_LIBRARY_ONLY ON)
set(ZEST_BUILD_TESTS OFF)
add_definitions(-DIMGUI_DISABLE_INCLUDE_IMCONFIG_H)

if(NOT VKLIVE_ENABLE_VULKAN)
    set(_vklive_saved_cmake_module_path "${CMAKE_MODULE_PATH}")
    list(PREPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/cmake/no-vulkan")
endif()

add_subdirectory(libs/zing)

if(NOT VKLIVE_ENABLE_VULKAN)
    set(CMAKE_MODULE_PATH "${_vklive_saved_cmake_module_path}")
    unset(_vklive_saved_cmake_module_path)

    if(TARGET Zest)
        get_target_property(_vklive_zest_sources Zest SOURCES)
        if(_vklive_zest_sources)
            list(FILTER _vklive_zest_sources EXCLUDE REGEX "imgui_impl_vulkan\\.(cpp|h)$")
            set_property(TARGET Zest PROPERTY SOURCES ${_vklive_zest_sources})
        endif()

        foreach(_vklive_link_property LINK_LIBRARIES INTERFACE_LINK_LIBRARIES)
            get_target_property(_vklive_zest_link_libraries Zest ${_vklive_link_property})
            if(_vklive_zest_link_libraries)
                list(REMOVE_ITEM _vklive_zest_link_libraries Vulkan::Vulkan)
                set_property(TARGET Zest PROPERTY ${_vklive_link_property} ${_vklive_zest_link_libraries})
            endif()
        endforeach()

        unset(_vklive_zest_sources)
        unset(_vklive_link_property)
        unset(_vklive_zest_link_libraries)
    endif()
endif()

# Set this to use a lib instead of the single header build
set (ZEP_LIB 1)
if (DEFINED ZEP_LIB)
    add_subdirectory(zep/src)
else()
    add_definitions(-DZEP_SINGLE_HEADER=1)
endif()

set(VKLIVE_ROOT ${CMAKE_CURRENT_LIST_DIR})
configure_file(${CMAKE_CURRENT_LIST_DIR}/cmake/config_app.h.cmake ${CMAKE_BINARY_DIR}/config_app.h)

set(VULKAN_SOURCES
    src/vulkan/vulkan_bindings.cpp
    src/vulkan/vulkan_buffer.cpp
    src/vulkan/vulkan_command.cpp
    src/vulkan/vulkan_context.cpp
    src/vulkan/vulkan_debug.cpp
    src/vulkan/vulkan_descriptor.cpp
    src/vulkan/vulkan_device.cpp
    src/vulkan/vulkan_imgui.cpp
    src/vulkan/vulkan_nanovg.cpp
    src/vulkan/vulkan_imgui_viewports.cpp
    src/vulkan/vulkan_imgui_texture.cpp
    src/vulkan/vulkan_model.cpp
    src/vulkan/vulkan_model_as.cpp
    src/vulkan/vulkan_pass.cpp
    src/vulkan/vulkan_pipeline.cpp
    src/vulkan/vulkan_reflect.cpp
    src/vulkan/vulkan_render.cpp
    src/vulkan/vulkan_scene.cpp
    src/vulkan/vulkan_shader.cpp
    src/vulkan/vulkan_surface.cpp
    src/vulkan/vulkan_uniform.cpp
    src/vulkan/vulkan_utils.cpp
    src/vulkan/vulkan_window.cpp

    include/vklive/vulkan/vulkan_buffer.h
    include/vklive/vulkan/vulkan_command.h
    include/vklive/vulkan/vulkan_context.h
    include/vklive/vulkan/vulkan_debug.h
    include/vklive/vulkan/vulkan_descriptor.h
    include/vklive/vulkan/vulkan_device.h
    include/vklive/vulkan/vulkan_imgui.h
    include/vklive/vulkan/vulkan_nanovg.h
    include/vklive/vulkan/vulkan_imgui_texture.h
    include/vklive/vulkan/vulkan_model.h
    include/vklive/vulkan/vulkan_pass.h
    include/vklive/vulkan/vulkan_pipeline.h
    include/vklive/vulkan/vulkan_reflect.h
    include/vklive/vulkan/vulkan_render.h
    include/vklive/vulkan/vulkan_scene.h
    include/vklive/vulkan/vulkan_shader.h
    include/vklive/vulkan/vulkan_surface.h
    include/vklive/vulkan/vulkan_uniform.h
    include/vklive/vulkan/vulkan_utils.h
    include/vklive/vulkan/vulkan_window.h
    include/vklive/vulkan/nanovg_vk.h
)

set(METAL_OBJCXX_SOURCES
    src/metal/imgui_impl_metal_1917.mm
    src/metal/metal_context.mm
    src/metal/metal_device.mm
    src/metal/metal_imgui.mm
    src/metal/metal_imgui_texture.mm
    src/metal/metal_model.mm
    src/metal/metal_pass.mm
    src/metal/metal_scene.mm
    src/metal/metal_shader.mm
    src/metal/metal_surface.mm
)

set(METAL_SOURCES
    ${METAL_OBJCXX_SOURCES}

    src/metal/imgui_impl_metal_1917.h

    include/vklive/metal/metal_context.h
    include/vklive/metal/metal_device.h
    include/vklive/metal/metal_imgui.h
    include/vklive/metal/metal_imgui_texture.h
    include/vklive/metal/metal_model.h
    include/vklive/metal/metal_pass.h
    include/vklive/metal/metal_scene.h
    include/vklive/metal/metal_shader.h
    include/vklive/metal/metal_surface.h
)

set(EXTERNAL_DIR libs)

set(CLIP_SOURCE
    ${EXTERNAL_DIR}/clip/clip.cpp
    ${EXTERNAL_DIR}/clip/image.cpp
    )


if(WIN32)
    LIST(APPEND CLIP_SOURCE
        ${EXTERNAL_DIR}/clip/clip_win.cpp)
endif()
if (UNIX)
    if (APPLE)
        LIST(APPEND CLIP_SOURCE
            ${EXTERNAL_DIR}/clip/clip_osx.mm)
    else()
        LIST(APPEND CLIP_SOURCE
            ${EXTERNAL_DIR}/clip/clip_x11.cpp)
    endif() # APPLE
endif() # UNIX

set(VK_SOURCES
    src/camera.cpp
    src/device_factory.cpp
    #src/imgui/imgui_utils.cpp
    src/model.cpp
    src/process/process.cpp
    src/scene.cpp
    src/shader_compiler.cpp
    src/validation.cpp
    src/python_scripting.cpp

    libs/mpc/mpc.c

    include/vklive/IDevice.h
    include/vklive/camera.h
    include/vklive/device_factory.h
    include/vklive/model.h
    include/vklive/process/process.h
    include/vklive/scene.h
    include/vklive/shader_bindings.h
    include/vklive/shader_compiler.h
    include/vklive/validation.h
    include/vklive/python_scripting.h
)

set(LIB_SOURCES
    ${VK_SOURCES}
)

if(VKLIVE_ENABLE_VULKAN)
    list(APPEND LIB_SOURCES ${VULKAN_SOURCES})
endif()

if(VKLIVE_ENABLE_METAL)
    list(APPEND LIB_SOURCES ${METAL_SOURCES})
endif()

set(APP_SOURCES
    TODO.md
    README.md
    app/src/command_line.cpp
    app/src/config.cpp
    app/src/controller.cpp
    app/src/editor.cpp
    app/src/main.cpp
    app/src/window_render.cpp
    app/src/window_targets.cpp
    app/src/window_sequencer.cpp
    app/src/menu.cpp
    app/src/project.cpp

    app/include/app/command_line.h
    app/include/app/config.h
    app/include/app/controller.h
    app/include/app/editor.h
    app/include/app/menu.h
    app/include/app/project.h
    app/include/app/window_sequencer.h
    app/include/app/window_render.h
    app/include/app/window_targets.h
)

set_source_files_properties (libs/mpc/mpc.c PROPERTIES SKIP_PRECOMPILE_HEADERS 1)
set_source_files_properties (src/python_scripting.cpp PROPERTIES SKIP_PRECOMPILE_HEADERS 1)
if(VKLIVE_ENABLE_METAL)
    set_source_files_properties(${METAL_OBJCXX_SOURCES} PROPERTIES COMPILE_FLAGS "-fobjc-arc")
endif()


add_library(vklive ${LIB_SOURCES})
target_link_libraries(vklive
    PUBLIC
        reproc++
        assimp::assimp
        fmt::fmt-header-only
        range-v3
        range-v3-meta
        range-v3::meta
        range-v3-concepts
        Zing::Zing
        unofficial::spirv-reflect
        lodepng
    )

if(VKLIVE_ENABLE_VULKAN)
    target_compile_definitions(vklive PUBLIC VKLIVE_ENABLE_VULKAN=1)
    target_link_libraries(vklive PUBLIC Vulkan::Vulkan)
    target_precompile_headers(vklive
      PRIVATE
        include/vklive/vulkan/vulkan_context.h
    )
endif()

if(VKLIVE_ENABLE_METAL)
    target_compile_definitions(vklive PUBLIC VKLIVE_ENABLE_METAL=1)
    target_link_libraries(vklive
        PRIVATE
            spirv-cross-core
            spirv-cross-msl
            spirv-cross-cpp
            ${METAL_FRAMEWORK}
            ${METALKIT_FRAMEWORK}
            ${QUARTZCORE_FRAMEWORK}
            ${FOUNDATION_FRAMEWORK}
            ${COCOA_FRAMEWORK}
    )
endif()

if("${CMAKE_SYSTEM_NAME}" STREQUAL "Linux")
    target_link_libraries(vklive PUBLIC atomic)
endif()

if(WIN32)
# Symbols for release builds on windows
target_compile_options(vklive PRIVATE "$<$<CONFIG:Release>:/Zi>")
target_link_options(vklive PRIVATE "$<$<CONFIG:Release>:/DEBUG>")
target_link_options(vklive PRIVATE "$<$<CONFIG:Release>:/OPT:REF>")
target_link_options(vklive PRIVATE "$<$<CONFIG:Release>:/OPT:ICF>")
endif()

target_include_directories(vklive
    PUBLIC
        ${CMAKE_CURRENT_LIST_DIR}/include
    PRIVATE
        ${CMAKE_BINARY_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/libs
        ${TSL_ORDERED_MAP_INCLUDE_DIRS}
    )

enable_testing()

add_executable(vklive_model_tests tests/model_inspect.cpp)
target_link_libraries(vklive_model_tests PRIVATE vklive)
target_include_directories(vklive_model_tests PRIVATE ${CMAKE_BINARY_DIR})

add_executable(vklive_scene_tests tests/scene_inspect.cpp)
target_link_libraries(vklive_scene_tests PRIVATE vklive unofficial::spirv-reflect)
target_include_directories(vklive_scene_tests PRIVATE ${CMAKE_BINARY_DIR})

add_executable(vklive_app_command_line_tests tests/app_command_line_tests.cpp app/src/command_line.cpp)
target_link_libraries(vklive_app_command_line_tests PRIVATE vklive)
target_include_directories(vklive_app_command_line_tests PRIVATE
    ${CMAKE_BINARY_DIR}
    ${CMAKE_CURRENT_LIST_DIR}/app/include)

add_executable(vklive_render_parity_harness tests/render_parity_harness.cpp)
target_link_libraries(vklive_render_parity_harness PRIVATE reproc++)

add_executable(vklive_render_backend_tests tests/render_backend_tests.cpp)
target_link_libraries(vklive_render_backend_tests PRIVATE vklive)
target_include_directories(vklive_render_backend_tests PRIVATE ${CMAKE_BINARY_DIR})

add_executable(vklive_zep_imgui_input_tests tests/zep_imgui_input_tests.cpp)
target_link_libraries(vklive_zep_imgui_input_tests PRIVATE Zep::Zep Zing::Zing)
target_include_directories(vklive_zep_imgui_input_tests PRIVATE
    ${CMAKE_BINARY_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/zep/include)

if(VKLIVE_ENABLE_METAL)
    add_executable(vklive_metal_binding_tests tests/metal_binding_tests.mm)
    target_link_libraries(vklive_metal_binding_tests
        PRIVATE
            vklive
            ${METAL_FRAMEWORK}
            ${FOUNDATION_FRAMEWORK})
    target_include_directories(vklive_metal_binding_tests PRIVATE ${CMAKE_BINARY_DIR})
endif()

add_test(
    NAME vklive_model_tests_robot
    COMMAND vklive_model_tests ${CMAKE_CURRENT_LIST_DIR}/run_tree/projects/pbr_robot/models/robot/scene.gltf
)

add_test(
    NAME vklive_scene_tests_override
    COMMAND vklive_scene_tests ${CMAKE_CURRENT_LIST_DIR}/run_tree/projects/pbr_robot uv_debug.scenegraph valid
)

add_test(
    NAME vklive_scene_tests_missing_override
    COMMAND vklive_scene_tests ${CMAKE_CURRENT_LIST_DIR}/run_tree/projects/pbr_robot missing.scenegraph invalid
)

add_test(
    NAME vklive_app_command_line_tests
    COMMAND vklive_app_command_line_tests
)

if(VKLIVE_ENABLE_METAL)
    add_test(
        NAME vklive_metal_binding_tests
        COMMAND vklive_metal_binding_tests ${CMAKE_CURRENT_LIST_DIR}
    )

    add_test(NAME vklive_render_parity_simple_metal
        COMMAND vklive_render_parity_harness
            $<TARGET_FILE:Rezonality>
            ${CMAKE_SOURCE_DIR}/run_tree/projects/simple
            metal)
endif()

add_test(
    NAME vklive_render_backend_tests
    COMMAND vklive_render_backend_tests
)

add_test(
    NAME vklive_zep_imgui_input_tests
    COMMAND vklive_zep_imgui_input_tests
)

# App
set(APP_ROOT ${CMAKE_CURRENT_LIST_DIR}/app)
include(${APP_ROOT}/cmake/demo_common.cmake)

set(RESOURCE_FOLDER ${CMAKE_CURRENT_LIST_DIR}/app/res)
add_project_meta(META_FILES_TO_INCLUDE ${CMAKE_CURRENT_LIST_DIR}/app/res/app.manifest)

add_executable(Rezonality ${OS_BUNDLE} ${APP_SOURCES} ${CLIP_SOURCE} ${META_FILES_TO_INCLUDE})

target_include_directories(Rezonality
    PRIVATE
    ${CMAKE_BINARY_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/zep/include
    ${CMAKE_CURRENT_LIST_DIR}/app/include
    ${CMAKE_CURRENT_LIST_DIR}/libs
    ${STB_INCLUDE_DIRS}
    )

target_link_libraries(Rezonality
    PUBLIC
        vklive
        tinyfiledialogs::tinyfiledialogs
    PRIVATE
        $<TARGET_NAME_IF_EXISTS:SDL2::SDL2main>
        $<IF:$<TARGET_EXISTS:SDL2::SDL2>,SDL2::SDL2,SDL2::SDL2-static>
        unofficial::concurrentqueue::concurrentqueue
        clipp::clipp
        nfd::nfd
        unofficial::spirv-reflect
        gli
        Zing::Zing
    )

if(WIN32)
# Symbols
target_compile_options(Rezonality PRIVATE "$<$<CONFIG:Release>:/Zi>")
target_link_options(Rezonality PRIVATE "$<$<CONFIG:Release>:/DEBUG>")
target_link_options(Rezonality PRIVATE "$<$<CONFIG:Release>:/OPT:REF>")
target_link_options(Rezonality PRIVATE "$<$<CONFIG:Release>:/OPT:ICF>")
endif()

if("${CMAKE_SYSTEM_NAME}" STREQUAL "Linux")
    target_link_libraries(Rezonality PRIVATE X11 Xext xcb)
endif()

if (DEFINED ZEP_LIB)
    target_link_libraries(Rezonality PRIVATE Zep::Zep)
endif()

if (APPLE)
    install(TARGETS Rezonality
        EXPORT rezonality-targets
        COMPONENT binaries
        BUNDLE DESTINATION . COMPONENT Runtime
        RUNTIME DESTINATION bin COMPONENT Runtime
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        INCLUDES DESTINATION ${LIBLEGACY_INCLUDE_DIRS})
else()
    if (FALSE)
    # Artifacts are seperate, not in the bundle
    install(
        DIRECTORY
            ${REZONALITY_ROOT}/run_tree
        TYPE BIN
    )

    install(TARGETS Rezonality
        EXPORT rezonality-targets
        COMPONENT binaries
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        INCLUDES DESTINATION ${LIBLEGACY_INCLUDE_DIRS}
        )
    endif()
endif()

source_group (vklive//vulkan FILES ${VULKAN_SOURCES})
source_group (vklive//metal FILES ${METAL_SOURCES})
source_group (vklive FILES ${VK_SOURCES})
source_group (app FILES ${APP_SOURCES})
