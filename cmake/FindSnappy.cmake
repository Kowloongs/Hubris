find_path(SNAPPY_INCLUDE_DIRS NAMES snappy.h)
find_library(SNAPPY_LIBRARIES NAMES snappy)

include(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(
    Snappy DEFAULT_MSG
    SNAPPY_LIBRARIES SNAPPY_INCLUDE_DIRS)

mark_as_advanced(SNAPPY_INCLUDE_DIRS SNAPPY_LIBRARIES)
