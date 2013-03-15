#
# LIBND
#

include(ExternalProject)
include(FindPackageHandleStandardArgs)

if(NOT ND_LIBRARIES)
  string(REPLACE ";" "^^" SEP_GTEST_BOTH_LIBRARIES "${GTEST_BOTH_LIBRARIES}")

  set(ND_GIT_REPOSITORY ssh://git@bitbucket.org/nclack/nd.git CACHE STRING "Location of the git repository for libnd.")
  ExternalProject_Add(libnd
    GIT_REPOSITORY ${ND_GIT_REPOSITORY}
    GIT_TAG spin-off-plugins
    # UPDATE_COMMAND ""
    # UPDATE_COMMAND ${GIT_EXECUTABLE} pull origin master
    LIST_SEPARATOR ^^
    CMAKE_ARGS -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
               -DCMAKE_INSTALL_PREFIX:PATH=<INSTALL_DIR>#${CMAKE_INSTALL_PREFIX}
               -DGTEST_INCLUDE_DIR:PATH=${GTEST_INCLUDE_DIR}
               #-DGTEST_LIBRARY:STRING=${GTEST_LIBRARY}
               -DGTEST_BOTH_LIBRARIES:STRING=${SEP_GTEST_BOTH_LIBRARIES}
    )
  ExternalProject_Get_Property(libnd SOURCE_DIR)
  ExternalProject_Get_Property(libnd INSTALL_DIR)
  set(ND_INCLUDE_DIR ${SOURCE_DIR} CACHE PATH "Path to nd.h")

  add_library(nd STATIC IMPORTED)
  set_target_properties(nd PROPERTIES
    IMPORTED_LINK_INTERFACE_LANGUAGES "C"
    IMPORTED_LOCATION "${INSTALL_DIR}/lib/${CMAKE_STATIC_LIBRARY_PREFIX}nd${CMAKE_STATIC_LIBRARY_SUFFIX}"
    )
  add_dependencies(nd libnd)

  add_executable(libnd-tests IMPORTED)
  set_target_properties(libnd-tests PROPERTIES
    IMPORTED_LOCATION "${INSTALL_DIR}/bin/libnd-tests${CMAKE_EXECUTABLE_SUFFIX}"
    )
  add_dependencies(libnd-tests libnd)

  find_package(CUDA 4.0)
  
  get_property(ND_LIBRARY TARGET nd PROPERTY LOCATION)
  set(ND_INCLUDE_DIRS  ${ND_INCLUDE_DIR} ${CUDA_INCLUDE_DIRS})
  set(ND_LIBRARIES     ${ND_LIBRARY} ${CUDA_LIBRARIES})
endif()

### Handle components
foreach(package ${ND_FIND_COMPONENTS})
  find_package(${package} CONFIG PATHS .)
endforeach()

### Check everything got found
find_package_handle_standard_args(ND
  REQUIRED_VARS
    ND_LIBRARIES
    ND_INCLUDE_DIRS
  HANDLE_COMPONENTS
)

### macro for copying plugins to a target's build Location
macro(nd_copy_plugins_to_target _target)
    add_custom_command(TARGET ${_target} POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy_directory
          ${INSTALL_DIR}/bin/plugins
          $<TARGET_FILE_DIR:${_target}>/plugins
          COMMENT "Copying libnd plugins to build path for ${_target}."
          )  
    add_dependencies(${_target} libnd)
endmacro()
