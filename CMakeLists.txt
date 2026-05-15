cmake_minimum_required(VERSION 3.16)

project(MusicPlayer VERSION 0.1 LANGUAGES CXX)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets Multimedia Network)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets Multimedia Network)
find_package(PkgConfig REQUIRED)
pkg_check_modules(TAGLIB REQUIRED taglib)
pkg_check_modules(AUBIO REQUIRED aubio)

set(PROJECT_SOURCES
        main.cpp
        app/musicplayer.cpp
        app/musicplayer.h
        app/musicplayer.ui
        model/songinfo.h
        library/filescanner.cpp
        library/filescanner.h
        playback/playercontroller.cpp
        playback/playercontroller.h
        playui/playwidget.cpp
        playui/playwidget.h
        playui/playwidget.ui
        playui/turntablealbumwidget.cpp
        playui/turntablealbumwidget.h
        commands/aicontroller.h
        commands/aicontroller.cpp
        commands/command.h
        commands/localruleengine.h
        commands/localruleengine.cpp
        commands/commandvalidator.h
        commands/commandvalidator.cpp
        commands/commanddispatcher.h
        commands/commanddispatcher.cpp
        playback/volumesafety.h
        playback/volumesafety.cpp
        voice/voiceinputwidget.h
        voice/voiceinputwidget.cpp
        voice/voiceinputwidget.ui
        library/librarylistdelegate.cpp
        library/librarylistdelegate.h
        library/listwidget.cpp
        library/listwidget.h
        library/listwidget.ui
        playback/beatdetector.cpp
        playback/beatdetector.h
        beatui/beatlyricwidget.cpp
        beatui/beatlyricwidget.h
        playback/lrcparser.cpp
        playback/lrcparser.h
        lyrics/lyriclinerow.cpp
        lyrics/lyriclinerow.h
        lyrics/lyricscrollarea.cpp
        lyrics/lyricscrollarea.h
        voice/micoverlaywidget.cpp
        voice/micoverlaywidget.h
        playui/playprogressslider.cpp
        playui/playprogressslider.h
        voice/voicechatdelegate.cpp
        voice/voicechatdelegate.h
        voice/voicedrawerscrim.cpp
        voice/voicedrawerscrim.h
        library/filescanworker.cpp
        library/filescanworker.h
        resources.qrc
)

if(${QT_VERSION_MAJOR} GREATER_EQUAL 6)
    qt_add_executable(MusicPlayer
        MANUAL_FINALIZATION
        ${PROJECT_SOURCES}
    )
else()
    if(ANDROID)
        add_library(MusicPlayer SHARED
            ${PROJECT_SOURCES}
        )
    else()
        add_executable(MusicPlayer
            ${PROJECT_SOURCES}
        )
    endif()
endif()

target_link_libraries(MusicPlayer PRIVATE
    Qt${QT_VERSION_MAJOR}::Widgets
    Qt${QT_VERSION_MAJOR}::Multimedia
    Qt${QT_VERSION_MAJOR}::Network
    ${TAGLIB_LIBRARIES}
    ${AUBIO_LIBRARIES}
)

target_include_directories(MusicPlayer PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/app
    ${CMAKE_CURRENT_SOURCE_DIR}/model
    ${CMAKE_CURRENT_SOURCE_DIR}/playback
    ${CMAKE_CURRENT_SOURCE_DIR}/library
    ${CMAKE_CURRENT_SOURCE_DIR}/playui
    ${CMAKE_CURRENT_SOURCE_DIR}/lyrics
    ${CMAKE_CURRENT_SOURCE_DIR}/beatui
    ${CMAKE_CURRENT_SOURCE_DIR}/voice
    ${CMAKE_CURRENT_SOURCE_DIR}/commands
    ${TAGLIB_INCLUDE_DIRS}
    ${AUBIO_INCLUDE_DIRS}
)

if(${QT_VERSION} VERSION_LESS 6.1.0)
  set(BUNDLE_ID_OPTION MACOSX_BUNDLE_GUI_IDENTIFIER com.example.MusicPlayer)
endif()
set_target_properties(MusicPlayer PROPERTIES
    ${BUNDLE_ID_OPTION}
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
)

include(GNUInstallDirs)
install(TARGETS MusicPlayer
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

if(QT_VERSION_MAJOR EQUAL 6)
    qt_finalize_executable(MusicPlayer)
endif()
