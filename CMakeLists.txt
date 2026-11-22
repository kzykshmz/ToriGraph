cmake_minimum_required( VERSION 3.16 )

# Target platform is Windows 10
if( CMAKE_GENERATOR STREQUAL "Visual Studio 15 2017" OR
    CMAKE_GENERATOR STREQUAL "Visual Studio 14 2015" )
  set(CMAKE_SYSTEM_VERSION 10.0)
endif()

cmake_policy(SET CMP0091 NEW)

project( scidavis
  VERSION 3.0.0
  DESCRIPTION "ToriGraph is a free application for Scientific Data Analysis and Visualization."
  HOMEPAGE_URL "https://github.com/kzykshmz/ToriGraph"
  LANGUAGES CXX C)
include(GNUInstallDirs)

# ---------------------------------------------------------------------------
# Version: the project() VERSION above is the single source of truth.
# libscidavis/src/version.cpp, the Linux appdata file and the installer are
# generated from it; the Python SDK's __version__ is checked against it below
# (and by scripts/check_version.py in CI / release builds).
# Update TORIGRAPH_RELEASE_DATE (ISO 8601) together with VERSION when tagging.
# ---------------------------------------------------------------------------
set( TORIGRAPH_VERSION "${PROJECT_VERSION}" )
set( TORIGRAPH_RELEASE_DATE "2026-11-22" )
set( TORIGRAPH_EXTRA_VERSION "" )
math( EXPR TORIGRAPH_VERSION_NO
  "(${PROJECT_VERSION_MAJOR} << 16) | (${PROJECT_VERSION_MINOR} << 8) | ${PROJECT_VERSION_PATCH}" )
if( NOT TORIGRAPH_RELEASE_DATE MATCHES "^[0-9][0-9][0-9][0-9]-[0-9][0-9]-[0-9][0-9]$" )
  message( FATAL_ERROR "TORIGRAPH_RELEASE_DATE must be YYYY-MM-DD, got '${TORIGRAPH_RELEASE_DATE}'" )
endif()
# Same rule as scripts/check_version.py: first `__version__ = "x"` or 'x' line, exact match.
file( STRINGS "${CMAKE_SOURCE_DIR}/python/torigraph/__init__.py" _tg_py_version_lines
  REGEX "^__version__[ \t]*=" )
set( _tg_py_version_line "" )
set( _tg_py_version "" )
if( _tg_py_version_lines )
  list( GET _tg_py_version_lines 0 _tg_py_version_line )
  if( _tg_py_version_line MATCHES "^__version__[ \t]*=[ \t]*[\"']([^\"']+)[\"']" )
    set( _tg_py_version "${CMAKE_MATCH_1}" )
  endif()
endif()
if( NOT "${_tg_py_version}" STREQUAL "${TORIGRAPH_VERSION}" )
  message( FATAL_ERROR
    "Version mismatch: CMake project VERSION is ${TORIGRAPH_VERSION} but "
    "python/torigraph/__init__.py has '${_tg_py_version_line}'. "
    "Update both together (see scripts/check_version.py)." )
endif()
message( STATUS "ToriGraph version ${TORIGRAPH_VERSION} (${TORIGRAPH_RELEASE_DATE})" )

set( CMAKE_CXX_STANDARD 17)
set( CMAKE_CXX_STANDARD_REQUIRED TRUE )
set( CMAKE_CXX_EXTENSIONS OFF )

set( CMAKE_C_STANDARD 11)
set( CMAKE_C_STANDARD_REQUIRED TRUE )
set( CMAKE_C_EXTENSIONS OFF )

get_property( MULTI_CONFIG GLOBAL PROPERTY GENERATOR_IS_MULTI_CONFIG )

set( CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake ${CMAKE_MODULE_PATH} )

# Target platform is Windows 10
if( CMAKE_GENERATOR STREQUAL "Visual Studio 15 2017" OR
    CMAKE_GENERATOR STREQUAL "Visual Studio 14 2015" )
  if (CMAKE_VS_WINDOWS_TARGET_PLATFORM_VERSION VERSION_LESS 10)
    message(SEND_ERROR "Windows 10 SDK is not found! Please, install one or set \
      a CMAKE_WINDOWS_KITS_10_DIR environment variable to an absolute path to look \
      for Windows 10 SDKs. The directory is expected to contain Include/10.0.* directories.")
  else()
    message(STATUS "Use Windows 10 SDK version ${CMAKE_VS_WINDOWS_TARGET_PLATFORM_VERSION}")
  endif()
endif()

find_package( Qt5
  COMPONENTS
  Core
  Gui
  Widgets
  Svg
  Network
  PrintSupport
  Xml
  OpenGL
  LinguistTools
  REQUIRED
  )

set( CMAKE_AUTOMOC ON )
set( CMAKE_AUTOUIC ON )
set( CMAKE_AUTORCC ON )

