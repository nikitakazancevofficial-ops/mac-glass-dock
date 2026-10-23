cmake_minimum_required(VERSION 3.21)

project(MacGlassDock VERSION 0.4.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Qt6 6.8 REQUIRED COMPONENTS Quick ShaderTools DBus)
find_package(LayerShellQt 6.0 REQUIRED)
find_package(KF6WindowSystem REQUIRED)
find_package(KF6Service REQUIRED)
find_package(KF6KIO REQUIRED)
find_package(KWayland QUIET)

qt_standard_project_setup(REQUIRES 6.8)
qt_policy(SET QTP0001 NEW)
qt_policy(SET QTP0004 NEW)

qt_add_executable(mac-glass-dock
    src/main.cpp
    src/dockplatform.cpp src/dockplatform.h
    src/docksettings.cpp src/docksettings.h
    src/controlcenterstate.cpp src/controlcenterstate.h
    src/applicationmodel.cpp src/applicationmodel.h
    src/applicationcatalogue.cpp src/applicationcatalogue.h
    src/windowtracker.cpp src/windowtracker.h
    src/framediagnostics.cpp src/framediagnostics.h
    src/desktopintegration.cpp src/desktopintegration.h
)

qt_add_qml_module(mac-glass-dock
    URI MacGlassDock
    VERSION 1.0
    QML_FILES
        qml/Main.qml
        qml/ControlCenterWindow.qml
        qml/LauncherWindow.qml
        qml/SettingsWindow.qml
        qml/AppMenuWindow.qml
        qml/WindowSwitcher.qml
        qml/components/GlassPanelWindow.qml
        qml/components/GlassButton.qml
        qml/components/ControlIcon.qml
        qml/components/StatusIcon.qml
        qml/components/ControlTile.qml
        qml/components/ControlSlider.qml
        qml/components/GlassSlider.qml
        qml/components/GlassSwitch.qml
        qml/components/Dock.qml
        qml/components/DockItem.qml
        qml/components/AdaptiveAppIcon.qml
        qml/components/WindowThumbnail.qml
        qml/effects/GlassSurface.qml
        qml/effects/VolumetricGlassSurface.qml
    RESOURCES
        assets/control-center/icons/wifi-c.svg
        assets/control-center/icons/bluetooth-c.svg
        assets/control-center/icons/focus-c.svg
        assets/control-center/icons/display-c.svg
        assets/control-center/icons/sound-c.svg
        assets/control-center/icons/keyboard-c.svg
        assets/control-center/icons/battery-c.svg
        assets/control-center/icons/wifi-c-dark.svg
        assets/control-center/icons/focus-c-dark.svg
        assets/control-center/icons/battery-c-dark.svg
        assets/control-center/icons/mirroring-c.svg
)

set_target_properties(mac-glass-dock PROPERTIES
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
)

target_link_libraries(mac-glass-dock PRIVATE Qt6::Quick Qt6::DBus LayerShellQt::Interface
    KF6::WindowSystem KF6::Service KF6::KIOGui)
target_include_directories(mac-glass-dock PRIVATE src)
target_compile_definitions(mac-glass-dock PRIVATE DOCK_PROJECT_DIR="${CMAKE_SOURCE_DIR}")
if(TARGET Plasma::KWaylandClient)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(WAYLANDCLIENT REQUIRED IMPORTED_TARGET wayland-client)
    target_link_libraries(mac-glass-dock PRIVATE Plasma::KWaylandClient)
    target_link_libraries(mac-glass-dock PRIVATE PkgConfig::WAYLANDCLIENT)
    target_compile_definitions(mac-glass-dock PRIVATE HAVE_KWAYLAND)
endif()

qt_add_shaders(mac-glass-dock glass_shaders
    PREFIX "/effects"
    BASE "shaders"
    FILES shaders/glass.frag shaders/adaptive-icon.frag shaders/panel-genie.vert shaders/panel-genie.frag
)

