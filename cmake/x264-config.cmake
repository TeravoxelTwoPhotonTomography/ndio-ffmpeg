# Locate libx264 library  

include(ExternalProject)
include(FindPackageHandleStandardArgs)
if(NOT TARGET libx264)
  ExternalProject_Add(libx264
    DEPENDS yasm
    URL ftp://ftp.videolan.org/pub/x264/snapshots/last_x264.tar.bz2
    #GIT_REPOSITORY https://git.videolan.org/x264.git
    CONFIGURE_COMMAND <SOURCE_DIR>/configure
          --prefix=<INSTALL_DIR>
          --enable-static
          --enable-pic
  )
endif()


ExternalProject_Get_Property(libx264 INSTALL_DIR)
set(X264_INCLUDE_DIR ${INSTALL_DIR}/include)
set(X264_LIBRARY   x264)
set(X264_LIBRARIES x264)

foreach(tgt ${X264_LIBRARIES})
  add_library(${tgt} IMPORTED STATIC)
  add_dependencies(${tgt} libx264)
  set_target_properties(${tgt} PROPERTIES
    IMPORTED_LINK_INTERFACE_LANGUAGES "C"
    IMPORTED_LOCATION  ${INSTALL_DIR}/lib/${CMAKE_CFG_INTDIR}/${CMAKE_STATIC_LIBRARY_PREFIX}${tgt}${CMAKE_STATIC_LIBRARY_SUFFIX}
  )
endforeach()

find_package_handle_standard_args(X264 DEFAULT_MSG
  X264_LIBRARY
  X264_INCLUDE_DIR
)
