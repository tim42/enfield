
# set the name of the sample
set(SAMPLE_NAME "base-sample")

add_executable(${SAMPLE_NAME} main.cpp)

target_compile_options(${SAMPLE_NAME} PRIVATE ${PROJECT_CXX_FLAGS})


target_include_directories(${SAMPLE_NAME} PRIVATE SYSTEM ntools)
target_include_directories(${SAMPLE_NAME} PRIVATE SYSTEM fmt)
target_include_directories(${SAMPLE_NAME} PRIVATE SYSTEM enfield)

target_link_libraries(${SAMPLE_NAME} PUBLIC ntools)
target_link_libraries(${SAMPLE_NAME} PUBLIC fmt)
target_link_libraries(${SAMPLE_NAME} PUBLIC enfield)
if (${USE_TRACY})
  target_link_libraries(${SAMPLE_NAME} PUBLIC TracyClient)
endif()

