##
## CMAKE file for neam/...
##

cmake_minimum_required(VERSION 3.21)

project(enfield VERSION 0.0.1)

message(STATUS "-----------------------------------------------")
message(STATUS "            ${CMAKE_PROJECT_NAME}")
message(STATUS "-----------------------------------------------")

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/)

set(CMAKE_POSITION_INDEPENDENT_CODE ON)

option(USE_TRACY "Enable Tracy support" ON)

if (${USE_TRACY})
  message(STATUS "Profiling with Tracy enabled")
  add_compile_definitions(TRACY_ENABLE=1)
else()
  message(STATUS "Profiling with Tracy disabled")
endif()

# dependencies:
include(cmake/deps.cmake)

if (NOT DEFINED ENFIELD_IS_MASTER_PROJECT)
  option(ENFIELD_IS_MASTER_PROJECT "Use the third-party folder and build samples" ON)
endif()

if (${ENFIELD_IS_MASTER_PROJECT})
  # build / configure the third_party libs
  add_subdirectory(third_party/)
endif()

# project flags:
include(cmake/flags.cmake)

if (${CMAKE_CURRENT_BINARY_DIR} STREQUAL ${PROJECT_SOURCE_DIR})
  message(WARNING "when checking: '${CMAKE_CURRENT_BINARY_DIR}' is the same as '${PROJECT_SOURCE_DIR}'")
  message(WARNING "you shhould create a build folder in order to build ${PROJ_NAME} !!!")
endif()

# global setup:
add_compile_definitions(N_HYDRA_RESOURCES_OBFUSCATE=1) # obfuscate the index and the resources. (require an index rebuild / res repack)
add_compile_definitions(N_RLE_VERBOSE_FAILS=1)
add_compile_definitions(N_RES_MAX_SIZE_TO_EMBED=64) # resources <= this size will be written in the index and always be in memory
add_compile_definitions(N_ALLOW_DEBUG=false)
add_compile_definitions(N_DISABLE_CHECKS=false)

# build a static lib
add_subdirectory(enfield/)

if (${ENFIELD_IS_MASTER_PROJECT})
  # build the samples
  add_subdirectory(samples/)
endif()

