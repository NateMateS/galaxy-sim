cmake_minimum_required(VERSION 3.10)
project(SpaceGalaxySim)

set(CMAKE_CXX_STANDARD 17)

# Source files
file(GLOB SOURCES "Space C++/*.cpp" "Space C++/*.c")
# Add glad.c explicitly if not picked up (it is in libs/glad/src/glad.c)
list(APPEND SOURCES "libs/glad/src/glad.c")

# Include directories
include_directories("libs/glfw/include")
include_directories("libs/glad/include")
include_directories("Space C++")

# Libraries
# Assuming MSVC/Windows for now as per original project setup
link_directories("libs/glfw/lib-vc2022")

add_executable(galaxy-sim ${SOURCES})

# Link libraries
target_link_libraries(galaxy-sim glfw3 opengl32)

# Copy assets to build directory
add_custom_command(TARGET galaxy-sim POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/assets
        $<TARGET_FILE_DIR:galaxy-sim>/assets
    COMMENT "Copying assets to output directory"
)
