#----------------------------------------------------------------
# Generated CMake target import file.
#----------------------------------------------------------------

# Commands may need to know the format version.
set(CMAKE_IMPORT_FILE_VERSION 1)

# Import target "HonkordGL::HonkordGL" for configuration ""
set_property(TARGET HonkordGL::HonkordGL APPEND PROPERTY IMPORTED_CONFIGURATIONS NOCONFIG)
set_target_properties(HonkordGL::HonkordGL PROPERTIES
  IMPORTED_LOCATION_NOCONFIG "${_IMPORT_PREFIX}/lib/libHonkordGL.so"
  IMPORTED_SONAME_NOCONFIG "libHonkordGL.so"
  )

list(APPEND _cmake_import_check_targets HonkordGL::HonkordGL )
list(APPEND _cmake_import_check_files_for_HonkordGL::HonkordGL "${_IMPORT_PREFIX}/lib/libHonkordGL.so" )

# Commands beyond this point should not need to know the version.
set(CMAKE_IMPORT_FILE_VERSION)
