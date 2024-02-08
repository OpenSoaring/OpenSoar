
#include "OV/Settings.hpp"
#include "OV/System.hpp"
#include "Widget/RowFormWidget.hpp"

#include <string>
#include <fmt/format.h>

#if OV_SETTINGS

static DialogSettings dialog_settings;
static UI::SingleWindow *global_main_window;
static DialogLook *global_dialog_look;

const DialogSettings &
UIGlobals::GetDialogSettings()
{
  return dialog_settings;
}

const DialogLook &
UIGlobals::GetDialogLook()
{
  assert(global_dialog_look != nullptr);

  return *global_dialog_look;
}

UI::SingleWindow &
UIGlobals::GetMainWindow()
{
  assert(global_main_window != nullptr);

  return *global_main_window;
}



void
SystemMenuWidget::Prepare([[maybe_unused]] ContainerWindow &parent,
                          [[maybe_unused]] const PixelRect &rc) noexcept
{
  AddButton("Update System", [](){
    static constexpr const char *argv[] = {
      "/usr/bin/update-system.sh", nullptr
    };

    RunProcessDialog(UIGlobals::GetMainWindow(),
                     UIGlobals::GetDialogLook(),
                     "Update System", argv);
  });

  AddButton("Update Maps", [](){
    static constexpr const char *argv[] = {
      "/usr/bin/update-maps.sh", nullptr
    };

    RunProcessDialog(UIGlobals::GetMainWindow(),
                     UIGlobals::GetDialogLook(),
                     "Update Maps", argv);
  });

  AddButton("Calibrate Sensors", CalibrateSensors);
  AddButton("Calibrate Touch", [this](){
    const UI::ScopeDropMaster drop_master{display};
    const UI::ScopeSuspendEventQueue suspend_event_queue{event_queue};
    Run("/usr/bin/ov-calibrate-ts.sh");
  });

  AddButton("System Settings", [this](){
    const UI::ScopeDropMaster drop_master{display};
    const UI::ScopeSuspendEventQueue suspend_event_queue{event_queue};
    Run("/usr/lib/openvario/libexec/system_settings.sh");
  });

  AddButton("System Info", [this](){
    const UI::ScopeDropMaster drop_master{display};
    const UI::ScopeSuspendEventQueue suspend_event_queue{event_queue};
    Run("/usr/lib/openvario/libexec/system_info.sh");
  });
}

#endif

#if OV_SETTINGS

#include "Profile/File.hpp"
#include "system/FileUtil.hpp"

class ScreenBrightnessWidget final
  : public RowFormWidget
{
  UI::Display &display;
  UI::EventQueue &event_queue;

public:
  ScreenBrightnessWidget(UI::Display &_display, UI::EventQueue &_event_queue,
                 const DialogLook &look) noexcept
    :RowFormWidget(look),
     display(_display), event_queue(_event_queue) {}

private:
  /* virtual methods from class Widget */
  void Prepare(ContainerWindow &parent,
               const PixelRect &rc) noexcept override;

  uint_least8_t brightness = 10;
  void SaveBrightness(const string &brightness);
};

void
ScreenBrightnessWidget::SaveBrightness(const string &brightness)
{
  File::WriteExisting(Path("/sys/class/backlight/lcd/brightness"), (brightness).c_str());
}

void
ScreenBrightnessWidget::Prepare([[maybe_unused]] ContainerWindow &parent,
                                [[maybe_unused]] const PixelRect &rc) noexcept
{
#if 0
  for (unsigned i = 1; i <= 10; i++) {
    // char buffer[10];
    // sprintf // (buffer, "%d", i * 10);
    brightness = i;    
    AddButton(fmt::format_int{i*10}.c_str(), [this]() {
      OpenvarioSetBrightness(brightness);  // i);
        // SaveBrightness("2");
    });
  }
#else
  AddButton("20", [this](){
    SaveBrightness("2");
  });

  AddButton("30", [this](){
    SaveBrightness("3");
  });

  AddButton("40", [this](){
    SaveBrightness("4");
  });

  AddButton("50", [this](){
    SaveBrightness("5");
  });

  AddButton("60", [this](){
    SaveBrightness("6");
  });

  AddButton("70", [this](){
    SaveBrightness("7");
  });

  AddButton("80", [this](){
    SaveBrightness("8");
  });

  AddButton("90", [this](){
    SaveBrightness("9");
  });

  AddButton("100", [this](){
    SaveBrightness("10");
  });
#endif
}
#endif
