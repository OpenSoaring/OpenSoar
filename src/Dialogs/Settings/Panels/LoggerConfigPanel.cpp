// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#include "LoggerConfigPanel.hpp"
#include "Profile/Profile.hpp"
#include "Language/Language.hpp"
#include "Interface.hpp"
#include "Widget/RowFormWidget.hpp"
#include "UIGlobals.hpp"
#include "Form/DataField/Enum.hpp"
#include "Logger/NMEALogger.hpp"
#include "UtilsSettings.hpp"
#include "Components.hpp"
#include "BackendComponents.hpp"
#include "Units/Group.hpp"
#include "Units/Units.hpp"
#include "Logger/LogbookBuilder.hpp"
#include "Form/DataField/File.hpp"
#include "Dialogs/FilePicker.hpp"
#include "Dialogs/Message.hpp"
#include "Repository/FileType.hpp"
#include "Device/Config.hpp"
#include "Device/Driver.hpp"
#include "Device/Register.hpp"
#include "Device/Features.hpp"
#include "SystemSettings.hpp"
#include "Form/Edit.hpp"

using namespace std::chrono;

enum ControlIndex {
  PilotName,
  CoPilotName,
  CrewWeightTemplate,
  LoggerTimeStepCruise,
  LoggerTimeStepCircling,
  DisableAutoLogger,
  EnableNMEALogger,
  EnableFlightLogger,
  Recorder1,
  Recorder1FromFile,
  Recorder1Device,
  Recorder2,
  Recorder2FromFile,
  Recorder2Device,
  LoggerID,
};

class LoggerConfigPanel final : public RowFormWidget {
public:
  LoggerConfigPanel()
    :RowFormWidget(UIGlobals::GetDialogLook()) {}

public:
  void Prepare(ContainerWindow &parent, const PixelRect &rc) noexcept override;
  bool Save(bool &changed) noexcept override;

private:
  void ReadRecorderFromFile(unsigned row) noexcept;
  void AddRecorderDevice(const char *label, const char *help,
                         unsigned value) noexcept;
};

/**
 * A choice of the configured devices whose driver can download
 * flights: "A: LX Nav", ... and "None".
 */
void
LoggerConfigPanel::AddRecorderDevice(const char *label, const char *help,
                                     unsigned value) noexcept
{
  WndProperty *wp = AddEnum(label, help);
  DataFieldEnum &df = *(DataFieldEnum *)wp->GetDataField();
  df.AddChoice(0, _("None"));

  const auto &devices = CommonInterface::GetSystemSettings().devices;
  for (unsigned i = 0; i < NUMDEV; ++i) {
    const DeviceConfig &config = devices[i];
    const DeviceRegister *driver = config.IsDisabled()
      ? nullptr
      : FindDriverByName(config.driver_name);

    /* the one set before stays in the list, even if it changed */
    if ((driver == nullptr || !driver->IsLogger()) && i + 1 != value)
      continue;

    StaticString<64> text;
    text.Format("%c: %s", 'A' + i,
                driver != nullptr ? driver->display_name
                                  : config.driver_name.c_str());
    df.AddChoice(i + 1, text);
  }

  df.SetValue(value);
  wp->RefreshDisplay();
}

static constexpr StaticEnumChoice auto_logger_list[] = {
  { LoggerSettings::AutoLogger::ON, N_("On") },
  { LoggerSettings::AutoLogger::START_ONLY, N_("Start only") },
  { LoggerSettings::AutoLogger::OFF, N_("Off") },
  nullptr
};

