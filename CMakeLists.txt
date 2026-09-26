cmake_minimum_required(VERSION 3.21)

project(CopperIPTVPlayer
    VERSION 0.1.0
    DESCRIPTION "Copper IPTV Player - a modern IPTV player for Windows"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON)

find_package(Qt6 6.2 REQUIRED COMPONENTS
    Core
    Gui
    Widgets
    Network
    Multimedia
    MultimediaWidgets
    Sql
    Concurrent
)

qt_standard_project_setup()

set(APP_ORG "CopperSoftware")
set(APP_NAME "CopperIPTVPlayer")
set(APP_DISPLAY_NAME "Copper IPTV Player")
set(APP_VERSION ${PROJECT_VERSION})

# Placeholder, intentionally empty so no repository URL is invented.
# Override from the command line if a real project URL should be baked in:
#   -DAPP_GITHUB_URL=https://...  -DAPP_DOCS_URL=https://...
set(APP_GITHUB_URL "" CACHE STRING "GitHub repository URL shown in About/Help")
set(APP_DOCS_URL "" CACHE STRING "Documentation URL shown in Help")

configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core/AppInfo.h.in
    ${CMAKE_CURRENT_BINARY_DIR}/generated/core/AppInfo.h
    @ONLY
)

set(COPPER_CORE_SOURCES
    src/core/AppSettings.cpp
    src/core/AppSettings.h
    src/core/Constants.h
    src/core/Logger.cpp
    src/core/Logger.h

    src/database/DatabaseManager.cpp
    src/database/DatabaseManager.h

    src/models/Channel.cpp
    src/models/Channel.h
    src/models/ChannelGroup.cpp
    src/models/ChannelGroup.h
    src/models/Playlist.cpp
    src/models/Playlist.h

    src/network/NetworkManager.cpp
    src/network/NetworkManager.h
    src/network/PlaylistDownloader.cpp
    src/network/PlaylistDownloader.h
    src/network/ChannelLogoLoader.cpp
    src/network/ChannelLogoLoader.h

    src/playlist/M3UParser.cpp
    src/playlist/M3UParser.h
    src/playlist/PlaylistManager.cpp
    src/playlist/PlaylistManager.h

    src/player/IPTVPlayer.cpp
    src/player/IPTVPlayer.h
    src/player/PlaybackController.cpp
    src/player/PlaybackController.h
)

set(COPPER_APP_SOURCES
    src/main.cpp

    src/app/Application.cpp
    src/app/Application.h

    src/ui/MainWindow.cpp
    src/ui/MainWindow.h
    src/ui/ControlBar.cpp
    src/ui/ControlBar.h
    src/ui/Themes.cpp
    src/ui/Themes.h

    src/ui/models/ChannelFilterProxyModel.cpp
    src/ui/models/ChannelFilterProxyModel.h
    src/ui/models/ChannelListModel.cpp
    src/ui/models/ChannelListModel.h
    src/ui/models/ChannelItemDelegate.cpp
    src/ui/models/ChannelItemDelegate.h
    src/ui/models/GroupListModel.cpp
    src/ui/models/GroupListModel.h
    src/ui/models/PlaylistListModel.cpp
    src/ui/models/PlaylistListModel.h

    src/ui/widgets/ChannelListWidget.cpp
    src/ui/widgets/ChannelListWidget.h
    src/ui/widgets/GroupListWidget.cpp
    src/ui/widgets/GroupListWidget.h
    src/ui/widgets/PlaylistSidebar.cpp
    src/ui/widgets/PlaylistSidebar.h
    src/ui/widgets/VideoPlayerWidget.cpp
    src/ui/widgets/VideoPlayerWidget.h

    src/ui/dialogs/AboutDialog.cpp
    src/ui/dialogs/AboutDialog.h
    src/ui/dialogs/AddPlaylistDialog.cpp
    src/ui/dialogs/AddPlaylistDialog.h
    src/ui/dialogs/ChannelInfoDialog.cpp
    src/ui/dialogs/ChannelInfoDialog.h
    src/ui/dialogs/PlaylistManagerDialog.cpp
    src/ui/dialogs/PlaylistManagerDialog.h
    src/ui/dialogs/RestoreDeletedDialog.cpp
    src/ui/dialogs/RestoreDeletedDialog.h
    src/ui/dialogs/SettingsDialog.cpp
    src/ui/dialogs/SettingsDialog.h
)

