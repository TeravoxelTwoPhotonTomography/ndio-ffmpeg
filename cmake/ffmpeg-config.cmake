# Include this to generate and build FFMPEG
# INPUT
#   FFMPEG_GIT_REPOSITORY     has default
#   FFMPEG_GIT_BRANCH         has deefalt
#
# OUTPUT
#   FFMPEG_FOUND
#   FFMPEG_LIBRARIES
#   FFMPEG_INCLUDE_DIR

include(GenerateFFMPEG)
include(FindPackageHandleStandardArgs)

function(find_msvc_build)
  #set(prefix ${PROJECT_SOURCE_DIR}/ffmpeg/ffmpeg-20120708-git-299387e)
  set(prefix ${PROJECT_SOURCE_DIR}/ffmpeg/ffmpeg-20120630-git-3233ad4)
  if(CMAKE_CL_64)
    set(arch win64)
  else()
    set(arch win32)
  endif()
  set(dlldir                 ${prefix}-${arch}-shared/bin)
  set(libdir                 ${prefix}-${arch}-dev/lib)
  file(GLOB FFMPEG_LIBRARIES ${libdir}/*${CMAKE_STATIC_LIBRARY_SUFFIX})
  file(GLOB FFMPEG_DLLS      ${dlldir}/*${CMAKE_SHARED_LIBRARY_SUFFIX})
  set(FFMPEG_INCLUDE_DIR     ${prefix}-${arch}-dev/include PARENT_SCOPE)
  set(FFMPEG_LIBRARIES ${FFMPEG_LIBRARIES} PARENT_SCOPE)
  set(FFMPEG_DLLS      ${FFMPEG_DLLS}      PARENT_SCOPE)
endfunction(find_msvc_build)

if(MSVC)
  # Can't build FFMPEG on MSVC so use a prebuilt package
  # Zerano's FFMPEG builds are provided as 7z, which ExternalProject
  # won't unpack (as of CMake 2.8.8), so I rely on a fixed path to
  # a build commited to the repository.
  #
  find_msvc_build()
  # Copy Dll's to build locations so they'll be found during debugging
  foreach(type ${CMAKE_CONFIGURATION_TYPES})
    file(COPY ${FFMPEG_DLLS} DESTINATION ${PROJECT_BINARY_DIR}/${type})
  endforeach()
  ### INSTALL
  install(FILES ${FFMPEG_DLLS} DESTINATION bin/plugins)
else()
  GenerateFFMPEG(http://github.com/FFmpeg/FFmpeg.git n0.11.1)
endif()
find_package_handle_standard_args(FFMPEG DEFAULT_MSG
  FFMPEG_LIBRARIES
  FFMPEG_INCLUDE_DIR
)
