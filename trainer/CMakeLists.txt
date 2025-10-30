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
    src/aimbot.cpp
    src/gl_hook.cpp
    src/render_utils.cpp
    src/ui.cpp
    src/pch.cpp
)

set(IMGUI_SOURCES
    third_party/imgui/imgui.cpp
    third_party/imgui/imgui_draw.cpp
    third_party/imgui/imgui_tables.cpp
    third_party/imgui/imgui_widgets.cpp
    third_party/imgui/imgui_impl_opengl3.cpp
    third_party/imgui/imgui_impl_win32.cpp
)

# Header files
set(HEADERS
    include/trainer.h
    include/aimbot.h
    include/memory.h
    include/gl_hook.h
    include/render_utils.h
    include/ui.h
    src/pch.h
)

add_library(minhook STATIC
    third_party/minhook/src/buffer.c
    third_party/minhook/src/hook.c
    third_party/minhook/src/trampoline.c
    third_party/minhook/src/hde/hde32.c
    third_party/minhook/src/hde/hde64.c
)

target_include_directories(minhook
    PUBLIC ${PROJECT_SOURCE_DIR}/third_party/minhook/include
    PRIVATE ${PROJECT_SOURCE_DIR}/third_party/minhook/src
            ${PROJECT_SOURCE_DIR}/third_party/minhook/src/hde
)

# Create DLL
add_library(ACTrainer SHARED ${SOURCES} ${IMGUI_SOURCES} ${HEADERS})

# Link libraries
target_link_libraries(ACTrainer
    minhook
    psapi
    opengl32   # For OpenGL API (glGetDoublev, etc.)
    gdi32
    imm32
)

target_include_directories(ACTrainer
    PRIVATE ${PROJECT_SOURCE_DIR}/third_party/imgui
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
