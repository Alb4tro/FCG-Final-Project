add_subdirectory(yocto)
add_subdirectory(yocto_pathtrace)
add_subdirectory(yocto_extension)

if(YOCTO_OPENGL)
add_subdirectory(yocto_gui)
endif(YOCTO_OPENGL)

add_subdirectory(bcd)

TARGET_INCLUDE_DIRECTORIES(bcdcore INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/bcd)
TARGET_INCLUDE_DIRECTORIES(bcdcore INTERFACE ${CMAKE_CURRENT_BINARY_DIR}/bcd)