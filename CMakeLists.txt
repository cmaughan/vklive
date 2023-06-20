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

# Set additional project information
set(COMPANY "Rezonality")
set(COPYRIGHT "Copyright (c) 2022 Rezonality")
set(IDENTIFIER "com.rezonality.vklive")

find_package(Vulkan REQUIRED) # Vulkan API
find_package(assimp CONFIG REQUIRED) # Asset import for loading models
find_package(clipp CONFIG REQUIRED) # Commandline parser for command line arguments
find_package(fmt CONFIG REQUIRED) # String formatting library
find_package(reproc++ REQUIRED) # Process launch library for loading shader compiler binary
find_package(SDL2 CONFIG REQUIRED) # SDL UI library for creating windows, etc.
find_package(tomlplusplus CONFIG REQUIRED) # Toml file format parser for reading config files
find_package(unofficial-concurrentqueue REQUIRED) # Concurrent lock/free queue for thread interactions
find_package(tinyfiledialogs CONFIG REQUIRED) # File dialogs that look OS specific and more standard
find_package(unofficial-nativefiledialog CONFIG REQUIRED) # File dialogs that look OS specific and more standard
find_package(unofficial-spirv-reflect CONFIG REQUIRED)
find_package(unofficial-vulkan-memory-allocator CONFIG REQUIRED)
find_package(gli CONFIG REQUIRED)
find_path(STB_INCLUDE_DIRS "stb_c_lexer.h")
find_package(range-v3 CONFIG REQUIRED)

# Set this if we are sitting on SDL
add_definitions(-DZEP_USE_SDL)

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
add_subdirectory(external/zing)

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
    src/vulkan/vulkan_framebuffer.cpp
    src/vulkan/vulkan_imgui.cpp
    src/vulkan/vulkan_imgui_viewports.cpp
    src/vulkan/vulkan_model.cpp
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
    include/vklive/vulkan/vulkan_framebuffer.h
    include/vklive/vulkan/vulkan_imgui.h
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
)

set(EXTERNAL_DIR external)

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
    #src/imgui/imgui_utils.cpp
    src/model.cpp
    src/pass.cpp
    src/process/process.cpp
    src/scene.cpp
    src/validation.cpp

    external/mpc/mpc.c

    include/vklive/IDevice.h
    include/vklive/camera.h
    include/vklive/model.h
    include/vklive/pass.h
    include/vklive/process/process.h
    include/vklive/scene.h
    include/vklive/validation.h

)

set(LIB_SOURCES
    ${VULKAN_SOURCES}
    ${VK_SOURCES}
    )

set(APP_SOURCES
    TODO.md
    README.md
    app/src/config.cpp
    app/src/controller.cpp
    app/src/editor.cpp
    app/src/main.cpp
    app/src/menu.cpp
    app/src/project.cpp

    app/include/app/config.h
    app/include/app/controller.h
    app/include/app/editor.h
    app/include/app/menu.h
    app/include/app/project.h
)

set_source_files_properties (external/mpc/mpc.c PROPERTIES SKIP_PRECOMPILE_HEADERS 1)

add_library(vklive ${LIB_SOURCES})
target_link_libraries(vklive
    PUBLIC
        Vulkan::Vulkan
        reproc++
        assimp::assimp
        fmt::fmt-header-only
        tomlplusplus::tomlplusplus
        range-v3
        range-v3-meta
        range-v3::meta
        range-v3-concepts
        Zing::Zing
    )
target_precompile_headers(vklive
  PRIVATE
    include/vklive/vulkan/vulkan_context.h
)

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
        ${CMAKE_CURRENT_SOURCE_DIR}/external
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
    ${CMAKE_CURRENT_LIST_DIR}/external
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
        unofficial::nativefiledialog::nfd
        unofficial::spirv-reflect::spirv-reflect
        unofficial::vulkan-memory-allocator::vulkan-memory-allocator
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

source_group (vklive//vulkan FILES ${VULKAN_SOURCES})
source_group (vklive FILES ${VK_SOURCES})
source_group (app FILES ${APP_SOURCES})
