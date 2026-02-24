add_library(core
    spot_noise.cpp
    spot_noise.hpp
    spot_noise_constants.hpp
    noise.cpp
    noise.hpp
    utils.cpp
    utils.hpp
    random.hpp
    gradients.cpp
    gradients.hpp
)

target_include_directories(core PUBLIC ./)

target_link_libraries(core PUBLIC warnings_interface)

set_property(TARGET core PROPERTY CXX_STANDARD 26)