#----------------------------------------------------------------
# Generated CMake target import file for configuration "Debug".
#----------------------------------------------------------------

# Commands may need to know the format version.
set(CMAKE_IMPORT_FILE_VERSION 1)

# Import target "SQLite::sqlite3" for configuration "Debug"
set_property(TARGET SQLite::sqlite3 APPEND PROPERTY IMPORTED_CONFIGURATIONS DEBUG)
set_target_properties(SQLite::sqlite3 PROPERTIES
  IMPORTED_LINK_INTERFACE_LANGUAGES_DEBUG "C"
  IMPORTED_LOCATION_DEBUG "${_IMPORT_PREFIX}/lib/sqlite3d.lib"
  )

list(APPEND _cmake_import_check_targets SQLite::sqlite3 )
list(APPEND _cmake_import_check_files_for_SQLite::sqlite3 "${_IMPORT_PREFIX}/lib/sqlite3d.lib" )

# Commands beyond this point should not need to know the version.
set(CMAKE_IMPORT_FILE_VERSION)
