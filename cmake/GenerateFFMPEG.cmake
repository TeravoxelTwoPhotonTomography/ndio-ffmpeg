# Defines functions for building FFMPEG.
#
# GenerateFFMPEG(GIT_URL GIT_TAG)
#   - Example:
#
#         GenerateFFMPEG(https://github.com/FFmpeg/FFmpeg.git n0.11.1)
#
#   - Makes an ExternalProject that downloads and builds FFMPEG 
#     together with it's dependencies.  Currently, this emphasises
#     common 3rd-party video codecs; audio codecs are neglected but
#     could be added.
#
#   - Exports the following:
#       target:     ffmpeg
#       variable:   FFMPEG_LIBRARIES
#       variable:   FFMPEG_INCLUDE_DIR
#
#
# Author: Nathan Clack
# Date: July 2012

include(FeatureSummary)
include(ExternalProject)

##
# Modifies FFMPEG_DEP_LIBRARIES, _ffmpeg_conf, _ffmpeg_paths, and _ffmpeg_deps.
# Adds libraries and configure lines if a library is found.
# If <confname> is false, will only add include directories and feature description.
#   The library won't be "enabled" or "disabled" in the FFMPEG config.
macro(_ffmpeg_maybe_add name confname description url)
  find_package(${name} CONFIG PATHS cmake)
  set_package_properties(${name} PROPERTIES
    DESCRIPTION ${description}
    URL         ${url}
  )
  string(TOUPPER ${name} uname)
  add_feature_info(${name} ${uname}_FOUND ${description})
  if(${uname}_FOUND)
    include_directories(${uname}_INCLUDE_DIR)
    set(_ffmpeg_conf ${_ffmpeg_conf} --enable-${confname})
    if(${uname}_INCLUDE_DIRS)
      foreach(dir ${${uname}_INCLUDE_DIRS})
        set(_ffmpeg_paths ${_ffmpeg_paths} --extra-cflags=-I${dir})
      endforeach()
    else()
      set(_ffmpeg_paths ${_ffmpeg_paths} --extra-cflags=-I${${uname}_INCLUDE_DIR})
    endif()
    if(TARGET ${confname})
      set(_ffmpeg_deps ${_ffmpeg_deps} ${confname}) #External project targets must correspond to confname's
    endif()
    if(${uname}_LIBRARIES)
      set(FFMPEG_DEP_LIBRARIES ${FFMPEG_DEP_LIBRARIES} ${${uname}_LIBRARIES})
    else()
      set(FFMPEG_DEP_LIBRARIES ${FFMPEG_DEP_LIBRARIES} ${${uname}_LIBRARY})
    endif()
  else()
      set(_ffmpeg_conf ${_ffmpeg_conf} --disable-${confname})
  endif()
endmacro(_ffmpeg_maybe_add)

#####
# Example:
#   FFMPEG_FIND(avcodec)
# Input:
#   FFMPEG_ROOT_DIR
# Modifies
#   FFMPEG_LIBRARIES

macro(FFMPEG_FIND name)
  #find_library(FFMPEG_${name}_LIBRARY ${name}) # -- uncomment to enable use of installed system libraries
  if(CMAKE_SYSTEM_NAME MATCHES Linux) #use shared (can't get fPIC to work)
    #get_filename_component(_loc
    #  "${FFMPEG_ROOT_DIR}/lib/${CMAKE_SHARED_LIBRARY_PREFIX}${name}${CMAKE_SHARED_LIBRARY_SUFFIX}" REALPATH)
    set(_loc
      "${FFMPEG_ROOT_DIR}/lib/${CMAKE_SHARED_LIBRARY_PREFIX}${name}${CMAKE_SHARED_LIBRARY_SUFFIX}")
    add_library(lib${name} SHARED IMPORTED GLOBAL)
    set_target_properties(lib${name} PROPERTIES IMPORTED_LOCATION ${_loc})
    add_dependencies(lib${name} ffmpeg)
    set(FFMPEG_${name}_LIBRARY lib${name})
    list(APPEND FFMPEG_SHARED_LIBS lib${name})
    file(GLOB _files ${_loc}*)
    install(FILES ${_files} DESTINATION bin/plugins)
  else() #use static
    SET(FFMPEG_${name}_LIBRARY
      ${FFMPEG_ROOT_DIR}/lib/${CMAKE_STATIC_LIBRARY_PREFIX}${name}${CMAKE_STATIC_LIBRARY_SUFFIX})
  endif()
  set(FFMPEG_LIBRARIES ${FFMPEG_LIBRARIES} ${FFMPEG_${name}_LIBRARY})
endmacro()

