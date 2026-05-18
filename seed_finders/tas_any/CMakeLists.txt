add_executable(tas_any_seed_finder
    main.cpp
    stages.hpp
    stages.cpp
)

target_link_libraries(tas_any_seed_finder PRIVATE core)

set_property(TARGET tas_any_seed_finder PROPERTY CXX_STANDARD 23)