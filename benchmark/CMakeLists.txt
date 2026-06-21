###################################################################################################
## benchmark/CMakeLists.txt
##
## Defines the sync and async benchmark server executables.
## Enabled when -DOATPP_BUILD_BENCHMARKS=ON is passed.
###################################################################################################

# Sync benchmark server
add_executable(benchmark-sync
    sync/App.cpp
    sync/AppComponent.hpp
    sync/BenchController.hpp
    DTOs.hpp
)
# set_target_source_groups not used — benchmark dir is the source root

target_link_libraries(benchmark-sync PRIVATE oatpp)

target_include_directories(benchmark-sync PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

set_target_properties(benchmark-sync PROPERTIES
    CXX_STANDARD 17
    CXX_EXTENSIONS OFF
    CXX_STANDARD_REQUIRED ON
)
if(MSVC)
    target_compile_options(benchmark-sync PRIVATE /permissive-)
endif()

# Async benchmark server
add_executable(benchmark-async
    async/App.cpp
    async/AppComponent.hpp
    async/BenchController.hpp
    DTOs.hpp
)
# set_target_source_groups not used — benchmark dir is the source root

target_link_libraries(benchmark-async PRIVATE oatpp)

target_include_directories(benchmark-async PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

set_target_properties(benchmark-async PROPERTIES
    CXX_STANDARD 17
    CXX_EXTENSIONS OFF
    CXX_STANDARD_REQUIRED ON
)
if(MSVC)
    target_compile_options(benchmark-async PRIVATE /permissive-)
endif()

# Copy benchmark scripts to build directory
file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/scripts/ DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/scripts/)
