##
## CMAKE file for neam/yaggler dev sample
##

cmake_minimum_required(VERSION 2.8)

# set the name of the sample
set(SAMPLE_NAME "${PROJ_NAME}-test-dev")

include_directories(SYSTEM ${GLM_INCLUDE_DIR})
include_directories(SYSTEM ${GLFW_INCLUDE_DIR})
include_directories(SYSTEM ${LODEPNG_INCLUDE_DIR})
include_directories(../../)
add_definitions(${PROJ_FLAGS})

# avoid listing all the files
file(GLOB_RECURSE srcs ./*.cpp)

set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${PROJ_FLAGS}")
set(CMAKE_MODULE_LINKER_FLAGS "${CMAKE_MODULE_LINKER_FLAGS} ${PROJ_FLAGS}")

add_executable(${SAMPLE_NAME} ${srcs})

target_link_libraries(${SAMPLE_NAME} ${PROJ_DEPS_LIBS})
target_link_libraries(${SAMPLE_NAME} ${GLFW_LIBRARY})
target_link_libraries(${SAMPLE_NAME} ${LODEPNG_LIBRARY})
target_link_libraries(${SAMPLE_NAME} ${libntools})


# target_link_libraries(${SAMPLE_NAME} ${PROJ_APP})

