# Find Reflective
#
# REFLECTIVE_INCLUDE_DIR
# REFLECTIVE_LIBRARY
# REFLECTIVE_FOUND
#

find_path(REFLECTIVE_INCLUDE_DIR
  NAMES reflective
  PATHS /usr/local /usr
  ENV REFLECTIVE_DIR
  PATH_SUFFIXES include/neam include-unix/neam
)

find_library(REFLECTIVE_LIBRARY
  NAMES reflective-0.0.1
  PATHS /usr/local /usr
  ENV REFLECTIVE_LIB
  PATH_SUFFIXES lib64/neam lib/neam
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Reflective DEFAULT_MSG REFLECTIVE_LIBRARY REFLECTIVE_INCLUDE_DIR)

mark_as_advanced(REFLECTIVE_INCLUDE_DIR REFLECTIVE_LIBRARY)
