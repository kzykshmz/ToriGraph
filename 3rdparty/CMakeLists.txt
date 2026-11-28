
set( CMAKE_AUTOMOC OFF )
set( CMAKE_AUTOUIC OFF )
set( CMAKE_AUTORCC OFF )

# minigzip: mini gzip implementation
add_library( minigzip STATIC "minigzip/minigzip.c" )
target_link_libraries( minigzip ZLIB::ZLIB )

if (Qt5Gui_FOUND)
  get_target_property(QT_INCLUDE_DIR Qt5::Gui INTERFACE_INCLUDE_DIRECTORIES)
  message( STATUS "Qt5 GUI found ${QT_INCLUDE_DIR}" )
else()
  message( STATUS Qt5 GUI not found )
endif ()

# Qwt5
option( BUILD_QWT "Build Qwt using own code" OFF )

if( NOT BUILD_QWT )
  find_library ( QWT_LIBRARY
    NAMES qwt5-qt5 qwt-qt5
    HINTS ${QT_LIBRARY_DIR} /usr/lib64
  )
  if( NOT QWT_LIBRARY )
    set( BUILD_QWT ON CACHE BOOL "Build Qwt using own code" FORCE )
  endif()
endif()

if( NOT BUILD_QWT )
  find_path ( QWT_INCLUDE_DIR
    NAMES qwt_plot.h
    HINTS ${QT_INCLUDE_DIR} qwt5-qt5
    PATH_SUFFIXES qwt5 qwt5-qt5 src
    REQUIRED
  )
  message( STATUS "Found Qwt5: ${QWT_LIBRARY}, with include: ${QWT_INCLUDE_DIR}" )
else()
  # Build Qwt5
  message( STATUS "Qwt5: using own code" )
  set(QWT_DESIGNER off)
  add_subdirectory( qwt5-qt5 )
endif()

# QwtPlot3D
option( BUILD_QWTPLOT3D "Build QwtPlot3D using own code" ON )

if( NOT BUILD_QWTPLOT3D )
find_library ( QWTPLOT3D_LIBRARY
    NAMES qwtplot3d
    HINTS ${QT_LIBRARY_DIR}
  )
  if( NOT QWTPLOT3D_LIBRARY )
    set( BUILD_QWTPLOT3D ON CACHE BOOL "Build QwtPlot3D using own code" FORCE )
  endif()
endif()

if( NOT BUILD_QWTPLOT3D )
  find_path ( QWTPLOT3D_INCLUDE_DIR
    NAMES qwt3d_plot.h
    HINTS ${QT_INCLUDE_DIR}
    PATH_SUFFIXES qwtplot3d qwtplot3d-qt5
    REQUIRED
  )
  message( STATUS "Found QwtPlot3D: ${QWTPLOT3D_LIBRARY}, with include: ${QWTPLOT3D_INCLUDE_DIR}" )
else()
  # Build QwtPlot3D
  message( STATUS "QwtPlot3D: using own code" )
  add_subdirectory( qwtplot3d )
endif()

# liborigin
#
# The bundled liborigin (git submodule, upstream SciDAVis/liborigin) is
# hardened against truncated/corrupted .opj files by
# patches/liborigin-hardening.patch, applied here at configure time because
# the submodule itself cannot carry our commits. Idempotent: the patch is
# detected as already applied via a marker comment, so re-configuring is a
# no-op. Never fails the configure step: if git is missing or the patch does
# not apply (e.g. the submodule was updated), it only warns.
# Fuzz harness: liborigin-fuzz/ (see build.sh / fuzz.py there).
function( torigraph_apply_liborigin_hardening )
  set( _dir "${CMAKE_CURRENT_SOURCE_DIR}/liborigin" )
  set( _patch "${CMAKE_CURRENT_SOURCE_DIR}/patches/liborigin-hardening.patch" )
  set( _probe "${_dir}/OriginAnyParser.cpp" )
  set( _marker "[liborigin-hardening]" )
  set_property( DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${_patch}" )
  if( NOT EXISTS "${_patch}" OR NOT EXISTS "${_probe}" )
    message( WARNING "liborigin: hardening patch or sources missing, building unpatched liborigin" )
    return()
  endif()
  file( READ "${_probe}" _src )
  string( FIND "${_src}" "${_marker}" _pos )
  if( NOT _pos EQUAL -1 )
    message( STATUS "liborigin: hardening patch already applied" )
    return()
  endif()
  find_package( Git QUIET )
  if( NOT GIT_FOUND )
    message( WARNING "liborigin: git not found, cannot apply ${_patch}; "
                     "the .opj importer is NOT hardened against corrupted files" )
    return()
  endif()
  # GIT_CEILING_DIRECTORIES keeps git from treating liborigin/ as a sub
  # directory of the ToriGraph repository when it is not a submodule checkout
  # (git apply would then silently skip every file).
  set( _git "${CMAKE_COMMAND}" -E env "GIT_CEILING_DIRECTORIES=${CMAKE_CURRENT_SOURCE_DIR}"
       "${GIT_EXECUTABLE}" apply )
  # second attempt tolerates CRLF working trees (core.autocrlf=true on Windows)
  foreach( _opts "--verbose" "--ignore-whitespace;--verbose" )
    execute_process( COMMAND ${_git} --check ${_opts} "${_patch}"
                     WORKING_DIRECTORY "${_dir}"
                     RESULT_VARIABLE _rc OUTPUT_VARIABLE _out ERROR_VARIABLE _err )
    if( _rc EQUAL 0 )
      execute_process( COMMAND ${_git} ${_opts} "${_patch}"
                       WORKING_DIRECTORY "${_dir}"
                       RESULT_VARIABLE _rc OUTPUT_VARIABLE _out ERROR_VARIABLE _err )
      break()
    endif()
  endforeach()
  file( READ "${_probe}" _src )
  string( FIND "${_src}" "${_marker}" _pos )
  if( _rc EQUAL 0 AND NOT _pos EQUAL -1 )
    message( STATUS "liborigin: applied hardening patch ${_patch}" )
  else()
    message( WARNING "liborigin: could not apply ${_patch} (submodule changed?); "
                     "the .opj importer is NOT hardened against corrupted files.\n${_err}" )
  endif()
endfunction()

if( ORIGIN_IMPORT )
  find_package( PkgConfig )
  if( PkgConfig_FOUND )
    pkg_search_module( LIBORIGIN liborigin>=3.0.0 IMPORTED_TARGET GLOBAL )
  endif()
  if( NOT PkgConfig_FOUND OR NOT LIBORIGIN_FOUND )
    message( STATUS "liborigin: using own code" )
    torigraph_apply_liborigin_hardening()
    add_subdirectory( liborigin )
  endif()
else()
  message( STATUS "liborigin not enabled" )
endif()
