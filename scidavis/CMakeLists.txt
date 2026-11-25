set( RESOURCES
  "appicons.qrc"
  "icons.qrc"
  )

if( WIN32 )
  list( APPEND RESOURCES "scidavis.rc" )
endif()

# Translation Files
# Copying these into the build directory is intended to prevent these
# files from being overwritten by lupdate
file( REMOVE_RECURSE ./translations-copy)
# ToriGraph ships English (source strings) and Japanese only.
set(languages ja)
foreach(i ${languages})
   file( COPY translations/scidavis_${i}.ts DESTINATION ./translations-copy )
   list(APPEND TS_FILES "${CMAKE_CURRENT_BINARY_DIR}/translations-copy/scidavis_${i}.ts")
endforeach()

qt5_create_translation( QM_FILES "${CMAKE_SOURCE_DIR}/libscidavis" ${TS_FILES} )

#set( MACOSX_BUNDLE_PLIST scidavis/scidavis.app/Contents/Info.plist )
#set( MACOSX_BUNDLE_ICON scidavis/scidavis.app/Contents/Resources/scidavis.icns )

add_executable( scidavis
  WIN32
#  MACOSX_BUNDLE
  "src/main.cpp"
  "${RESOURCES}"
  "${QM_FILES}"
  )
set_target_properties( scidavis PROPERTIES OUTPUT_NAME "ToriGraph" )

