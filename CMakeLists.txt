cmake_minimum_required(VERSION 3.23.0)
project(http_client LANGUAGES C CXX)
file(GLOB src_files *.cpp *.h *.hh *.cc)
include_directories("${PROJECT_BINARY_DIR}")
add_executable(${PROJECT_NAME} ${src_files})

set(CMAKE_C_STANDARD 99)
set(CMAKE_CXX_STANDARD 20)

find_package(Boost REQUIRED)
message("------------------------- Our boost is found at ${Boost_INCLUDE_DIRS} --------------------------")
include_directories(${Boost_INCLUDE_DIRS} SYSTEM)
target_compile_definitions(${PROJECT_NAME} PUBLIC AppName="${PROJECT_NAME}")


set(CPACK_PROJECT_NAME ${PROJECT_NAME})
include(CPack)

# test support
enable_testing()
