# ZLIB!
#
# Doesn't "find" ZLIB so much as it download's and builds it.
# 
include(ExternalProject)
include(FindPackageHandleStandardArgs)

macro(show v)
  message("${v} is ${${v}}")
endmacro()

if(NOT TARGET libz)
  set( _PATCH_FILE "${PROJECT_BINARY_DIR}/cmake/zlib-patch.cmake" )
  set( _SOURCE_DIR "${PROJECT_BINARY_DIR}/libz-prefix/src/libz" ) # Assumes default prefix path
  configure_file(  "${PROJECT_SOURCE_DIR}/cmake/zlib-patch.cmake.in" "${_PATCH_FILE}" @ONLY )
  ExternalProject_Add(libz
    URL        http://zlib.net/zlib-1.2.7.tar.gz
    URL_MD5    60df6a37c56e7c1366cca812414f7b85
    #For MSVC x64: a patch is required to deal with static libraries 
    #that use an rc file (such a zlibstatic)
    PATCH_COMMAND "${CMAKE_COMMAND};-P;${_PATCH_FILE}" 
    CMAKE_ARGS -DCMAKE_INSTALL_PREFIX:PATH=<INSTALL_DIR>
               -DCMAKE_PREFIX_PATH:PATH=${PROJECT_SOURCE_DIR}/cmake    
               -DCMAKE_C_FLAGS=-fPIC
    )
endif()

get_target_property(_ZLIB_INCLUDE_DIR libz _EP_SOURCE_DIR)
get_target_property(ZLIB_ROOT_DIR     libz _EP_INSTALL_DIR)

set(ZLIB_INCLUDE_DIR ${ZLIB_ROOT_DIR}/include CACHE PATH "Path to zlib.h" FORCE)

add_library(z STATIC IMPORTED)
if(WIN32)
  set(DEBUG_POSTFIX)
  set(LIB zlibstatic)
else()
  set(DEBUG_POSTFIX)
  set(LIB ${CMAKE_STATIC_LIBRARY_PREFIX}z)
endif()

set_target_properties(z PROPERTIES
  IMPORTED_LINK_INTERFACE_LANGUAGES "C"  
  IMPORTED_LOCATION         "${ZLIB_ROOT_DIR}/lib/${LIB}${CMAKE_STATIC_LIBRARY_SUFFIX}"
  IMPORTED_LOCATION_RELEASE "${ZLIB_ROOT_DIR}/lib/${LIB}${CMAKE_STATIC_LIBRARY_SUFFIX}"
  IMPORTED_LOCATION_DEBUG   "${ZLIB_ROOT_DIR}/lib/${LIB}${DEBUG_POSTFIX}${CMAKE_STATIC_LIBRARY_SUFFIX}"
  )
add_dependencies(z libz)
#get_target_property(ZLIB_LIBRARY z IMPORTED_LOCATION)
set(ZLIB_LIBRARY z)
set(ZLIB_LIBRARIES    ${ZLIB_LIBRARY})
set(ZLIB_INCLUDE_DIRS ${ZLIB_INCLUDE_DIR})
find_package_handle_standard_args(ZLIB DEFAULT_MSG ZLIB_INCLUDE_DIR ZLIB_LIBRARY)
