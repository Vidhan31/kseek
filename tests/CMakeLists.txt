find_package(Qt6 REQUIRED COMPONENTS Test)

qt_add_executable(test_runner
    test_runner.cpp
)

target_link_libraries(test_runner PRIVATE
    kseek_core
    Qt6::Test
)

target_compile_definitions(test_runner PRIVATE
    KSEEK_SOURCE_DIR="${CMAKE_SOURCE_DIR}"
)

add_test(NAME test_runner COMMAND test_runner)

