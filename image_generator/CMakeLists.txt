add_executable(image_generator
    main.cpp
)

target_link_libraries(image_generator PRIVATE core)

target_include_directories(image_generator SYSTEM PRIVATE ${CMAKE_SOURCE_DIR}/third_party/stb)

set_property(TARGET image_generator PROPERTY CXX_STANDARD 23)