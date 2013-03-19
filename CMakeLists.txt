cmake_minimum_required(VERSION 2.8)
project(ndio-ffmpeg-plugin)

macro(show v)
  message("${v} is ${${v}}")
endmacro()
macro(showeach v)
  message("${v} is:")
  foreach(e  ${${v}})
    message("   ${e}")
  endforeach()
endmacro()
macro(show_target_prop t p)
  get_target_property(pp ${t} ${p})
  message("The ${p} of ${t} is ${pp}")
endmacro()

################################################################################
# CONFIG
################################################################################
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${PROJECT_SOURCE_DIR}/cmake)
set(ROOT_3RDPARTY_DIR ${PROJECT_SOURCE_DIR})

find_package(ND     CONFIG PATHS cmake)
find_package(FFMPEG CONFIG PATHS cmake)

include_directories(${ND_INCLUDE_DIRS})
include_directories(${FFMPEG_INCLUDE_DIR})

get_directory_property(ND_SOURCE_DIR PARENT_DIRECTORY)

##############################################################################
# SOURCE
##############################################################################

file(GLOB SRCS src/*.c)
file(GLOB HDRS src/*.h)

##############################################################################
# TARGETS
##############################################################################

add_definitions(-fPIC)
add_library(ndio-ffmpeg MODULE ${SRCS} ${HDRS})
target_link_libraries(ndio-ffmpeg ${FFMPEG_LIBRARIES} ${ND_LIBRARIES})
set_target_properties(ndio-ffmpeg PROPERTIES POSITION_INDEPENDENT_CODE TRUE)
add_dependencies(ndio-ffmpeg ffmpeg nd)

add_subdirectory(app/bench)

## Copy FFMPEG shared libs to plugin build dir so build functions in place
if(MSVC)
  foreach(cfg ${CMAKE_CONFIGURATION_TYPES})
    file(COPY ${FFMPEG_SHARED_LIBS} DESTINATION "${PROJECT_BINARY_DIR}/${cfg}") 
  endforeach(cfg)
endif()

################################################################################
#  Documentation
################################################################################

find_package(Doxygen)
if(DOXYGEN_FOUND)
  configure_file(${PROJECT_SOURCE_DIR}/doc/Doxyfile.in
                 ${PROJECT_BINARY_DIR}/doc/Doxyfile
                 @ONLY
                 )
  add_custom_target(ndio-ffmpeg-docs
    ${DOXYGEN_EXECUTABLE} ${PROJECT_BINARY_DIR}/doc/Doxyfile
    WORKING_DIRECTORY ${PROJECT_BINARY_DIR}
    COMMENT           "Generating Docs for FFMPEG IO Plugin"
    VERBATIM
    )
endif()

################################################################################
#  Install
################################################################################
foreach(_path ${NDIO_PLUGIN_PATH})
  install(TARGETS ndio-ffmpeg 
    RUNTIME DESTINATION ${_path}
    LIBRARY DESTINATION ${_path})
endforeach()
install(TARGETS ndio-ffmpeg 
  RUNTIME DESTINATION bin/plugins
  LIBRARY DESTINATION bin/plugins
)