qt_add_library(CopperCore STATIC ${COPPER_CORE_SOURCES})

target_include_directories(CopperCore PUBLIC
    ${CMAKE_CURRENT_BINARY_DIR}/generated
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(CopperCore PUBLIC
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Network
    Qt6::Multimedia
    Qt6::MultimediaWidgets
    Qt6::Sql
    Qt6::Concurrent
)

qt_add_executable(CopperIPTVPlayer WIN32 ${COPPER_APP_SOURCES}
    resources/resources.qrc
)

# Embed the Copper application icon into the executable. windres' off-line RC
# rule forwards the project's #-I include dirs to a preprocessor that breaks
# on the space in this source tree's path, so we invoke windres ourselves with
# a minimal command line and link the resulting object into the app.
if(WIN32)
    set(APP_ICON_OBJ ${CMAKE_CURRENT_BINARY_DIR}/app_icon_res.o)
    add_custom_command(
        OUTPUT ${APP_ICON_OBJ}
        COMMAND ${CMAKE_RC_COMPILER} -O coff
                -i ${CMAKE_CURRENT_SOURCE_DIR}/resources/win32.rc
                -o ${APP_ICON_OBJ}
        DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/resources/win32.rc
                ${CMAKE_CURRENT_SOURCE_DIR}/Assets/app.ico
        VERBATIM
    )
    target_sources(CopperIPTVPlayer PRIVATE ${APP_ICON_OBJ})
endif()

# Make the build output self-contained: windeployqt copies the Qt DLLs,
# plugins and the MinGW compiler runtime next to the executable, so it can be
# started by double-clicking without Qt on PATH (the Debug exe links the
# release Qt libraries that ship with the MinGW kit).
if(WIN32)
    find_program(QT_WINDEPLOYQT_EXECUTABLE windeployqt
        HINTS "${CMAKE_PREFIX_PATH}/bin"
    )
    if(QT_WINDEPLOYQT_EXECUTABLE)
        add_custom_command(TARGET CopperIPTVPlayer POST_BUILD
            COMMAND ${QT_WINDEPLOYQT_EXECUTABLE}
                    --release --no-translations --no-opengl-sw
                    --no-system-d3d-compiler
                    "$<TARGET_FILE:CopperIPTVPlayer>"
            COMMENT "Deploying Qt runtime next to the debug executable"
            VERBATIM
        )
        # Drop the SQL drivers that drag third-party client DLLs in
        # (Firebird, Oracle, PostgreSQL, MyODBC); keep only SQLite.
        add_custom_command(TARGET CopperIPTVPlayer POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E rm -f
                    "$<TARGET_FILE_DIR:CopperIPTVPlayer>/sqldrivers/qsqlibase.dll"
                    "$<TARGET_FILE_DIR:CopperIPTVPlayer>/sqldrivers/qsqloci.dll"
                    "$<TARGET_FILE_DIR:CopperIPTVPlayer>/sqldrivers/qsqlodbc.dll"
                    "$<TARGET_FILE_DIR:CopperIPTVPlayer>/sqldrivers/qsqlpsql.dll"
                    "$<TARGET_FILE_DIR:CopperIPTVPlayer>/sqldrivers/qsqlmimer.dll"
            VERBATIM
        )
    endif()
endif()

target_link_libraries(CopperIPTVPlayer PRIVATE CopperCore)

set_target_properties(CopperIPTVPlayer PROPERTIES
    OUTPUT_NAME "CopperIPTVPlayer"
    WIN32_EXECUTABLE TRUE
)

install(TARGETS CopperIPTVPlayer
    BUNDLE DESTINATION .
    RUNTIME DESTINATION bin
)

include(CTest)
if(BUILD_TESTING)
    enable_testing()
    find_package(Qt6 REQUIRED COMPONENTS Test)
    add_subdirectory(tests)
endif()