## Find libogg

if(NOT TARGET libogg)
  include(ExternalProject)
  ExternalProject_Add(libogg
    URL      http://downloads.xiph.org/releases/ogg/libogg-1.3.0.tar.gz
    URL_MD5  0a7eb40b86ac050db3a789ab65fe21c2
    CONFIGURE_COMMAND <SOURCE_DIR>/configure --prefix=<INSTALL_DIR> --with-pic
  )
endif()

get_target_property(OGG_ROOT_DIR libogg _EP_INSTALL_DIR)

add_library(ogg IMPORTED STATIC)
add_dependencies(ogg libogg)
set_target_properties(ogg PROPERTIES
  IMPORTED_LINK_INTERFACE_LANGUAGES "C"
  IMPORTED_LOCATION  ${OGG_ROOT_DIR}/lib/${CMAKE_CFG_INTDIR}/${CMAKE_STATIC_LIBRARY_PREFIX}ogg${CMAKE_STATIC_LIBRARY_SUFFIX}
  )

set(OGG_INCLUDE_DIR ${OGG_ROOT_DIR}/include)
set(OGG_LIBRARY ogg)
#plural forms
set(OGG_INCLUDE_DIRS ${OGG_INCLUDE_DIR})
set(OGG_LIBRARIES    ${OGG_LIBRARY})

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(OGG DEFAULT_MSG
  OGG_INCLUDE_DIR
  OGG_LIBRARY
)
