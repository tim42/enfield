##
## CMAKE file for neam/...
##

cmake_minimum_required(VERSION 2.8)


set(PROJ_VERSION_MAJOR 0)
set(PROJ_VERSION_MINOR 0)
set(PROJ_VERSION_SUPERMINOR 1)

add_definitions(-D_PROJ_VERSION_MAJOR=${PROJ_VERSION_MAJOR})
add_definitions(-D_PROJ_VERSION_MINOR=${PROJ_VERSION_MINOR})
add_definitions(-D_PROJ_VERSION_SUPERMINOR=${PROJ_VERSION_SUPERMINOR})

set(PROJ_VERSION_NAME "")

set(PROJ_NAME enfield)

if (PROJ_VERSION_NAME STREQUAL "")
  set(PROJ_APP ${PROJ_NAME}-${PROJ_VERSION_MAJOR}.${PROJ_VERSION_MINOR}.${PROJ_VERSION_SUPERMINOR})
else ()
  set(PROJ_APP ${PROJ_NAME}-${PROJ_VERSION_NAME}-${PROJ_VERSION_MAJOR}.${PROJ_VERSION_MINOR}.${PROJ_VERSION_SUPERMINOR})
endif()

project(${PROJ_APP})

message(STATUS "-----------------------------------------------")
message(STATUS "${PROJ_NAME}")
message(STATUS "-----------------------------------------------")


if (CMAKE_BUILD_TYPE STREQUAL "")
  # CMake defaults to leaving CMAKE_BUILD_TYPE empty. This screws up
  # differentiation between debug and release builds.
  set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Choose the type of build, options are: None (CMAKE_CXX_FLAGS or CMAKE_C_FLAGS used) Debug Release MinSizeRel." FORCE)
endif ()

if (${CMAKE_CURRENT_BINARY_DIR} STREQUAL ${PROJECT_SOURCE_DIR})
  # And this screw up users.
  message(WARNING "when checking: '${CMAKE_CURRENT_BINARY_DIR}' is the same as '${PROJECT_SOURCE_DIR}'")
  message(WARNING "you shhould create a build folder in order to build ${PROJ_NAME} !!!")
endif()

# output
set(CMAKE_INSTALL_PREFIX "${CMAKE_CURRENT_BINARY_DIR}/")
set(EXECUTABLE_OUTPUT_PATH "${CMAKE_CURRENT_BINARY_DIR}/")
set(LIBRARY_OUTPUT_PATH "${CMAKE_CURRENT_BINARY_DIR}/libs")

# export the installation prefix
if (${CMAKE_PROJECT_NAME} STREQUAL ${PROJECT_NAME})
  set(NEAM_INSTALL_PREFIX ${CMAKE_INSTALL_PREFIX})
endif()

# deps check
include(cmake/deps.cmake)

# build / configure the third_party libs
# add_subdirectory("./third_party/")

# build the tools
add_subdirectory("./${PROJ_NAME}/tools")

# create the PROJ_FLAGS variable
include(cmake/flags.cmake)

# build a static lib
include_directories(${GLM_INCLUDE_DIR})
add_subdirectory(${PROJ_NAME})

# export project name to parent project
if (NOT ${CMAKE_PROJECT_NAME} STREQUAL ${PROJECT_NAME})
  set(lib${PROJ_NAME} ${PROJ_APP} PARENT_SCOPE)
endif()

# build the samples
if (${CMAKE_PROJECT_NAME} STREQUAL ${PROJECT_NAME})
  add_subdirectory(samples/)
endif()

# build the tools
# if (${CMAKE_PROJECT_NAME} STREQUAL ${PROJECT_NAME})
#   add_subdirectory(tools)
# endif()

