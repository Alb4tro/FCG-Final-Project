add_executable(yimagedenoise  yimagedenoise.cpp)


set_target_properties(yimagedenoise  PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED YES)
target_include_directories(yimagedenoise  PRIVATE ${CMAKE_SOURCE_DIR}/bcd/src)
target_link_libraries(yimagedenoise bcdcore bcdio)

add_executable(yimagedenoisetry  similyocto.cpp)


set_target_properties(yimagedenoisetry  PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED YES)
target_include_directories(yimagedenoisetry  PRIVATE ${CMAKE_SOURCE_DIR}/bcd/src)
target_link_libraries(yimagedenoisetry yocto yocto_gui bcdcore bcdio)