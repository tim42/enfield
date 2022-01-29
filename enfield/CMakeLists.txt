
add_library(enfield STATIC
    type_id.cpp
)


target_compile_options(enfield PRIVATE ${PROJECT_CXX_FLAGS})


target_link_libraries(enfield PUBLIC ntools)


target_include_directories(enfield PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_include_directories(enfield PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)

foreach(target ntools fmt)
    get_target_property(target_dirs ${target} INCLUDE_DIRECTORIES)
    target_include_directories(enfield PUBLIC ${target_dirs})
endforeach()
