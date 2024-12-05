# TestApp CMake
message("===== Testing application: application: ${appName}")
if (MSVC)
    message ("Windows build")
else()
    set (URING_LIB "uring")
endif()
get_filename_component(appName ${CMAKE_CURRENT_SOURCE_DIR} NAME)


file(GLOB src_files *.cpp *.h *.hh *.cc)
add_executable(${appName} ${src_files} ${external_src})

target_compile_definitions(${appName} PUBLIC PROJECT_NAME="${appName}")
set_property(TARGET ${appName} PROPERTY POSITION_INDEPENDENT_CODE ON)
target_link_libraries(${appName} PRIVATE 
    client
    glog::glog
    ${Boost_LIBRARIES}
    ${URING_LIB}
)

target_compile_definitions(${appName} PUBLIC DAA_VERSION="v${CMAKE_PROJECT_VERSION}")
include_directories(
    ${CMAKE_SOURCE_DIR}/. 
    ${CMAKE_CURRENT_SOURCE_DIR}/.
    ${CMAKE_CURRENT_SOURCE_DIR}/../
)
set(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT ON)
if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
    set(CMAKE_INSTALL_PREFIX
        "${CMAKE_CURRENT_BINARY_DIR}/deploy"
        CACHE PATH "" FORCE
    )
endif()


