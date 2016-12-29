

set(PROJ_SOURCES
  type_id.cpp
)

add_definitions(${PROJ_FLAGS})


add_library(${PROJ_APP} STATIC ${PROJ_SOURCES})

# install
install(DIRECTORY ./ DESTINATION include/neam/enfield
        FILES_MATCHING PATTERN "*.hpp"
        PATTERN "tools" EXCLUDE        # submodule tools has its own include rules
        PATTERN "persistence" EXCLUDE  # submodule persistence has it own include rules
        PATTERN ".git" EXCLUDE
        PATTERN "cmake" EXCLUDE
        PATTERN "samples" EXCLUDE)

# link tools
set(tools_dest ${NEAM_INSTALL_PREFIX}/include/neam/enfield/)
set(tools_target ${NEAM_INSTALL_PREFIX}/include/neam/tools)
if (UNIX)
  install(CODE "execute_process(COMMAND ln -s \"${tools_target}\" \"${tools_dest}/\")")
else()
  install(CODE "file(COPY ${tools_target} DESTINATION ${tools_dest})")
endif()

# link persistence
set(persistence_dest ${NEAM_INSTALL_PREFIX}/include/neam/enfield/)
set(persistence_target ${NEAM_INSTALL_PREFIX}/include/neam/persistence)
if (UNIX)
  install(CODE "execute_process(COMMAND ln -s \"${persistence_target}\" \"${persistence_dest}/\")")
else()
  install(CODE "file(COPY ${persistence_target} DESTINATION ${persistence_dest})")
endif()
