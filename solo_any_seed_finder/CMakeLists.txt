add_executable(solo_any_seed_finder
    main.cpp
    stages.hpp
    stages.cpp
)

target_link_libraries(solo_any_seed_finder PRIVATE core)

set_property(TARGET solo_any_seed_finder PROPERTY CXX_STANDARD 26)