void
LoggerConfigPanel::Prepare(ContainerWindow &parent,
                           const PixelRect &rc) noexcept
{
  const ComputerSettings &settings_computer = CommonInterface::GetComputerSettings();
  const LoggerSettings &logger = settings_computer.logger;

  RowFormWidget::Prepare(parent, rc);
  AddText(_("Pilot name"),
          _("Name of the pilot in command, recorded in the IGC flight log."),
          logger.pilot_name);

  AddText(_("CoPilot name"),
          _("The co-pilot name recorded in the IGC flight log."),
          logger.copilot_name);

  AddFloat(_("Crew weight default"),
            _("Default for all weight loaded to the glider beyond the empty weight and besides "
                "the water ballast."),
            "%.0f %s", "%.0f",
            0, Units::ToUserMass(300), 5, false, UnitGroup::MASS,
            logger.crew_mass_template);

  AddDuration(_("Time step cruise"),
              _("This is the time interval between logged points when not circling."),
              seconds{1}, seconds{30}, seconds{1}, logger.time_step_cruise);
  SetExpertRow(LoggerTimeStepCruise);

  AddDuration(_("Time step circling"),
              _("This is the time interval between logged points when circling."),
              seconds{1}, seconds{30}, seconds{1}, logger.time_step_circling);
  SetExpertRow(LoggerTimeStepCircling);

  AddEnum(_("Auto. logger"),
          _("Enables the automatic starting and stopping of logger on takeoff and landing "
            "respectively. Disable when flying paragliders."),
          auto_logger_list, (unsigned)logger.auto_logger);
  SetExpertRow(DisableAutoLogger);

  AddBoolean(_("NMEA Logger"),
             _("Enable the NMEA logger on startup? If this option is disabled, "
                 "the NMEA logger can still be started manually."),
             logger.enable_nmea_logger);
  SetExpertRow(EnableNMEALogger);

  AddBoolean(_("Log book"),
             _("Records each flight in the log book: takeoff and landing "
               "with time and place, launch, crew, aircraft, free and DMSt "
               "distance (Info menu, file logbook.csv). At startup, the "
               "flights of recorded files not yet in the log book are "
               "added, the first time all of them."),
             logger.enable_flight_logger);

  const char *const recorder_help =
    _("An external flight recorder of this glider, as in the A record of "
      "its IGC files: the three letter manufacturer code and the serial "
      "number, e.g. LXVJNI. When a flight is recorded several times, the "
      "log book takes it from the file of Recorder 1, then Recorder 2, "
      "then the IGC file of this program.");

  const char *const device_help =
    _("The device the recorder is connected to; its driver is the type of "
      "the recorder. A while after the landing, the newest flight is "
      "downloaded from it into the log book.");

  AddText(_("Recorder 1"), recorder_help, logger.recorders[0]);
  AddButton(_("Recorder 1 from IGC file"),
            [this](){ ReadRecorderFromFile(Recorder1); });
  AddRecorderDevice(_("Recorder 1 device"), device_help,
                    logger.recorder_devices[0]);
  AddText(_("Recorder 2"), recorder_help, logger.recorders[1]);
  AddButton(_("Recorder 2 from IGC file"),
            [this](){ ReadRecorderFromFile(Recorder2); });
  AddRecorderDevice(_("Recorder 2 device"), device_help,
                    logger.recorder_devices[1]);

  AddText(_("Logger ID"),
          _("The three-letter logger ID used in the IGC filename."),
          logger.logger_id);
  SetExpertRow(LoggerID);
}

bool
LoggerConfigPanel::Save(bool &changed) noexcept
{
  ComputerSettings &settings_computer = CommonInterface::SetComputerSettings();
  LoggerSettings &logger = settings_computer.logger;

  changed |= SaveValue(PilotName, ProfileKeys::PilotName,
                       logger.pilot_name);

  changed |= SaveValue(CoPilotName, ProfileKeys::CoPilotName,
                       logger.copilot_name);

  changed |= SaveValue(CrewWeightTemplate, UnitGroup::MASS, ProfileKeys::CrewWeightTemplate,
                       logger.crew_mass_template);

  changed |= SaveValue(LoggerTimeStepCruise, ProfileKeys::LoggerTimeStepCruise,
                       logger.time_step_cruise);

  changed |= SaveValue(LoggerTimeStepCircling, ProfileKeys::LoggerTimeStepCircling,
                       logger.time_step_circling);

  /* GUI label is "Enable Auto Logger" */
  changed |= SaveValueEnum(DisableAutoLogger, ProfileKeys::AutoLogger,
                           logger.auto_logger);

  changed |= SaveValue(EnableNMEALogger, ProfileKeys::EnableNMEALogger,
                       logger.enable_nmea_logger);

  if (logger.enable_nmea_logger && backend_components->nmea_logger != nullptr)
    backend_components->nmea_logger->Enable();

  if (SaveValue(EnableFlightLogger, ProfileKeys::EnableFlightLogger,
                logger.enable_flight_logger)) {
    changed = true;

    /* currently, the GlueFlightLogger instance is created on startup
       only, which means XCSoar needs to be restarted to apply the new
       setting */
    require_restart = true;
  }

  /* stored as in the A record, so "lxv jni" works as well */
  for (unsigned i = 0; i < 2; ++i) {
    const auto id =
      Logbook::NormalizeRecorder(GetValueString(i == 0 ? Recorder1 : Recorder2));
    if (id != logger.recorders[i].c_str()) {
      logger.recorders[i] = id.c_str();
      Profile::Set(i == 0 ? ProfileKeys::Recorder1 : ProfileKeys::Recorder2,
                   logger.recorders[i]);
      changed = true;
    }
  }

  changed |= SaveValueEnum(Recorder1Device, ProfileKeys::Recorder1Device,
                           logger.recorder_devices[0]);
  changed |= SaveValueEnum(Recorder2Device, ProfileKeys::Recorder2Device,
                           logger.recorder_devices[1]);

  changed |= SaveValue(LoggerID, ProfileKeys::LoggerID, logger.logger_id);

  return true;
}

void
LoggerConfigPanel::ReadRecorderFromFile(unsigned row) noexcept
{
  FileDataField df;
  df.SetFileType(FileType::IGC);
  df.ScanMultiplePatterns("*.igc\0*.IGC\0");
  if (!FilePicker(_("IGC file"), df, nullptr, false))
    return;

  const auto path = df.GetValue();
  if (path == nullptr)
    return;

  const auto header = Logbook::ReadIgcHeader(path);
  if (header.recorder_code.empty()) {
    ShowMessageBox(_("This file has no A record with the recorder."),
                   _("IGC file"), MB_OK | MB_ICONERROR);
    return;
  }

  LoadValue(row, (header.recorder_code + header.recorder_serial).c_str());
}

std::unique_ptr<Widget>
CreateLoggerConfigPanel()
{
  return std::make_unique<LoggerConfigPanel>();
}
