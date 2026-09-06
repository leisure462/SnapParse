cmake_minimum_required(VERSION 3.20)

project(SnapParse VERSION 3.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 REQUIRED COMPONENTS
    Core
    Gui
    Widgets
    Sql
    Network
    Svg
)

# Source files
set(SOURCES
    src/main.cpp

    # Core
    src/core/AppConfig.h
    src/core/AppConfig.cpp
    src/core/AppPaths.h
    src/core/AppPaths.cpp
    src/core/Logger.h
    src/core/Logger.cpp
    src/core/EventBus.h
    src/core/EventBus.cpp

    # DB
    src/db/Models.h
    src/db/DatabaseManager.h
    src/db/DatabaseManager.cpp

    # System
    src/system/ClipboardMonitor.h
    src/system/ClipboardMonitor.cpp
    src/system/PasteInjector.h
    src/system/PasteInjector.cpp
    src/system/GlobalHookManager.h
    src/system/GlobalHookManager.cpp
    src/system/HotkeyManager.h
    src/system/HotkeyManager.cpp
    src/system/AutoStartManager.h
    src/system/AutoStartManager.cpp
    src/system/WindowBackdropHelper.h
    src/system/WindowBackdropHelper.cpp
    src/system/MemoryManager.h
    src/system/MemoryManager.cpp
    src/system/SystemProcessManager.h
    src/system/SystemProcessManager.cpp

    # Service
    src/service/ContentParser.h
    src/service/ContentParser.cpp
    src/service/SensitiveDetector.h
    src/service/SensitiveDetector.cpp
    src/service/HistoryCleanService.h
    src/service/HistoryCleanService.cpp
    src/service/BackupService.h
    src/service/BackupService.cpp

    # UI Theme & Components
    src/ui/theme/ThemeManager.h
    src/ui/theme/ThemeManager.cpp
    src/ui/fluent/FluentIcon.h
    src/ui/fluent/FluentIcon.cpp
    src/ui/fluent/FluentSettingCard.h
    src/ui/fluent/FluentSettingCard.cpp
    src/ui/fluent/FluentSwitchSettingCard.h
    src/ui/fluent/FluentSwitchSettingCard.cpp
    src/ui/fluent/FluentOptionsSettingCard.h
    src/ui/fluent/FluentOptionsSettingCard.cpp
    src/ui/fluent/FluentRangeSettingCard.h
    src/ui/fluent/FluentRangeSettingCard.cpp
    src/ui/fluent/FluentPushSettingCard.h
    src/ui/fluent/FluentPushSettingCard.cpp
    src/ui/fluent/FluentColorSettingCard.h
    src/ui/fluent/FluentColorSettingCard.cpp
    src/ui/fluent/FluentFontSettingCard.h
    src/ui/fluent/FluentFontSettingCard.cpp
    src/ui/fluent/FluentSettingCardGroup.h
    src/ui/fluent/FluentSettingCardGroup.cpp
    src/ui/fluent/FluentSearchLineEdit.h
    src/ui/fluent/FluentSearchLineEdit.cpp
    src/ui/fluent/FluentAppFilterWidget.h
    src/ui/fluent/FluentAppFilterWidget.cpp
    src/ui/components/SwitchWidget.h
    src/ui/components/SwitchWidget.cpp
    src/ui/components/SegmentedWidget.h
    src/ui/components/SegmentedWidget.cpp
    src/ui/components/KeySequenceRecorder.h
    src/ui/components/KeySequenceRecorder.cpp
    src/ui/components/SortableTreeDialog.h
    src/ui/components/SortableTreeDialog.cpp
    src/ui/components/StorageMeterWidget.h
    src/ui/components/StorageMeterWidget.cpp

    # UI Tray & Preview
    src/ui/tray/SystemTrayManager.h
    src/ui/tray/SystemTrayManager.cpp
    src/ui/preview/PreviewWindow.h
    src/ui/preview/PreviewWindow.cpp

    # UI Preferences
    src/ui/preferences/PreferencesWindow.h
    src/ui/preferences/PreferencesWindow.cpp
    src/ui/preferences/PreferenceSidebar.h
    src/ui/preferences/PreferenceSidebar.cpp
    src/ui/preferences/PreferenceHeader.h
    src/ui/preferences/PreferenceHeader.cpp
    src/ui/preferences/SearchResultPopup.h
    src/ui/preferences/SearchResultPopup.cpp
    src/ui/preferences/pages/RecordPage.h
    src/ui/preferences/pages/RecordPage.cpp
    src/ui/preferences/pages/OrganizePage.h
    src/ui/preferences/pages/OrganizePage.cpp
    src/ui/preferences/pages/ReusePage.h
    src/ui/preferences/pages/ReusePage.cpp
    src/ui/preferences/pages/WorkflowPage.h
    src/ui/preferences/pages/WorkflowPage.cpp
    src/ui/preferences/pages/ShortcutsPage.h
    src/ui/preferences/pages/ShortcutsPage.cpp
    src/ui/preferences/pages/DataPage.h
    src/ui/preferences/pages/DataPage.cpp
    src/ui/preferences/pages/AboutPage.h
    src/ui/preferences/pages/AboutPage.cpp
    src/ui/preferences/modals/BackupExportDialog.h
    src/ui/preferences/modals/BackupExportDialog.cpp
    src/ui/preferences/modals/BackupImportDialog.h
    src/ui/preferences/modals/BackupImportDialog.cpp
    src/ui/preferences/modals/LifecycleDialog.h
    src/ui/preferences/modals/LifecycleDialog.cpp

    # UI Clipboard
    src/ui/clipboard/ClipboardWindow.h
    src/ui/clipboard/ClipboardWindow.cpp
    src/ui/clipboard/HeaderBar.h
    src/ui/clipboard/HeaderBar.cpp
    src/ui/clipboard/ClipboardListView.h
    src/ui/clipboard/ClipboardListView.cpp
    src/ui/clipboard/ClipboardCardDelegate.h
    src/ui/clipboard/ClipboardCardDelegate.cpp
    src/ui/clipboard/QuickActionBar.h
    src/ui/clipboard/QuickActionBar.cpp
    src/ui/clipboard/NoteDialog.h
    src/ui/clipboard/NoteDialog.cpp

    # Resources
    resources/resources.qrc
    resources/app.rc
)

# WIN32 creates standard GUI application
add_executable(SnapParse WIN32 ${SOURCES})

target_include_directories(SnapParse PRIVATE
    src
    src/core
    src/db
    src/system
    src/service
    src/ui
    src/ui/theme
    src/ui/fluent
    src/ui/components
    src/ui/tray
    src/ui/preview
    src/ui/preferences
    src/ui/preferences/pages
    src/ui/preferences/modals
    src/ui/clipboard
)

target_link_libraries(SnapParse PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Sql
    Qt6::Network
    Qt6::Svg
)

if (WIN32)
    target_link_libraries(SnapParse PRIVATE
        user32
        shell32
        gdi32
        dwmapi
        advapi32
        psapi
        version
    )
endif()

if (MSVC)
    target_compile_options(SnapParse PRIVATE
        $<$<CONFIG:Release>:/O2 /Oi /Ot /Gy /GF>
    )
    target_link_options(SnapParse PRIVATE
        $<$<CONFIG:Release>:/OPT:REF /OPT:ICF>
    )
endif()