# SciDAVis expects qm files to be in the translations directory. qt5_create_translation puts them somewhere else
add_custom_command(TARGET scidavis POST_BUILD COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_CURRENT_BINARY_DIR}/translations" COMMAND ${CMAKE_COMMAND} -E copy_if_different ${QM_FILES} "${CMAKE_CURRENT_BINARY_DIR}/translations")
if( WIN32 )
  add_custom_command(TARGET scidavis POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_FILE:scidavis>" "${CMAKE_CURRENT_BINARY_DIR}/scidavis.exe")
  file(GLOB TORIGRAPH_RUNTIME_DLLS
    "${CMAKE_SOURCE_DIR}/.deps/Library/bin/libomp.dll"
    "${CMAKE_SOURCE_DIR}/.deps/Library/bin/libiomp5md.dll"
    "${CMAKE_SOURCE_DIR}/.deps/Library/bin/tbb12.dll"
    "${CMAKE_SOURCE_DIR}/.deps/Library/bin/zlib1.dll"
    "${CMAKE_SOURCE_DIR}/.deps/Library/bin/zlib.dll"
    "${CMAKE_SOURCE_DIR}/.deps/Library/bin/freetype.dll"
    "${CMAKE_SOURCE_DIR}/.deps/Library/bin/harfbuzz.dll"
    "${CMAKE_SOURCE_DIR}/.deps/Library/bin/graphite2.dll"
    "${CMAKE_SOURCE_DIR}/.deps/Library/bin/glib-2.0-0.dll"
    "${CMAKE_SOURCE_DIR}/.deps/Library/bin/intl-8.dll"
    "${CMAKE_SOURCE_DIR}/.deps/Library/bin/iconv.dll"
    "${CMAKE_SOURCE_DIR}/.deps/Library/bin/charset.dll"
    "${CMAKE_SOURCE_DIR}/.deps/Library/bin/pcre2-8.dll"
    "${CMAKE_SOURCE_DIR}/.deps/Library/bin/pcre2-16.dll"
    "${CMAKE_SOURCE_DIR}/.deps/Library/bin/icudt*.dll"
    "${CMAKE_SOURCE_DIR}/.deps/Library/bin/icuin*.dll"
    "${CMAKE_SOURCE_DIR}/.deps/Library/bin/icuuc*.dll"
    "${CMAKE_SOURCE_DIR}/.deps/Library/bin/liblzma.dll"
    "${CMAKE_SOURCE_DIR}/.deps/Library/bin/libzstd.dll"
    "${CMAKE_SOURCE_DIR}/.deps/Library/bin/zstd.dll"
    "${CMAKE_SOURCE_DIR}/.deps/Library/bin/libbz2.dll"
    "${CMAKE_SOURCE_DIR}/.deps/Library/bin/libexpat.dll"
    "${CMAKE_SOURCE_DIR}/.deps/Library/bin/sqlite3.dll"
    "${CMAKE_SOURCE_DIR}/.deps/Library/bin/turbojpeg.dll"
    "${CMAKE_SOURCE_DIR}/.deps/Library/bin/jpeg8.dll"
    "${CMAKE_SOURCE_DIR}/.deps/Library/bin/libpng16.dll"
    "${CMAKE_SOURCE_DIR}/.deps/Library/bin/gsl-28.dll"
    "${CMAKE_SOURCE_DIR}/.deps/Library/bin/libcblas.dll"
    "${CMAKE_SOURCE_DIR}/.deps/Library/bin/libblas.dll"
    # Local developer environments may use an MKL-backed libblas/libcblas, which loads
    # mkl_rt at run time. Copy it so build/scidavis runs; release packages refuse MKL
    # (scripts/build-windows-installer.ps1) and CI uses netlib BLAS.
    "${CMAKE_SOURCE_DIR}/.deps/Library/bin/mkl_*.dll"
    "${CMAKE_SOURCE_DIR}/.deps/Library/bin/muparser.dll"
    "${CMAKE_SOURCE_DIR}/.deps/Library/bin/libcrypto-3-x64.dll"
    "${CMAKE_SOURCE_DIR}/.deps/Library/bin/libssl-3-x64.dll"
    "${CMAKE_SOURCE_DIR}/.deps/Library/bin/vcruntime140*.dll"
    "${CMAKE_SOURCE_DIR}/.deps/Library/bin/msvcp140*.dll"
    "${CMAKE_SOURCE_DIR}/.deps/Library/bin/vcomp140.dll"
    "${CMAKE_SOURCE_DIR}/.deps/Library/bin/concrt140.dll"
    "${CMAKE_SOURCE_DIR}/.deps/Library/bin/Qt5Core_conda.dll"
    "${CMAKE_SOURCE_DIR}/.deps/Library/bin/Qt5Gui_conda.dll"
    "${CMAKE_SOURCE_DIR}/.deps/Library/bin/Qt5Widgets_conda.dll"
    "${CMAKE_SOURCE_DIR}/.deps/Library/bin/Qt5Svg_conda.dll"
    "${CMAKE_SOURCE_DIR}/.deps/Library/bin/Qt5Network_conda.dll"
    "${CMAKE_SOURCE_DIR}/.deps/Library/bin/Qt5PrintSupport_conda.dll"
    "${CMAKE_SOURCE_DIR}/.deps/Library/bin/Qt5Xml_conda.dll"
    "${CMAKE_SOURCE_DIR}/.deps/Library/bin/Qt5OpenGL_conda.dll"
    "${CMAKE_SOURCE_DIR}/.deps/Library/bin/Qt5Pdf_conda.dll"
  )
  if( TORIGRAPH_RUNTIME_DLLS )
    add_custom_command(TARGET scidavis POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy_if_different ${TORIGRAPH_RUNTIME_DLLS} "${CMAKE_CURRENT_BINARY_DIR}"
    )
  endif()
  if( TORIGRAPH_ASAN_RUNTIME_DLL )
    add_custom_command(TARGET scidavis POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy_if_different "${TORIGRAPH_ASAN_RUNTIME_DLL}" "${CMAKE_CURRENT_BINARY_DIR}"
    )
  endif()
endif()

if( WIN32 )
  target_compile_definitions( scidavis PUBLIC
    TS_PATH="${CMAKE_INSTALL_PREFIX}/translations"
    DOC_PATH="${CMAKE_INSTALL_PREFIX}"
    PLUGIN_PATH="${CMAKE_INSTALL_PREFIX}/fitPlugins"
    )
else()
  target_compile_definitions( scidavis PUBLIC
     TS_PATH="${CMAKE_INSTALL_DATAROOTDIR}/scidavis/translations"
     DOC_PATH="${CMAKE_INSTALL_DOCDIR}/scidavis"
     PLUGIN_PATH="${CMAKE_INSTALL_LIBDIR}/scidavis/plugins"
    )
endif()

target_link_libraries( scidavis libscidavis )

if( MULTI_CONFIG )
  target_include_directories( scidavis PRIVATE
    "${CMAKE_BINARY_DIR}/libscidavis/libscidavis_autogen/include_$<CONFIG>"
    )
else()
  target_include_directories( scidavis PRIVATE
    "${CMAKE_BINARY_DIR}/libscidavis/libscidavis_autogen/include"
    )
endif()

if (NOT CMAKE_CROSS_COMPILING)
    if( WIN32 )
      install( TARGETS scidavis RUNTIME DESTINATION . )
      install( FILES ${QM_FILES} DESTINATION translations )
    else()
      install( TARGETS scidavis RUNTIME DESTINATION bin  BUNDLE DESTINATION .)
      install( FILES ${QM_FILES}  DESTINATION share/scidavis/translations )
      install( SCRIPT bundle.cmake)
      
endif()
    
    if( RUNTIME_DIR )
      set( EXCLUDE_DLL "system32/*" "shell32*" "kernel32*" )
      install( CODE "
        file( GET_RUNTIME_DEPENDENCIES
          RESOLVED_DEPENDENCIES_VAR RESOLVED_DEPENDENCIES
          UNRESOLVED_DEPENDENCIES_VAR UNRESOLVED_DEPENDENCIES
          CONFLICTING_DEPENDENCIES_PREFIX CONFLICTING_DEPENDENCIES
          EXECUTABLES \"\${CMAKE_INSTALL_PREFIX}/ToriGraph.exe\"
          DIRECTORIES \"${RUNTIME_DIR}\"
          POST_EXCLUDE_REGEXES ${EXCLUDE_DLL}
          )
        foreach( lib \${RESOLVED_DEPENDENCIES} )
          execute_process(
            COMMAND \"${CMAKE_COMMAND}\" -E
            copy \${lib} \"\${CMAKE_INSTALL_PREFIX}\"
            )
        endforeach()
        ")
    endif()
    
    if( WIN32 )
      set( CMAKE_INSTALL_SYSTEM_RUNTIME_DESTINATION . )
      include(Windeployqt)
      windeployqt( scidavis . )
    endif()
    
    # Icons
    
    if( UNIX )
      install( FILES icons/scidavis.svg DESTINATION share/icons/hicolor/scalable/apps )
    
      foreach( res IN ITEMS 16 22 32 48 64 128 )
        install( FILES icons/hicolor-${res}/scidavis.png
          DESTINATION share/icons/hicolor/${res}x${res}/apps )
      endforeach()
    
      foreach( res IN ITEMS 16 22 32 )
        install( FILES icons/locolor-${res}/scidavis.png
          DESTINATION share/icons/locolor/${res}x${res}/apps )
      endforeach()
    endif()
    
    # Application Files
    if( UNIX )
      install( FILES scidavis.desktop DESTINATION share/applications )
      install( FILES scidavis.xml DESTINATION share/mime/packages )
      install( FILES scidavis.1 DESTINATION share/man/man1 )
      configure_file( scidavis.appdata.xml.in "${CMAKE_CURRENT_BINARY_DIR}/scidavis.appdata.xml" @ONLY )
      install( FILES "${CMAKE_CURRENT_BINARY_DIR}/scidavis.appdata.xml" DESTINATION share/metainfo )
      install( FILES x-sciprj.desktop DESTINATION share/mimelnk/application )
    endif()
    
endif()

if( WIN32 )
  set( OLE_TEST_SRCS
    "src/ole_test.cpp"
    "src/ole_test_suites_36_46.cpp"
    "src/ole_test_suites_47_52.cpp"
    "src/ole_test_suites_53_63.cpp"
    "src/ole_test_suites_64_72.cpp"
    "src/ole_test_suites_73_78.cpp"
    "src/ole_test_suites_79.cpp"
    "src/ole_test_suites_80.cpp"
    "src/ole_test_suites_81.cpp"
    "src/ole_test_suites_82.cpp"
    "src/ole_test_suites_83.cpp"
    "src/ole_test_suites_84.cpp"
    "src/ole_test_suites_85.cpp"
    "src/ole_test_suites_86.cpp"
    "src/ole_test_suites_87.cpp"
    "src/ole_test_suites_88.cpp"
    "src/ole_test_suites_89.cpp"
    "src/ole_test_suites_90.cpp"
    "src/ole_test_suites_91.cpp"
    "src/ole_test_suites_92.cpp"
    "src/ole_test_suites_93.cpp"
    "src/ole_test_suites_94.cpp"
    "src/ole_test_suites_95.cpp"
    "src/ole_test_suites_96.cpp"
    "src/ole_test_suites_97.cpp"
    "src/ole_test_common.h"
  )
  add_executable( scidavis_ole_test ${OLE_TEST_SRCS} "${RESOURCES}" )
  target_link_libraries( scidavis_ole_test libscidavis )
  target_include_directories( scidavis_ole_test PRIVATE "${CMAKE_BINARY_DIR}/libscidavis/libscidavis_autogen/include" "${CMAKE_SOURCE_DIR}/libscidavis/src" )
  if( TORIGRAPH_ENABLE_PCH )
    target_precompile_headers( scidavis_ole_test PRIVATE
      "$<$<COMPILE_LANGUAGE:CXX>:${CMAKE_CURRENT_SOURCE_DIR}/src/ole_test_common.h>"
    )
  endif()
  if( TORIGRAPH_RUNTIME_DLLS )
    add_custom_command(TARGET scidavis_ole_test POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy_if_different ${TORIGRAPH_RUNTIME_DLLS} "${CMAKE_CURRENT_BINARY_DIR}"
    )
  endif()
  if( TORIGRAPH_ASAN_RUNTIME_DLL )
    add_custom_command(TARGET scidavis_ole_test POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy_if_different "${TORIGRAPH_ASAN_RUNTIME_DLL}" "${CMAKE_CURRENT_BINARY_DIR}"
    )
  endif()

  # Register modular regression suites (36..97, note: 54 was never allocated) with CTest
  foreach( _sid RANGE 36 97 )
    if( NOT _sid EQUAL 54 )
      add_test(
        NAME "ole_suite_${_sid}"
        COMMAND scidavis_ole_test "--suite=${_sid}"
      )
      set_tests_properties( "ole_suite_${_sid}" PROPERTIES
        ENVIRONMENT "QT_OPENGL=software;TORIGRAPH_NO_OLE_REGISTER=1"
        RESOURCE_LOCK "win32_ole_clipboard"
      )
    endif()
  endforeach()
endif()