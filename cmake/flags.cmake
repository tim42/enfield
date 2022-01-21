
if(MSVC)
    set(PROJECT_CXX_FLAGS /W4)
else()
    set(PROJECT_CXX_FLAGS -Wall -Wextra)
endif()

# Workaround kdevelop refusing the C++23 standard if set in cmake...
#if(CMAKE_EXPORT_COMPILE_COMMANDS EQUAL ON)
    if(MSVC)
        # as of the writing of this file, it does not seems msvc has a flag for C++23
        set(PROJECT_CXX_FLAGS ${PROJECT_CXX_FLAGS} /std:c++latest)
    else()
        # kdevelop (and probably clang) requires 2b and not 23
        set(PROJECT_CXX_FLAGS ${PROJECT_CXX_FLAGS} -std=gnu++2b)
    endif()
#else()
    # this is the proper way, but kdevelop does not like it
    #set(CMAKE_CXX_STANDARD 23)
#endif()
