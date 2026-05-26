add_library(vklive_nvim STATIC
    src/mpack_codec.cpp
    src/nvim_ui.cpp
    src/nvim_host.cpp
    src/nvim_process.cpp
    src/nvim_rpc.cpp
    src/render_model.cpp
    third_party/mpack/mpack-common.c
    third_party/mpack/mpack-expect.c
    third_party/mpack/mpack-node.c
    third_party/mpack/mpack-platform.c
    third_party/mpack/mpack-reader.c
    third_party/mpack/mpack-writer.c

    include/vklive_nvim/mpack_codec.h
    include/vklive_nvim/nvim_host.h
    include/vklive_nvim/nvim_process.h
    include/vklive_nvim/nvim_rpc.h
    include/vklive_nvim/nvim_ui.h
    include/vklive_nvim/render_model.h
)

add_library(vklive_nvim::vklive_nvim ALIAS vklive_nvim)

target_compile_features(vklive_nvim PUBLIC cxx_std_20)
target_compile_definitions(vklive_nvim PUBLIC MPACK_EXTENSIONS=1)

target_include_directories(vklive_nvim
    PUBLIC
        ${CMAKE_CURRENT_LIST_DIR}/include
    PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}/third_party/mpack
)
