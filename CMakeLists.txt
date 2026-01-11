cmake_minimum_required(VERSION 3.15)

if(NOT EXISTS "${CMAKE_SOURCE_DIR}/vcpkg/scripts/buildsystems/vcpkg.cmake")
    message(STATUS "vcpkg no detectado. Intentando inicializar submódulos...")
    find_package(Git REQUIRED)
    execute_process(
        COMMAND ${GIT_EXECUTABLE} submodule update --init --recursive
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    )
endif()

if(NOT EXISTS "${CMAKE_SOURCE_DIR}/vcpkg/vcpkg.exe" AND NOT EXISTS "${CMAKE_SOURCE_DIR}/vcpkg/vcpkg")
    message(STATUS "Preparando vcpkg (Bootstrap)...")
    if(WIN32)
        execute_process(COMMAND cmd /c bootstrap-vcpkg.bat WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/vcpkg)
    else()
        execute_process(COMMAND sh ./bootstrap-vcpkg.sh WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/vcpkg)
    endif()
endif()

if(WIN32)
    if(NOT DEFINED VCPKG_TARGET_TRIPLET)
        set(VCPKG_TARGET_TRIPLET "x64-windows-static" CACHE STRING "")
    endif()
endif()

if(NOT DEFINED CMAKE_TOOLCHAIN_FILE)
    set(CMAKE_TOOLCHAIN_FILE "${CMAKE_SOURCE_DIR}/vcpkg/scripts/buildsystems/vcpkg.cmake"
        CACHE STRING "vcpkg toolchain file")
endif()

project(simulador)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(glfw3 CONFIG REQUIRED)
find_package(glm CONFIG REQUIRED)
find_package(glad CONFIG REQUIRED)

add_executable(Simulador_G Main.cpp)

target_link_libraries(Simulador_G PRIVATE 
    glfw 
    glm::glm
    glad::glad
)
