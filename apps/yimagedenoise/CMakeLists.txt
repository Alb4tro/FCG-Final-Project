add_executable(yimagedenoise  yimagedenoise.cpp)

set_target_properties(yimagedenoise  PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED YES)
target_include_directories(yimagedenoise  PRIVATE ${CMAKE_SOURCE_DIR}/bcd/src)
target_link_libraries(yimagedenoise yocto yocto_gui bcdcore bcdio)