cmake_minimum_required(VERSION 3.15)
project(SpaceInvaders3D)

set(CMAKE_CXX_STANDARD 17)

# Ищем библиотеки
find_package(GLEW CONFIG REQUIRED)
find_package(glfw3 CONFIG REQUIRED)
find_package(glm CONFIG REQUIRED)

add_executable(${PROJECT_NAME} main.cpp)

# Линкуем используя правильные имена целей (Targets)
# Обычно vcpkg экспортирует GLEW как GLEW::GLEW
target_link_libraries(${PROJECT_NAME} PRIVATE 
    GLEW::GLEW 
    glfw 
    glm::glm
    opengl32
)