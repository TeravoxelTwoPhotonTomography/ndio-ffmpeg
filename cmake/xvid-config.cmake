# Locate xvid library  

include(ExternalProject)
include(FindPackageHandleStandardArgs)

if(NOT TARGET libxvid)
  ExternalProject_Add(libxvid
      URL               http://downloads.xvid.org/downloads/xvidcore-1.3.2.tar.gz
      CONFIGURE_COMMAND ""
      BUILD_COMMAND     make -C <SOURCE_DIR>/build/generic
      INSTALL_COMMAND   make -C <SOURCE_DIR>/build/generic install
      BUILD_IN_SOURCE   TRUE
  )
  ExternalProject_Add_Step(libxvid CUSTOM_CONFIG
    COMMAND <SOURCE_DIR>/build/generic/configure --prefix=<INSTALL_DIR> 
    COMMENT "Configuring XVID from build/generic"
    DEPENDEES configure
    DEPENDERS build
    ALWAYS    0
    WORKING_DIRECTORY <SOURCE_DIR>/build/generic
  )
endif()

ExternalProject_Get_Property(libxvid INSTALL_DIR)
set(XVID_INCLUDE_DIR ${INSTALL_DIR}/include)
set(XVID_LIBRARY   xvidcore)
set(XVID_LIBRARIES xvidcore)

foreach(tgt ${XVID_LIBRARIES})
  add_library(${tgt} IMPORTED STATIC)
  add_dependencies(${tgt} libxvid)
  set_target_properties(${tgt} PROPERTIES
    IMPORTED_LINK_INTERFACE_LANGUAGES "C"
    IMPORTED_LOCATION  ${INSTALL_DIR}/lib/${CMAKE_CFG_INTDIR}/${CMAKE_STATIC_LIBRARY_PREFIX}${tgt}${CMAKE_STATIC_LIBRARY_SUFFIX}
  )
endforeach()

find_package_handle_standard_args(XVID DEFAULT_MSG
  XVID_INCLUDE_DIR
  XVID_LIBRARY
)
