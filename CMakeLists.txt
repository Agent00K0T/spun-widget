# Spun Widget: the record player of Spun (https://github.com/yappologistic/Spun, commit d4448ec)
# as a desktop widget. Everything else of Spun was removed; see the original to bring parts back.
cmake_minimum_required(VERSION 3.22)
project(SpunWidget VERSION 1.0.0 LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
include(GNUInstallDirs)

find_package(Qt6 6.8 REQUIRED COMPONENTS Core Gui Quick Qml QuickControls2 Concurrent DBus Network LinguistTools)
find_package(LayerShellQt REQUIRED)
qt_policy(SET QTP0004 NEW)

qt_add_executable(spun-widget
    src/main.cpp
    src/activeplayer.cpp src/activeplayer.h
    src/disc.cpp src/disc.h
    src/palette.cpp src/palette.h
    src/playerbody.cpp src/playerbody.h
    src/theme.cpp src/theme.h)
target_link_libraries(spun-widget PRIVATE Qt6::Core Qt6::Gui Qt6::Quick Qt6::Qml Qt6::QuickControls2
    Qt6::Concurrent Qt6::DBus Qt6::Network LayerShellQt::Interface)
# Where the installed chapter helper lives, relative to the installed binary
target_compile_definitions(spun-widget PRIVATE
    SPUN_WIDGET_HELPER_DIR="../${CMAKE_INSTALL_LIBEXECDIR}/spun-widget")
target_compile_options(spun-widget PRIVATE -Wall -Wextra -Wpedantic)
set_source_files_properties(qml/Style.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE)
qt_add_qml_module(spun-widget URI SpunWidget VERSION 1.0 NO_PLUGIN
    NO_RESOURCE_TARGET_PATH RESOURCE_PREFIX "/"
    QML_FILES qml/Main.qml qml/Tonearm.qml qml/Bubble.qml qml/ChapterRings.qml qml/Style.qml)
# Tooltips follow the system language (English source strings); add i18n/spun-widget_<lang>.ts to translate
qt_add_translations(spun-widget TS_FILES i18n/spun-widget_ru.ts RESOURCE_PREFIX /i18n)

# The chapter helper next to the build's binary, so the build tree runs as is
configure_file(scripts/yt-chapters ${CMAKE_CURRENT_BINARY_DIR}/yt-chapters COPYONLY)

install(TARGETS spun-widget RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(PROGRAMS scripts/yt-chapters DESTINATION ${CMAKE_INSTALL_LIBEXECDIR}/spun-widget)
