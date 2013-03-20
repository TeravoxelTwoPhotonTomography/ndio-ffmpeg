find_package(ND COMPONENTS ndio-hdf5)
include_directories(ND_INCLUDE_DIRS)

file(GLOB SRCS *.c *.h)

add_executable(bench ${SRCS})
target_link_libraries(bench ${ND_LIBRARIES})
install(TARGETS bench EXPORT ndio-ffmpeg-targets RUNTIME DESTINATION bin)

# Copy/install  the plugin 

set(_target bench)
get_target_property(plugin ndio-ffmpeg LOCATION)
add_custom_command(TARGET ${_target} POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E make_directory $<TARGET_FILE_DIR:${_target}>/plugins/
  COMMAND ${CMAKE_COMMAND} -E copy ${plugin}
      $<TARGET_FILE_DIR:${_target}>/plugins/.
      COMMENT "Copying ndio-ffmpeg plugin to build path for ${_target}."
      )
add_dependencies(${_target} ndio-ffmpeg)