# This file is part of the reference implementation for the paper 
#   Bayesian Collaborative Denoising for Monte-Carlo Rendering
#   Malik Boughida and Tamy Boubekeur.
#   Computer Graphics Forum (Proc. EGSR 2017), vol. 36, no. 4, p. 137-153, 2017
#
# All rights reserved. Use of this source code is governed by a
# BSD-style license that can be found in the LICENSE.txt file.


# Verbose option: put ON to see the whole build command lines
#SET(CMAKE_VERBOSE_MAKEFILE ON)


#SET(bcd_library "")

SET(lib_bcd_core "bcdcore")
SET(lib_bcd_io "bcdio")
SET(exe_bcd_raw_converter "bcd-raw-converter")

#MESSAGE(STATUS "Entering subdirectory 'core'")
ADD_SUBDIRECTORY(core)

#MESSAGE(STATUS "Entering subdirectory 'io'")
ADD_SUBDIRECTORY(io)


#MESSAGE(STATUS "Entering subdirectory 'raw_converter'")
ADD_SUBDIRECTORY(raw_converter)

