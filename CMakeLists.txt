cmake_minimum_required(VERSION 3.22)
include(ExternalProject)

#
# Top-level CMake project for the NUCLEO-N65X0Q-ISP demo (FSBL + Appli),
# hand-converted from the STM32CubeIDE Eclipse/Makefile project to mirror
# the layout used by the sibling STM32N6_Face_Detection CMake project.
#

# In order to use CMake for cross-compiling
set(CMAKE_CXX_COMPILER_FORCED "true")

# Set the project name
set(CMAKE_PROJECT_NAME NUCLEO_N65X0Q_ISP)

# Core project settings
project(${CMAKE_PROJECT_NAME})

# Include external project file
include("mx-generated.cmake")
