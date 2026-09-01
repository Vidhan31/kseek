cmake_minimum_required(VERSION 3.25)

file(READ "${CMAKE_CURRENT_SOURCE_DIR}/VERSION" KSEEK_VERSION_RAW)
string(STRIP "${KSEEK_VERSION_RAW}" KSEEK_VERSION)

project(kseek VERSION ${KSEEK_VERSION} LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Compiler optimization and hardening flags
add_compile_options(
    -Wall -Wextra -Wpedantic
    -fvisibility=hidden
    -fvisibility-inlines-hidden
    -ffunction-sections
    -fdata-sections
)
add_link_options(
    -Wl,--gc-sections
    -Wl,--as-needed
)

add_compile_definitions(
    QT_NO_CAST_FROM_ASCII
    QT_NO_CAST_TO_ASCII
    QT_NO_CAST_FROM_BYTEARRAY
    QT_STRICT_ITERATORS
    QT_DISABLE_DEPRECATED_BEFORE=0x060400
    KSEEK_VERSION="${PROJECT_VERSION}"
)

# Qt 6 dependencies
find_package(Qt6 6.4 REQUIRED COMPONENTS Core DBus)
qt_standard_project_setup()

include(GNUInstallDirs)
include(CTest)

# Static core library for runner logic & tests
add_library(kseek_core STATIC
    src/types.h
    src/config.h
    src/config.cpp
    src/icon_resolver.h
    src/icon_resolver.cpp
    src/process_pipeline.h
    src/process_pipeline.cpp
    src/action_handler.h
    src/action_handler.cpp
    src/kseek_runner.h
    src/kseek_runner.cpp
    src/runner_adaptor.h
    src/runner_adaptor.cpp
)

target_link_libraries(kseek_core PUBLIC
    Qt6::Core
    Qt6::DBus
)

target_include_directories(kseek_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# Main Executable
qt_add_executable(kseek
    src/main.cpp
)

target_link_libraries(kseek PRIVATE
    kseek_core
)

# Configuration & Service Files
if(NOT DEFINED KSEEK_EXECUTABLE)
    set(KSEEK_EXECUTABLE "${CMAKE_INSTALL_FULL_BINDIR}/kseek")
endif()

configure_file(
    plasma-runner-kseek.desktop.in
    ${CMAKE_CURRENT_BINARY_DIR}/plasma-runner-kseek.desktop
    @ONLY
)
configure_file(
    org.kde.krunner.kseek.service.in
    ${CMAKE_CURRENT_BINARY_DIR}/org.kde.krunner.kseek.service
    @ONLY
)
configure_file(
    plasma-runner-kseek.service.in
    ${CMAKE_CURRENT_BINARY_DIR}/plasma-runner-kseek.service
    @ONLY
)

# Installation rules
install(TARGETS kseek DESTINATION ${CMAKE_INSTALL_BINDIR})
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/plasma-runner-kseek.desktop DESTINATION ${CMAKE_INSTALL_DATADIR}/krunner/dbusplugins)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/org.kde.krunner.kseek.service DESTINATION ${CMAKE_INSTALL_DATADIR}/dbus-1/services)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/plasma-runner-kseek.service DESTINATION ${CMAKE_INSTALL_DATADIR}/systemd/user)

# Unit / Integration Testing
if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