include(CTest)
if(BUILD_TESTING)
    find_package(Python3 REQUIRED COMPONENTS Interpreter)
    add_test(NAME desktop_recovery_test COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/testdesktopintegration.py)
    find_package(Qt6 REQUIRED COMPONENTS Test)
    qt_add_executable(dock-control-center-tests tests/controlcenterstatetests.cpp src/controlcenterstate.cpp src/controlcenterstate.h)
    target_include_directories(dock-control-center-tests PRIVATE src)
    target_link_libraries(dock-control-center-tests PRIVATE Qt6::Qml Qt6::DBus Qt6::Test)
    add_test(NAME control_center_audio_test COMMAND dock-control-center-tests)
    set_tests_properties(control_center_audio_test PROPERTIES TIMEOUT 20)
    qt_add_executable(dock-genie-tests tests/genietests.cpp)
    target_include_directories(dock-genie-tests PRIVATE kwin)
    target_link_libraries(dock-genie-tests PRIVATE Qt6::Test)
    add_test(NAME genie_geometry_test COMMAND dock-genie-tests)
    qt_add_executable(dock-status-hit-tests tests/statushittests.cpp)
    target_include_directories(dock-status-hit-tests PRIVATE kwin)
    target_link_libraries(dock-status-hit-tests PRIVATE Qt6::Gui Qt6::Test)
    add_test(NAME control_center_compositor_hit_test COMMAND dock-status-hit-tests)
    qt_add_executable(dock-optics-tests tests/opticstests.cpp)
    qt_add_executable(dock-optics-session tests/opticssession.cpp)
    target_link_libraries(dock-optics-session PRIVATE Qt6::Quick Qt6::DBus Qt6::Test LayerShellQt::Interface)
    target_compile_definitions(dock-optics-session PRIVATE DOCK_BINARY_DIR="${CMAKE_BINARY_DIR}" DOCK_KWIN_BUILD="${CMAKE_SOURCE_DIR}/build-kwin")
    target_link_libraries(dock-optics-tests PRIVATE Qt6::Gui Qt6::OpenGL Qt6::Test)
    target_compile_definitions(dock-optics-tests PRIVATE DOCK_SOURCE_DIR="${CMAKE_SOURCE_DIR}" DOCK_BINARY_DIR="${CMAKE_BINARY_DIR}")
    add_test(NAME optics_shader_test COMMAND dock-optics-tests)
    set_tests_properties(optics_shader_test PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen" TIMEOUT 20)
    target_sources(mac-glass-dock PRIVATE tests/docktests.cpp tests/docktests.h)
    target_sources(mac-glass-dock PRIVATE tests/stage4tests.cpp tests/stage4tests.h)
    target_include_directories(mac-glass-dock PRIVATE src tests)
    target_link_libraries(mac-glass-dock PRIVATE Qt6::Test)
    target_compile_definitions(mac-glass-dock PRIVATE
        DOCK_TESTING
        DOCK_TEST_MARKER="${CMAKE_CURRENT_BINARY_DIR}/test-launch.marker"
        DOCK_TEST_CONFIG="${CMAKE_CURRENT_BINARY_DIR}/test-config/mac-glass-dock/settings.ini"
        DOCK_WINDOW_PROBE="${CMAKE_CURRENT_BINARY_DIR}/dock-window-probe"
    )
    qt_add_executable(dock-launch-probe tests/launchprobe.cpp)
    target_link_libraries(dock-launch-probe PRIVATE Qt6::Core)
    qt_add_executable(dock-window-probe tests/windowprobe.cpp)
    target_link_libraries(dock-window-probe PRIVATE Qt6::Quick)
    qt_add_executable(dock-settings-tests tests/settingstests.cpp src/docksettings.cpp src/docksettings.h)
    target_include_directories(dock-settings-tests PRIVATE src)
    target_link_libraries(dock-settings-tests PRIVATE Qt6::Gui Qt6::Qml Qt6::Test)
    target_compile_definitions(dock-settings-tests PRIVATE DOCK_TEST_DIR="${CMAKE_CURRENT_BINARY_DIR}")
    add_test(NAME settings_validation_test COMMAND dock-settings-tests)
    set_tests_properties(settings_validation_test PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen" TIMEOUT 10)
    configure_file(tests/probe.desktop.in test-data/applications/mac-glass-dock-probe.desktop @ONLY)
    configure_file(tests/settings.ini test-config/mac-glass-dock/settings.ini COPYONLY)
    add_test(NAME qml_smoke_test
        COMMAND mac-glass-dock --smoke-test
    )
    set_tests_properties(qml_smoke_test PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen;XDG_CONFIG_HOME=${CMAKE_CURRENT_BINARY_DIR}/test-config;XDG_CACHE_HOME=${CMAKE_CURRENT_BINARY_DIR}/test-cache;XDG_DATA_HOME=${CMAKE_CURRENT_BINARY_DIR}/test-data"
        TIMEOUT 10
    )
    add_test(NAME dock_interaction_test COMMAND mac-glass-dock --preview --interaction-test)
    set_tests_properties(dock_interaction_test PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software;XDG_CONFIG_HOME=${CMAKE_CURRENT_BINARY_DIR}/test-config;XDG_CACHE_HOME=${CMAKE_CURRENT_BINARY_DIR}/test-cache;XDG_DATA_HOME=${CMAKE_CURRENT_BINARY_DIR}/test-data"
        TIMEOUT 45
        RUN_SERIAL TRUE
    )
    add_test(NAME qml_failure_is_rejected COMMAND mac-glass-dock --smoke-test --test-missing-qml)
    add_test(NAME stage4_ui_test COMMAND mac-glass-dock --stage4-test)
    set_tests_properties(stage4_ui_test PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software;XDG_CONFIG_HOME=${CMAKE_CURRENT_BINARY_DIR}/test-config;XDG_CACHE_HOME=${CMAKE_CURRENT_BINARY_DIR}/test-cache;XDG_DATA_HOME=${CMAKE_CURRENT_BINARY_DIR}/test-data"
        TIMEOUT 90 RUN_SERIAL TRUE
    )
    add_test(NAME control_center_status_click_test COMMAND mac-glass-dock --control-center-test)
    set_tests_properties(control_center_status_click_test PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software;XDG_CONFIG_HOME=${CMAKE_CURRENT_BINARY_DIR}/test-config;XDG_CACHE_HOME=${CMAKE_CURRENT_BINARY_DIR}/test-cache;XDG_DATA_HOME=${CMAKE_CURRENT_BINARY_DIR}/test-data"
        TIMEOUT 20 RUN_SERIAL TRUE
    )
    set_tests_properties(qml_failure_is_rejected PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen;XDG_CONFIG_HOME=${CMAKE_CURRENT_BINARY_DIR}/test-config;XDG_CACHE_HOME=${CMAKE_CURRENT_BINARY_DIR}/test-cache;XDG_DATA_HOME=${CMAKE_CURRENT_BINARY_DIR}/test-data"
        WILL_FAIL TRUE
        TIMEOUT 10
    )
endif()

include(GNUInstallDirs)
install(TARGETS mac-glass-dock
    BUNDLE DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
