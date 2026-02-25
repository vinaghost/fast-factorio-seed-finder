add_library(core
    finder.cpp
    finder.hpp
    noise.cpp
    noise.hpp
    constants.hpp
    utils.cpp
    utils.hpp
    random.hpp
    gradients.cpp
    gradients.hpp
)

include(FetchContent)
FetchContent_Declare(
    argparse
    GIT_REPOSITORY https://github.com/p-ranav/argparse.git
)
FetchContent_MakeAvailable(argparse)

target_include_directories(core PUBLIC ./)

target_link_libraries(core PUBLIC warnings_interface argparse)

set_property(TARGET core PROPERTY CXX_STANDARD 26)