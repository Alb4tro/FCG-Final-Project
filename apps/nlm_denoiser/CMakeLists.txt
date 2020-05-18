add_executable(nlm_denoiser nlm_denoiser.cpp)

set_target_properties(nlm_denoiser PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED YES)
target_include_directories(nlm_denoiser PUBLIC ${CMAKE_SOURCE_DIR}/libs)
target_link_libraries(nlm_denoiser yocto)
