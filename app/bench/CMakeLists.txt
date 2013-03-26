find_package(ND COMPONENTS ndio-hdf5)
include_directories(ND_INCLUDE_DIRS)

file(GLOB SRCS *.c *.h)

add_executable(bench ${SRCS})
target_link_libraries(bench ${ND_LIBRARIES})
nd_copy_plugins_to_target(bench ndio-hdf5 ndio-ffmpeg)
install(TARGETS bench EXPORT ndio-ffmpeg-targets DESTINATION bin)

