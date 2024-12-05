cmake_minimum_required(VERSION 3.23.0)
project(boost_asio_client LANGUAGES C CXX)
include_directories("${PROJECT_BINARY_DIR}")


set(CMAKE_C_STANDARD 99)
set(CMAKE_CXX_STANDARD 20)

find_package(Boost REQUIRED)
find_package(glog REQUIRED)
message("------------------------- Our boost is found at ${Boost_INCLUDE_DIRS} --------------------------")
include_directories(${Boost_INCLUDE_DIRS} SYSTEM)
#target_include_directories(${PROJECT_NAME} PUBLIC .)
#target_compile_definitions(${PROJECT_NAME} PUBLIC AppName="${PROJECT_NAME}")
add_subdirectory(log)
add_subdirectory(client)
add_subdirectory(examples)

set(CPACK_PROJECT_NAME ${PROJECT_NAME})
include(CPack)

# test support
enable_testing()
