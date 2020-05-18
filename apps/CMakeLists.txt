add_subdirectory(yscenetrace)
add_subdirectory(yimagedenoise)
add_subdirectory(nlm_denoiser)

if(YOCTO_OPENGL)
add_subdirectory(ysceneitraces)
endif(YOCTO_OPENGL)
