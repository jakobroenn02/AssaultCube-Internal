cmake_minimum_required(VERSION 3.15)
project(ACTrainer)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Output directory
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# Include directories
include_directories(${PROJECT_SOURCE_DIR}/include)

# Source files
set(SOURCES
    src/dllmain.cpp
    src/trainer.cpp
    src/pch.cpp
)

# Header files
set(HEADERS
    include/trainer.h
    include/memory.h
    src/pch.h
)

# Create DLL
add_library(ACTrainer SHARED ${SOURCES} ${HEADERS})

# Link libraries
target_link_libraries(ACTrainer
    psapi
)

# Precompiled header
target_precompile_headers(ACTrainer PRIVATE src/pch.h)

# Compiler flags
if(MSVC)
    target_compile_options(ACTrainer PRIVATE /W4)
    # Set output name
    set_target_properties(ACTrainer PROPERTIES OUTPUT_NAME "actrainer")
else()
    target_compile_options(ACTrainer PRIVATE -Wall -Wextra -Wpedantic)
endif()

# Post-build: Copy DLL to root trainer directory for easy access
add_custom_command(TARGET ACTrainer POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy
    $<TARGET_FILE:ACTrainer>
    ${PROJECT_SOURCE_DIR}/actrainer.dll
    COMMENT "Copying DLL to trainer directory"
)
