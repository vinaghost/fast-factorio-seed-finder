add_executable(benchmark
    main.cpp
)

target_link_libraries(benchmark PRIVATE core)

set_property(TARGET benchmark PROPERTY CXX_STANDARD 26)