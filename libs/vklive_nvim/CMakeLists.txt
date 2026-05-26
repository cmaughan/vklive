add_library(vklive_nvim STATIC
    src/nvim_host.cpp
    src/nvim_process.cpp

    include/vklive_nvim/nvim_host.h
    include/vklive_nvim/nvim_process.h
)

add_library(vklive_nvim::vklive_nvim ALIAS vklive_nvim)

target_compile_features(vklive_nvim PUBLIC cxx_std_20)

target_include_directories(vklive_nvim
    PUBLIC
        ${CMAKE_CURRENT_LIST_DIR}/include
)
