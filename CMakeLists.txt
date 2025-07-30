

cmake_minimum_required(VERSION 3.0...3.25)
project(
    BaseWebGPU # name of project / VS solution
    VERSION 0.1.0
    LANGUAGES CXX C # programming languages used by the project
)

if (NOT EMSCRIPTEN)
    # Add the 'glfw' directory, which contains the definition of a 'glfw' target
    add_subdirectory(glfw)
else()
    # Create a mock 'glfw' target that just sets the `-sUSE_GLFW=3` link option:
    add_library(glfw INTERFACE)
    target_link_options(glfw INTERFACE -sUSE_GLFW=3)
endif()

# Include webgpu directory, to define the 'webgpu' target
add_subdirectory(webgpu)
add_subdirectory(glfw3webgpu)

add_executable(App
	main.cpp
	webgpu-utils.h
	webgpu-utils.cpp
)

# Add the 'webgpu' target as a dependency of our App
target_link_libraries(App PRIVATE webgpu glfw glfw3webgpu)

# The application's binary must find wgpu.dll or libwgpu.so at runtime,
# so we automatically copy it (it's called WGPU_RUNTIME_LIB in general)
# next to the binary.
target_copy_webgpu_binaries(App)

set_target_properties(App PROPERTIES
    CXX_STANDARD 17 # required minimum standard is CPP17
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
    COMPILE_WARNING_AS_ERROR ON
)

# have as many warning as possible for now
if (MSVC)
        target_compile_options(App PRIVATE /W4)
    else()
        target_compile_options(App PRIVATE -Wall -Wextra -pedantic)
    endif()

# Enable the use of emscripten_sleep()
target_link_options(App PRIVATE -sASYNCIFY)


