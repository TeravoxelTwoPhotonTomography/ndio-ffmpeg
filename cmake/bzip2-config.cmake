# Locate bz2 library  

include(ExternalProject)
include(FindPackageHandleStandardArgs)

# see the readme for windows...theres and nmake script

# We use the BIGFILES variable to inject the -fPIC option
# BIGFILES just ammends CFLAGS, so this should work even
# if the variable was intended for something else

if(NOT TARGET libbz2)
  ExternalProject_Add(libbz2
      URL               http://www.bzip.org/1.0.6/bzip2-1.0.6.tar.gz 
      URL_MD5           00b516f4704d4a7cb50a1d97e6e8e15b
      LIST_SEPARATOR    ^^
      CONFIGURE_COMMAND ""
      BUILD_COMMAND     make;BIGFILES=-fPIC
      INSTALL_COMMAND   make;install;PREFIX=<INSTALL_DIR>;BIGFILES=-fPIC
      BUILD_IN_SOURCE   TRUE
  )
endif()

ExternalProject_Get_Property(libbz2 INSTALL_DIR)
set(BZIP2_INCLUDE_DIR ${INSTALL_DIR}/include)
set(BZIP2_LIBRARY   bz2)
set(BZIP2_LIBRARIES bz2)

foreach(tgt ${BZIP2_LIBRARIES})
  add_library(${tgt} IMPORTED STATIC)
  add_dependencies(${tgt} libbz2)
  set_target_properties(${tgt} PROPERTIES
    IMPORTED_LINK_INTERFACE_LANGUAGES "C"
    IMPORTED_LOCATION  ${INSTALL_DIR}/lib/${CMAKE_CFG_INTDIR}/${CMAKE_STATIC_LIBRARY_PREFIX}${tgt}${CMAKE_STATIC_LIBRARY_SUFFIX}
  )
endforeach()

find_package_handle_standard_args(BZIP2 DEFAULT_MSG
  BZIP2_INCLUDE_DIR
  BZIP2_LIBRARY
)
