find_package(ND COMPONENTS ndio-hdf5 CONFIG PATHS cmake)
if(UNIX AND NOT APPLE)
  set(EXTRA_LIBS rt)
endif()
if(ND_FOUND) # if a component is missing this will be false
  include_directories(ND_INCLUDE_DIRS)
  file(GLOB SRCS *.c *.h)
  add_executable(bench-ffmpeg ${SRCS})
  target_link_libraries(bench-ffmpeg ${ND_LIBRARIES} ${EXTRA_LIBS})
  add_dependencies(bench-ffmpeg ndio-hdf5)
  nd_copy_plugins_to_target(bench-ffmpeg ndio-hdf5 ndio-ffmpeg)
  install(TARGETS bench-ffmpeg EXPORT ndio-ffmpeg-targets DESTINATION bin/test)
endif()
