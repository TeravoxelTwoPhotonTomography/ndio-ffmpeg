# Locate libva libraries
# This module defines
# VAAPI_LIBRARY, the name of the library to link against
# VAAPI_FOUND, if false, do not try to link
# VAAPI_INCLUDE_DIR, where to find header
#
include(ExternalProject)
include(FindPackageHandleStandardArgs)
if(0)

# VAAPI requires a mountain of dependencies 
# involving certiain X.org components
# I don't know if I care to support this.
# I'll leave the code below as a reference,
# or a starting point in case I want to work
# this out later.

  if(NOT WIN32) ## Building as an external project requires autotools
    if(NOT TARGET libvaapi)
     ExternalProject_Add(libvaapi
       GIT_REPOSITORY git://anongit.freedesktop.org/git/libva
       CONFIGURE_COMMAND <SOURCE_DIR>/autogen.sh
          --prefix=<INSTALL_DIR>
          --enable-static
          --with-pic
     )
    endif()

    ExternalProject_Get_Property(libvaapi INSTALL_DIR)
    set(VAAPI_INCLUDE_DIR ${INSTALL_DIR}/include)
    set(VAAPI_LIBRARY va)
    set(VAAPI_LIBRARIES va va-drm va-glx va-tpi va-x11)
    
    foreach(tgt ${VAAPI_LIBRARIES})
      add_library(${tgt} IMPORTED STATIC)
      add_dependencies(${tgt} libvaapi)
      set_target_properties(${tgt} PROPERTIES
        IMPORTED_LINK_INTERFACE_LANGUAGES "C"
        IMPORTED_LOCATION  ${INSTALL_DIR}/lib/${CMAKE_CFG_INTDIR}/${CMAKE_STATIC_LIBRARY_PREFIX}${tgt}${CMAKE_STATIC_LIBRARY_SUFFIX}
      )
    endforeach()

    #plural forms
    set(VAAPI_INCLUDE_DIRS ${VAAPI_INCLUDE_DIR})
  endif() # NOT WIN32
endif() # --- IF 0 --- DISABLE

find_package_handle_standard_args(VAAPI DEFAULT_MSG
  VAAPI_LIBRARIES
  VAAPI_INCLUDE_DIR
)
