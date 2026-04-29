cmake_minimum_required(VERSION 3.16)

project(xray_serial_console VERSION 1.00.00 LANGUAGES CXX)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC_SEARCH_PATHS ${CMAKE_CURRENT_SOURCE_DIR}/ui)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets LinguistTools)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets LinguistTools)

set(TS_FILES
        translations/xray_serial_console_zh_CN.ts
)

set(MARKDOWN_DOC_FILES
        # 文档文件加入目标源列表，方便 Qt Creator 项目树中直接查看和编辑。
        README.md
        README.zh-CN.md
        README.en-US.md
        docs/design.md
        docs/packaging-windows.md
        docs/pitfalls.md
)

set(CMAKE_PRESET_FILES
        CMakePresets.json
)

set(AGENT_RULE_FILES
        AGENTS.md
)

set(SKILL_DRAFT_FILES
        docs/skills/qt-widgets-cpp/SKILL.md
)

set(COMMIT_MESSAGE_FILES)
set(COMMIT_MESSAGE_FILE ${CMAKE_CURRENT_SOURCE_DIR}/build/commit-message-${PROJECT_VERSION}.txt)
if(EXISTS ${COMMIT_MESSAGE_FILE})
    # 提交日志位于被忽略的 build 目录，仅在本地文件存在时加入 Qt Creator 项目树。
    list(APPEND COMMIT_MESSAGE_FILES ${COMMIT_MESSAGE_FILE})
endif()

set(UTILITY_FILES
        scripts/build-local.ps1
        scripts/package-windows.bat
        scripts/package-windows.ps1
        scripts/syntax-check.ps1
)

set(APP_HEADERS
        include/appsettings.h
        include/appversion.h
        include/mainwindow.h
        include/multistringdock.h
        include/serialportworker.h
        include/toolboxdialog.h
)

set(APP_SOURCES
        src/appsettings.cpp
        src/mainwindow.cpp
        src/multistringdock.cpp
        src/serialportworker.cpp
        src/toolboxdialog.cpp
)

set(APP_UI
        ui/mainwindow.ui
)

set(APP_RESOURCES
        resources/resources.qrc
)

set(RESOURCE_ASSET_FILES
        resources/icons/checkmark.svg
        resources/icons/chevron-down.svg
        resources/icons/chevron-up.svg
)

set(PACKAGE_SCRIPT_SOURCE ${CMAKE_CURRENT_SOURCE_DIR}/scripts/package-windows.ps1)
set(PACKAGE_BAT_SOURCE ${CMAKE_CURRENT_SOURCE_DIR}/scripts/package-windows.bat)
set(PACKAGE_SCRIPT_IN_BUILD_DIR ${CMAKE_CURRENT_BINARY_DIR}/package-windows.ps1)
set(PACKAGE_BAT_IN_BUILD_DIR ${CMAKE_CURRENT_BINARY_DIR}/package-windows.bat)
configure_file(${PACKAGE_SCRIPT_SOURCE} ${PACKAGE_SCRIPT_IN_BUILD_DIR} COPYONLY)
configure_file(${PACKAGE_BAT_SOURCE} ${PACKAGE_BAT_IN_BUILD_DIR} COPYONLY)

set(PROJECT_SOURCES
        src/main.cpp
        ${APP_SOURCES}
        ${APP_HEADERS}
        ${APP_UI}
        ${APP_RESOURCES}
        ${RESOURCE_ASSET_FILES}
        ${TS_FILES}
        ${MARKDOWN_DOC_FILES}
        ${CMAKE_PRESET_FILES}
        ${AGENT_RULE_FILES}
        ${SKILL_DRAFT_FILES}
        ${COMMIT_MESSAGE_FILES}
        ${UTILITY_FILES}
)

source_group("Source Files" FILES src/main.cpp ${APP_SOURCES})
source_group("Header Files" FILES ${APP_HEADERS})
source_group("Forms" FILES ${APP_UI})
source_group("Resources" FILES ${APP_RESOURCES} ${RESOURCE_ASSET_FILES})
source_group("Translations" FILES ${TS_FILES})
source_group("Scripts" FILES ${UTILITY_FILES})
source_group("Markdown Documents" FILES ${MARKDOWN_DOC_FILES})
source_group("CMake Presets" FILES ${CMAKE_PRESET_FILES})
source_group("Agent Rules" FILES ${AGENT_RULE_FILES})
source_group("Skill Drafts" FILES ${SKILL_DRAFT_FILES})
source_group("Git Commit Messages" FILES ${COMMIT_MESSAGE_FILES})

if(${QT_VERSION_MAJOR} GREATER_EQUAL 6)
    qt_add_executable(xray_serial_console
        MANUAL_FINALIZATION
        ${PROJECT_SOURCES}
    )
# Define target properties for Android with Qt 6 as:
#    set_property(TARGET xray_serial_console APPEND PROPERTY QT_ANDROID_PACKAGE_SOURCE_DIR
#                 ${CMAKE_CURRENT_SOURCE_DIR}/android)
# For more information, see https://doc.qt.io/qt-6/qt-add-executable.html#target-creation

    qt_create_translation(QM_FILES ${CMAKE_SOURCE_DIR} ${TS_FILES})
else()
    if(ANDROID)
        add_library(xray_serial_console SHARED
            ${PROJECT_SOURCES}
        )
# Define properties for Android with Qt 5 after find_package() calls as:
#    set(ANDROID_PACKAGE_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/android")
    else()
        add_executable(xray_serial_console
            ${PROJECT_SOURCES}
        )
    endif()

    qt5_create_translation(QM_FILES ${CMAKE_SOURCE_DIR} ${TS_FILES})
endif()

target_include_directories(xray_serial_console PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

add_custom_command(TARGET xray_serial_console POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${PACKAGE_SCRIPT_SOURCE}
            ${PACKAGE_SCRIPT_IN_BUILD_DIR}
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${PACKAGE_BAT_SOURCE}
            ${PACKAGE_BAT_IN_BUILD_DIR}
    COMMENT "Copy Windows packaging helpers to CMake build directory"
    VERBATIM
)

target_link_libraries(xray_serial_console PRIVATE Qt${QT_VERSION_MAJOR}::Widgets setupapi)

# Qt for iOS sets MACOSX_BUNDLE_GUI_IDENTIFIER automatically since Qt 6.1.
# If you are developing for iOS or macOS you should consider setting an
# explicit, fixed bundle identifier manually though.
if(${QT_VERSION} VERSION_LESS 6.1.0)
  set(BUNDLE_ID_OPTION MACOSX_BUNDLE_GUI_IDENTIFIER com.example.xray_serial_console)
endif()
set_target_properties(xray_serial_console PROPERTIES
    ${BUNDLE_ID_OPTION}
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
)

include(GNUInstallDirs)
install(TARGETS xray_serial_console
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

if(QT_VERSION_MAJOR EQUAL 6)
    qt_finalize_executable(xray_serial_console)
endif()