#####
function(GenerateFFMPEG GIT_URL GIT_TAG)
  set(FFMPEG_GIT_REPOSITORY ${GIT_URL})
  set(FFMPEG_GIT_BRANCH     ${GIT_TAG})
  ##
  ## REQUIRED DEPENDENCIES
  ##
  ExternalProject_Add(yasm
    URL               http://www.tortall.net/projects/yasm/releases/yasm-1.2.0.tar.gz
    URL_MD5           4cfc0686cf5350dd1305c4d905eb55a6
    CONFIGURE_COMMAND <SOURCE_DIR>/configure --prefix=<INSTALL_DIR>
    )
  get_target_property(YASM_ROOT_DIR yasm _EP_INSTALL_DIR)

  if(APPLE) #FFMPEG may rely on these Frameworks
    find_library(CF_LIBS  CoreFoundation)
    find_library(VDA_LIBS VideoDecodeAcceleration)
    find_library(CV_LIBS  CoreVideo)
    set(FFMPEG_LIBRARIES ${FFMPEG_LIBRARIES} ${CF_LIBS} ${VDA_LIBS} ${CV_LIBS})
  endif()

  ##
  ## OPTIONAL DEPENDENCIES
  ##
  _ffmpeg_maybe_add(ZLIB   zlib      "A Massively Spiffy Yet Delicately Unobtrusive Compression Library" http://zlib.net)
  _ffmpeg_maybe_add(BZip2  bzlib     "A freely available, patent free, high-quality data compressor."    http://www.bzip.org)
  _ffmpeg_maybe_add(x264   libx264   "A free library for encoding videos streams into the H.264/MPEG-4 AVC format" http://www.videolab.org/developers/x264.html)
  _ffmpeg_maybe_add(theora libtheora "Video compression for the OGG format from Xiph.org" http://www.theora.org)
  _ffmpeg_maybe_add(vaapi  vaapi     "Enables hardware accelerated video decode/encode for prevailing standard formats." http://www.freedesktop.org/wiki/Software/vaapi)
  _ffmpeg_maybe_add(VPX    libvpx    "An open, royalty-free, media file format for the web." http://www.webmproject.org/code)
  _ffmpeg_maybe_add(xvid   libxvid   "The XVID video codec." http://www.xvid.org)

  #if(X264_FOUND) #X264 requires gpl
  #  set(_ffmpeg_conf ${_ffmpeg_conf} --enable-gpl)
  #endif()

  ## Finish up - must be after _ffmpeg_maybe_add section
  #  Add library paths for each library to config's cflags
  foreach(lib ${FFMPEG_DEP_LIBRARIES})
      get_target_property(loc ${lib} LOCATION)
      get_filename_component(dir ${loc} PATH)
      list(APPEND _ffmpeg_paths  --extra-ldflags=-L${dir})
  endforeach()
  list(REMOVE_DUPLICATES _ffmpeg_paths)

  ##
  ## EXTERNAL PROJECT CALL
  ##
  ExternalProject_Add(ffmpeg
    DEPENDS           ${_ffmpeg_deps} yasm
    GIT_REPOSITORY    ${FFMPEG_GIT_REPOSITORY}
    GIT_TAG           ${FFMPEG_GIT_BRANCH}
    UPDATE_COMMAND    ""
    CONFIGURE_COMMAND 
          <SOURCE_DIR>/configure
            --prefix=<INSTALL_DIR>
            --yasmexe=${YASM_ROOT_DIR}/bin/yasm
            --enable-gpl
            --enable-pic
            --disable-symver
            --enable-shared
            --enable-hardcoded-tables
            --enable-runtime-cpudetect
            --enable-version3
            --extra-cflags=-g
            ${_ffmpeg_conf}
            ${_ffmpeg_paths}
  )
  get_target_property(FFMPEG_ROOT_DIR ffmpeg _EP_INSTALL_DIR)

  set(FFMPEG_INCLUDE_DIR ${FFMPEG_ROOT_DIR}/include CACHE PATH "Location of FFMPEG headers.")
  FFMPEG_FIND(avformat)
  FFMPEG_FIND(avdevice)
  FFMPEG_FIND(avcodec)
  FFMPEG_FIND(avutil)
  FFMPEG_FIND(swscale)
  FFMPEG_FIND(avfilter)
  FFMPEG_FIND(postproc)
  FFMPEG_FIND(swresample)

  ##
  ## OUTPUT
  ##
  PRINT_ENABLED_FEATURES()
  PRINT_DISABLED_FEATURES()
  set(FFMPEG_ROOT_DIR    ${FFMPEG_ROOT_DIR} CACHE PATH "Path to root of ffmpeg installation.")
  set(FFMPEG_LIBRARIES   ${FFMPEG_LIBRARIES} ${FFMPEG_DEP_LIBRARIES} PARENT_SCOPE)
  set(FFMPEG_SHARED_LIBS ${FFMPEG_SHARED_LIBS} PARENT_SCOPE)
  set(FFMPEG_INCLUDE_DIR ${FFMPEG_INCLUDE_DIR} PARENT_SCOPE)
endfunction(GenerateFFMPEG)

