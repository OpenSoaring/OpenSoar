set(_SOURCES
        Widget/ActionWidget.cpp
        Widget/ArrowPagerWidget.cpp
        Widget/ButtonPanelWidget.cpp
        Widget/ButtonWidget.cpp
        Widget/CallbackWidget.cpp
        Widget/ContainerWidget.cpp
        Widget/CreateWindowWidget.cpp
        Widget/EditRowFormWidget.cpp
        Widget/KeyboardWidget.cpp
        Widget/LargeTextWidget.cpp
        Widget/ListWidget.cpp
        Widget/ManagedWidget.cpp
        Widget/OffsetButtonsWidget.cpp
        Widget/OverlappedWidget.cpp
        Widget/PagerWidget.cpp
        Widget/PanelWidget.cpp
        Widget/ProfileRowFormWidget.cpp
        Widget/QuestionWidget.cpp
        Widget/RowFormWidget.cpp
        Widget/FileRowFormWidget.cpp
        Widget/SolidWidget.cpp
        Widget/TabWidget.cpp
        Widget/TextListWidget.cpp
        Widget/TextWidget.cpp
        Widget/TwoWidgets.cpp
        Widget/UnitRowFormWidget.cpp
        Widget/ViewImageWidget.cpp
        Widget/Widget.cpp
        Widget/WindowWidget.cpp
        
        Widget/ProgressWidget.cpp
        Widget/LargeTextWidget.cpp
        Widget/VScrollWidget.cpp
        Widget/FileMultiSelectWidget.cpp
        Widget/MultiSelectListWidget.cpp
        Widget/QuickGuidePageWidget.cpp   # new with 7.44
        Widget/RichTextWidget.cpp   # new with 7.44
)

set(SCRIPT_FILES
    CMakeSource.cmake

    ../../build/libwidget.mk
)



# --- synced from upstream XCSoar (v7.44 -> master, 2026-08-13) ---
list(APPEND _SOURCES
        Widget/CursorBarWidget.cpp
        Widget/ImageZoomFrame.cpp
        Widget/ImageZoomView.cpp
        Widget/PropertyWidgetContainer.cpp
        Widget/ScrollableLargeTextWidget.cpp
)
