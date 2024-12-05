get_filename_component(libName ${CMAKE_CURRENT_SOURCE_DIR} NAME)

file(GLOB src_files *.cpp *.h)
add_library(${libName} STATIC ${src_files}) 
target_include_directories(${libName} PUBLIC . ${glog_INCLUDE_DIRS})
target_link_libraries( ${libName} glog::glog)
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/.
    ${CMAKE_CURRENT_SOURCE_DIR}/.. 
    ${CMAKE_CURRENT_SOURCE_DIR}/../..
  Boost::headers
)