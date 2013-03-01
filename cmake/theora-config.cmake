# Locate libtheora libraries
# This module defines
# THEORA_LIBRARY, the name of the library to link against
# THEORA_FOUND, if false, do not try to link
# THEORA_INCLUDE_DIR, where to find header
# THEORA_INCLUDE_DIRS, paths to all required headers
#

include(ExternalProject)
include(FindPackageHandleStandardArgs)

find_package(Ogg CONFIG PATHS .) #requires libogg
if(NOT TARGET libtheora)
  ExternalProject_Add(libtheora
    DEPENDS libogg
    URL     http://downloads.xiph.org/releases/theora/libtheora-1.1.1.tar.bz2
    URL_MD5 292ab65cedd5021d6b7ddd117e07cd8e
    CONFIGURE_COMMAND <SOURCE_DIR>/configure 
        --prefix=<INSTALL_DIR>
        --with-pic
        --with-ogg=${OGG_ROOT_DIR}
        --disable-oggtest
        --disable-shared
  )
endif()

get_target_property(THEORA_ROOT_DIR libtheora _EP_INSTALL_DIR)
set(THEORA_LIBRARY theora theoraenc theoradec)
set(THEORA_INCLUDE_DIR ${THEORA_ROOT_DIR}/include)
foreach(tgt ${THEORA_LIBRARY})
  add_library(${tgt} IMPORTED STATIC)
  add_dependencies(${tgt} libtheora)
  set_target_properties(${tgt} PROPERTIES
    IMPORTED_LINK_INTERFACE_LANGUAGES "C"
    IMPORTED_LOCATION  ${THEORA_ROOT_DIR}/lib/${CMAKE_CFG_INTDIR}/${CMAKE_STATIC_LIBRARY_PREFIX}${tgt}${CMAKE_STATIC_LIBRARY_SUFFIX}
  )
endforeach()

set(THEORA_LIBRARIES ${THEORA_LIBRARY} ${OGG_LIBRARY})
set(THEORA_INCLUDE_DIRS ${THEORA_INCLUDE_DIR} ${OGG_INCLUDE_DIR})
find_package_handle_standard_args(THEORA DEFAULT_MSG
  THEORA_INCLUDE_DIR
  THEORA_LIBRARY
)
