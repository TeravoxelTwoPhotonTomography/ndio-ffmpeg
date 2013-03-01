# Locate libvpx libraries
#
# Requires yasm.
# Caller should define an ExternalProject target named yasm, and define
# YASM_ROOT_DIR as the install directory.
#
# This module defines
# VPX_LIBRARY, the name of the library to link against
# VPX_FOUND, if false, do not try to link
# VPX_INCLUDE_DIR, where to find header
#
include(ExternalProject)
include(FindPackageHandleStandardArgs)
if(NOT TARGET libvpx)
  ExternalProject_Add(libvpx
    DEPENDS yasm
    GIT_REPOSITORY http://git.chromium.org/webm/libvpx.git
    UPDATE_COMMAND    ""
    CONFIGURE_COMMAND
        AS=${YASM_ROOT_DIR}/bin/yasm
        <SOURCE_DIR>/configure
          --prefix=<INSTALL_DIR>
          --enable-static
          --enable-pic
          --disable-examples
          --disable-unit-tests
  )
endif()

ExternalProject_Get_Property(libvpx INSTALL_DIR)
set(VPX_INCLUDE_DIR ${INSTALL_DIR}/include)
set(VPX_LIBRARY vpx)
set(VPX_LIBRARIES vpx)
  
foreach(tgt ${VPX_LIBRARIES})
  add_library(${tgt} IMPORTED STATIC)
  add_dependencies(${tgt} libvpx)
  set_target_properties(${tgt} PROPERTIES
    IMPORTED_LINK_INTERFACE_LANGUAGES "C"
    IMPORTED_LOCATION  ${INSTALL_DIR}/lib/${CMAKE_CFG_INTDIR}/${CMAKE_STATIC_LIBRARY_PREFIX}${tgt}${CMAKE_STATIC_LIBRARY_SUFFIX}
  )
endforeach()

set(VPX_INCLUDE_DIRS ${VPX_INCLUDE_DIR})
find_package_handle_standard_args(VPX DEFAULT_MSG
  VPX_LIBRARY
  VPX_INCLUDE_DIR
)
