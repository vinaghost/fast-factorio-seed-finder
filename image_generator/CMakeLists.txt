add_executable(image_generator
    main.cpp
)

target_link_libraries(image_generator PRIVATE core)

set_property(TARGET image_generator PROPERTY CXX_STANDARD 26)