if( MSVC )
  # /wd4456 /wd4457 /wd4458 Silent "declaration of %1 hides %2 ..."
  # /wd4251 Silent dll-related warnings
  # /wd4127 Silent conditional expression is constant (Qt headers)
  # /wd4310 Silent cast truncates constant value (muParser headers)
  # /wd4996 strcpy is unsafe warnings, equiv to #define _SCL_SECURE_NO_WARNINGS
  set( CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /utf-8 /EHsc /W4 /permissive- \
	 /wd4456 /wd4457 /wd4458 /wd4251 /wd4127 /wd4310 /wd4996" )
  set( CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /utf-8 /W4 /permissive-" )
  if( CMAKE_CXX_COMPILER_ID STREQUAL "Clang" )
    set( CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-nonportable-include-path" )
  else()
    add_compile_options( "/MP" )
  endif()
else()
  set( CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fexceptions -Wall -Wextra -pedantic" )
  set( CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -pedantic" )
endif()

# Guard against a garbled MSVC /showIncludes prefix with the Ninja generator.
# On Japanese Windows, CMake can record the localized prefix
# ("Note: including file:" in Japanese) as UTF-8 text that was mis-decoded as
# CP932. Ninja then never matches cl's output, records zero header dependencies
# and stops recompiling .cpp files when a header changes. The resulting mix of
# old and new class layouts crashes at run time (seen as a segfault in
# scidavis_ole_test Suite 51). The garbled form always contains U+7E5D or
# U+7E3A (UTF-8 e7b99d / e7b8ba), which never occur in a correct prefix.
if( MSVC AND CMAKE_GENERATOR MATCHES "Ninja" AND CMAKE_CL_SHOWINCLUDES_PREFIX )
  set( _tg_prefix_file "${CMAKE_BINARY_DIR}/CMakeFiles/torigraph_showincludes_prefix.txt" )
  file( WRITE "${_tg_prefix_file}" "${CMAKE_CL_SHOWINCLUDES_PREFIX}" )
  file( READ "${_tg_prefix_file}" _tg_prefix_hex HEX )
  if( _tg_prefix_hex MATCHES "e7b99d|e7b8ba" )
    message( FATAL_ERROR
      "The MSVC /showIncludes prefix detected by CMake is garbled "
      "(CMAKE_CL_SHOWINCLUDES_PREFIX). Ninja would record no header "
      "dependencies, so header edits would not trigger rebuilds and the "
      "binaries would crash. Delete ${CMAKE_BINARY_DIR}/CMakeFiles/${CMAKE_VERSION}* "
      "(or the whole build directory) and configure again from cmd.exe "
      "after vcvars64.bat, then rebuild everything." )
  endif()
endif()

if( APPLE )
  add_compile_definitions( GL_SILENCE_DEPRECATION )
endif()

# Searching for updates
option( SEARCH_FOR_UPDATES "Enable Searching for updates" ON )

# Download links
option( DOWNLOAD_LINKS "Enable Download links" ON )

if( SEARCH_FOR_UPDATES OR DOWNLOAD_LINKS )
  find_package( Qt5 COMPONENTS Network REQUIRED )
endif()

option( SCRIPTING_MUPARSER "Enable muParser Scripting" ON )

option( BUILD_TESTS "Build tests" OFF )

option( ORIGIN_IMPORT "Enable importing OriginLab project files" OFF )

# GSL
find_package( GSL REQUIRED )

# ZLIB
find_package( ZLIB "1.2.11" REQUIRED )

# OpenGL
find_package( OpenGL COMPONENTS OpenGL REQUIRED )

# muParser
if( SCRIPTING_MUPARSER )
find_library( MUPARSER_LIB
  NAMES muparser
  REQUIRED
  )
find_path( MUPARSER_INCLUDE_DIR
  NAMES "muParser.h"
  REQUIRED
  )
message( STATUS "Found muParser : ${MUPARSER_LIB} include: ${MUPARSER_INCLUDE_DIR}" )
endif()

add_subdirectory( 3rdparty )

add_subdirectory( libscidavis )

add_subdirectory( scidavis )

add_subdirectory( fitPlugins )

if( BUILD_TESTS )
  enable_testing()
  add_subdirectory( test )
endif()

# Documentation
set( DOC_FILES
  ChangeLog.md
  README.md
  LICENSE
  LICENSE-GPL-3.0.txt
  THIRD_PARTY_LICENSES.md
  )

if( WIN32 )
  install( FILES ${DOC_FILES} DESTINATION . )
else()
  install( FILES ${DOC_FILES} DESTINATION ${CMAKE_INSTALL_DOCDIR} )
endif()

# F1 quick guide. The destinations must match quickGuideFilePath() in
# libscidavis/src/UxNavigation.cpp: <exe dir>/manual on Windows, and
# <exe dir>/../share/doc/torigraph/manual next to bin/ elsewhere.
if( WIN32 )
  install( DIRECTORY manual/ DESTINATION manual )
else()
  install( DIRECTORY manual/ DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/doc/torigraph/manual )
endif()

if( MSVC )
  set_property(DIRECTORY PROPERTY VS_STARTUP_PROJECT "scidavis")
endif()

set( CPACK_PACKAGE_NAME "ToriGraph" )
set( CPACK_PACKAGE_VENDOR "ToriGraph" )
set( CPACK_PACKAGE_DESCRIPTION "ToriGraph Installer" )
# Binary packages are GPL-3.0-or-later (see THIRD_PARTY_LICENSES.md).
set( CPACK_RESOURCE_FILE_LICENSE "${CMAKE_SOURCE_DIR}/LICENSE-GPL-3.0.txt" )
set( CPACK_PACKAGE_EXECUTABLES "ToriGraph;ToriGraph" )
set( CPACK_CREATE_DESKTOP_LINKS "ToriGraph" )
set( CPACK_STRIP_FILES ON )

# Source packaging
set( CPACK_SOURCE_IGNORE_FILES "/build/;CMakeLists\.txt\.user*;/\.git/" )

# WIX
set( CPACK_WIX_UPGRADE_GUID "d6f4ef98-3744-47a2-b581-d789db8a4d63" )
set( CPACK_WIX_PRODUCT_ICON "${CMAKE_SOURCE_DIR}/scidavis/icons/scidavis.ico" )

# NSIS
set( CPACK_NSIS_MUI_ICON "${CMAKE_SOURCE_DIR}/scidavis/icons/scidavis.ico" )
set( CPACK_NSIS_ENABLE_UNINSTALL_BEFORE_INSTALL ON )
set( CPACK_NSIS_EXECUTABLES_DIRECTORY . )

include(CPack)
