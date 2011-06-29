# Find FCLIB includes and libraries.
# The following variables are set if FCLIB is found.  If FCLIB is not
# found, FCLIB_FOUND is set to false.
#  FCLIB_FOUND        - True when the FCLIB include directory is found.
#  FCLIB_INCLUDE_DIRS - the path to where the Siconos FCLIB include files are.
#  FCLIB_LIBRARY_DIRS - The path to where the Siconos library files are.
#  FCLIB_LIBRARIES    - The libraries to link against Siconos FCLIB

# One may want to use a specific FCLIB Library by setting
# FCLIB_LIBRARY_DIRECTORY before FIND_PACKAGE(FCLIB)

IF(FCLIB_LIBRARY_DIRECTORY)
  FIND_LIBRARY(FCLIB_FOUND fclib PATHS "${FCLIB_LIBRARY_DIRECTORY}")
ELSE(FCLIB_LIBRARY_DIRECTORY)
  FIND_LIBRARY(FCLIB_FOUND fclib)
ENDIF(FCLIB_LIBRARY_DIRECTORY)


IF(FCLIB_FOUND)
  GET_FILENAME_COMPONENT(FCLIB_LIBRARY_DIRS ${FCLIB_FOUND} PATH)
  SET(FCLIB_LIBRARIES ${FCLIB_FOUND} ${FCLIB_COMMON_FOUND} ${METIS_FOUND})
  GET_FILENAME_COMPONENT(FCLIB_LIBRARY_DIRS_DIR ${FCLIB_LIBRARY_DIRS} PATH)
  SET(FCLIB_INCLUDE_DIRS ${FCLIB_LIBRARY_DIRS_DIR}/include)
ELSE(FCLIB_FOUND)
  IF(FCLIB_FIND_REQUIRED)
    MESSAGE(FATAL_ERROR
      "Required FCLIB library not found. Please specify library location in FCLIB_LIBRARY_DIRECTORY")
  ENDIF(FCLIB_FIND_REQUIRED)
ENDIF(FCLIB_FOUND)
