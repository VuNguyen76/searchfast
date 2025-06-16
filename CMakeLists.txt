cmake_minimum_required(VERSION 3.20)
project(FastFileSearch VERSION 1.0.0 LANGUAGES CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Set build type if not specified
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# Compiler-specific options
if(MSVC)
    add_compile_options(/W4 /WX /permissive-)
    add_compile_definitions(_WIN32_WINNT=0x0A00)  # Windows 10
else()
    add_compile_options(-Wall -Wextra -Wpedantic -Werror)
endif()

# Find required packages
find_package(Qt6 REQUIRED COMPONENTS Core)
find_package(SQLite3 REQUIRED)
find_package(Threads REQUIRED)

# Include directories
include_directories(${CMAKE_SOURCE_DIR}/include)

# Source files
set(CORE_SOURCES
    src/core/logger.cpp
    src/core/types.cpp
)

set(STORAGE_SOURCES
    src/storage/sqlite_database.cpp
    src/storage/cache_manager.cpp
    src/storage/memory_index.cpp
)

set(ENGINE_SOURCES
    src/engine/index_manager.cpp
    src/engine/file_watcher.cpp
    src/engine/search_engine.cpp
    src/engine/fuzzy_matcher.cpp
    src/engine/regex_matcher.cpp
    src/engine/wildcard_matcher.cpp
)

set(APP_SOURCES
    src/app/search_manager.cpp
    src/app/config_manager.cpp
    src/app/export_manager.cpp
)

set(PLATFORM_SOURCES)
if(WIN32)
    list(APPEND PLATFORM_SOURCES src/platform/windows_file_watcher.cpp)
elseif(UNIX AND NOT APPLE)
    list(APPEND PLATFORM_SOURCES src/platform/linux_file_watcher.cpp)
elseif(APPLE)
    list(APPEND PLATFORM_SOURCES src/platform/macos_file_watcher.cpp)
endif()

# Create main library
add_library(FastFileSearchLib STATIC
    ${CORE_SOURCES}
    ${STORAGE_SOURCES}
    ${ENGINE_SOURCES}
    ${APP_SOURCES}
    ${PLATFORM_SOURCES}
)

# Link libraries
target_link_libraries(FastFileSearchLib
    Qt6::Core
    SQLite::SQLite3
    Threads::Threads
)

# Platform-specific libraries
if(WIN32)
    target_link_libraries(FastFileSearchLib kernel32 advapi32)
elseif(UNIX AND NOT APPLE)
    target_link_libraries(FastFileSearchLib pthread)
elseif(APPLE)
    target_link_libraries(FastFileSearchLib "-framework CoreServices")
endif()

# Console executable
add_executable(FastFileSearch src/main.cpp)
target_link_libraries(FastFileSearch
    FastFileSearchLib
    Qt6::Core
    SQLite::SQLite3
    Threads::Threads
)

# GUI executable
add_executable(FastFileSearchGUI
    src/main_gui.cpp
    src/ui/main_window.cpp
)
target_link_libraries(FastFileSearchGUI
    FastFileSearchLib
    Qt6::Core
    Qt6::Widgets
    Qt6::Gui
    SQLite::SQLite3
    Threads::Threads
)

# Full indexing test
add_executable(FullIndexingTest test_full_indexing.cpp)
target_link_libraries(FullIndexingTest
    FastFileSearchLib
    Qt6::Core
    SQLite::SQLite3
    Threads::Threads
)

# Enable testing
enable_testing()
add_subdirectory(tests)

# Install targets
install(TARGETS FastFileSearch
    RUNTIME DESTINATION bin
)

# CPack configuration
set(CPACK_PACKAGE_NAME "FastFileSearch")
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "High-performance file search application")
set(CPACK_PACKAGE_VENDOR "FastFileSearch Team")

if(WIN32)
    set(CPACK_GENERATOR "NSIS")
else()
    set(CPACK_GENERATOR "TGZ")
endif()

include(CPack)
