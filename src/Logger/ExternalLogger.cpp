// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#include "Logger/ExternalLogger.hpp"
#include "Form/DataField/ComboList.hpp"
#include "Dialogs/Error.hpp"
#include "Dialogs/Message.hpp"
#include "Dialogs/ComboPicker.hpp"
#include "Language/Language.hpp"
#include "Device/Descriptor.hpp"
#include "Device/MultipleDevices.hpp"
#include "Device/RecordedFlight.hpp"
#include "Device/Features.hpp"
#include "Device/Port/State.hpp"
#include "Components.hpp"
#include "BackendComponents.hpp"
#include "LocalPath.hpp"
#include "Repository/FileType.hpp"
#include "UIGlobals.hpp"
#include "Operation/Cancelled.hpp"
#include "Operation/MessageOperationEnvironment.hpp"
#include "Dialogs/JobDialog.hpp"
#include "Job/TriStateJob.hpp"
#include "system/FileUtil.hpp"
#include "system/Path.hpp"
#include "io/FileLineReader.hpp"
#include "io/FileTransaction.hpp"
#include "IGC/IGCParser.hpp"
#include "IGC/IGCHeader.hpp"
#include "Formatter/IGCFilenameFormatter.hpp"
#include "time/BrokenDate.hpp"
#include "Interface.hpp"
#include "net/client/WeGlide/UploadIGCFile.hpp"
#include "Dialogs/LogbookDialog.hpp"

#include <cstring>


class DeclareJob {
  DeviceDescriptor &device;
  const struct Declaration &declaration;
  const Waypoint *home;

public:
  DeclareJob(DeviceDescriptor &_device, const struct Declaration &_declaration,
             const Waypoint *_home)
    :device(_device), declaration(_declaration), home(_home) {}

  bool Run(OperationEnvironment &env) {
    bool result = device.Declare(declaration, home, env);
    device.EnableNMEA(env);
    return result;
  }
};

static TriStateJobResult
DoDeviceDeclare(DeviceDescriptor &device, const Declaration &declaration,
                const Waypoint *home)
{
  TriStateJob<DeclareJob> job(device, declaration, home);
  JobDialog(UIGlobals::GetMainWindow(), UIGlobals::GetDialogLook(),
            "", job, true);
  return job.GetResult();
}

static bool
DeviceDeclare(DeviceDescriptor &dev, const Declaration &declaration,
              const Waypoint *home)
try {
  if (dev.IsOccupied())
    return false;

  if (ShowMessageBox(_("Declare task?"), dev.GetDisplayName(),
                  MB_YESNO | MB_ICONQUESTION) != IDYES)
    return false;

  if (!dev.Borrow())
    return false;

  MessageOperationEnvironment env;
  const ScopeReturnDevice return_device{dev, env};

  const char *caption = dev.GetDisplayName();
  if (caption == nullptr)
    caption = _("Declare task");

  auto result = DoDeviceDeclare(dev, declaration, home);

  switch (result) {
  case TriStateJobResult::SUCCESS:
    ShowMessageBox(_("Task declared!"),
                   caption, MB_OK | MB_ICONINFORMATION);
    return true;

  case TriStateJobResult::ERROR:
    ShowMessageBox(_("Error occurred,\nTask NOT declared!"),
                   caption, MB_OK | MB_ICONERROR);
    return false;

  case TriStateJobResult::CANCELLED:
    return false;
  }

  gcc_unreachable();
} catch (OperationCancelled) {
  return false;
} catch (...) {
  ShowError(_("Error occurred,\nTask NOT declared!"),
            std::current_exception(),
            dev.GetDisplayName());
  return false;
}

void
ExternalLogger::Declare(const Declaration &decl, const Waypoint *home)
{
  bool found_logger = false;

  for (DeviceDescriptor *i : *backend_components->devices) {
    DeviceDescriptor &device = *i;

    if (device.CanDeclare() && device.GetState() == PortState::READY) {
      found_logger = true;
      DeviceDeclare(device, decl, home);
    }
  }

  if (!found_logger)
    ShowMessageBox(_("No logger connected"),
                _("Declare task"), MB_OK | MB_ICONINFORMATION);
}

class ReadFlightListJob {
  DeviceDescriptor &device;
  RecordedFlightList &flight_list;

public:
  ReadFlightListJob(DeviceDescriptor &_device,
                    RecordedFlightList &_flight_list)
    :device(_device), flight_list(_flight_list) {}

  bool Run(OperationEnvironment &env) {
    return device.ReadFlightList(flight_list, env);
  }
};

static TriStateJobResult
DoReadFlightList(DeviceDescriptor &device, RecordedFlightList &flight_list)
{
  TriStateJob<ReadFlightListJob> job(device, flight_list);
  JobDialog(UIGlobals::GetMainWindow(), UIGlobals::GetDialogLook(),
            "", job, true);
  return job.GetResult();
}

class DownloadFlightJob {
  DeviceDescriptor &device;
  const RecordedFlightInfo &flight;
  const Path path;

public:
  DownloadFlightJob(DeviceDescriptor &_device,
                    const RecordedFlightInfo &_flight, const Path _path)
    :device(_device), flight(_flight), path(_path) {}

  bool Run(OperationEnvironment &env) {
    return device.DownloadFlight(flight, path, env);
  }
};

static TriStateJobResult
DoDownloadFlight(DeviceDescriptor &device,
                 const RecordedFlightInfo &flight, Path path)
{
  TriStateJob<DownloadFlightJob> job(device, flight, path);
  JobDialog(UIGlobals::GetMainWindow(), UIGlobals::GetDialogLook(),
            "", job, true);
  return job.GetResult();
}

static void
ReadIGCMetaData(Path path, IGCHeader &header, BrokenDate &date)
try {
  strcpy(header.manufacturer, "XXX");
  strcpy(header.id, "000");
  header.flight = 0;

  FileLineReaderA reader(path);

  char *line = reader.ReadLine();
  if (line != nullptr)
    IGCParseHeader(line, header);

  line = reader.ReadLine();
  if (line == nullptr || !IGCParseDateRecord(line, date))
    date = BrokenDate::TodayUTC();
} catch (...) {
  date = BrokenDate::TodayUTC();
}

/**
 *
 * @param list list of flights from the logger
 * @param flight the flight
 * @return 1-99 Flight number of the day per section 2.5 of the
 * FAI IGC tech gnss spec Appendix 1
 * (spec says 35 flights - this handles up to 99 flights per day)
 */
static unsigned
GetFlightNumber(const RecordedFlightList &flight_list,
                const RecordedFlightInfo &flight)
{
  unsigned flight_number = 1;
  for (auto it = flight_list.begin(), end = flight_list.end(); it != end; ++it) {
    const RecordedFlightInfo &_flight = *it;
    if (flight.date == _flight.date &&
        flight.start_time > _flight.start_time)
      flight_number++;
  }
  return flight_number;
}

/**
 * The newest flight of the list: the one just landed.
 */
[[gnu::pure]]
static unsigned
FindNewestFlight(const RecordedFlightList &flight_list) noexcept
{
  unsigned newest = 0;
  for (unsigned i = 1; i < flight_list.size(); ++i) {
    const auto &a = flight_list[i], &b = flight_list[newest];
    if (b.date < a.date ||
        (a.date == b.date && b.start_time < a.start_time))
      newest = i;
  }
  return newest;
}

static const RecordedFlightInfo *
ShowFlightList(const RecordedFlightList &flight_list)
{
  // Prepare list of the flights for displaying
  ComboList combo;
  for (unsigned i = 0; i < flight_list.size(); ++i) {
    const RecordedFlightInfo &flight = flight_list[i];

    StaticString<64> buffer;
    if (flight.date.IsPlausible())
      buffer.UnsafeFormat("%04u/%02u/%02u %02u:%02u-%02u:%02u",
                          flight.date.year, flight.date.month, flight.date.day,
                          flight.start_time.hour, flight.start_time.minute,
                          flight.end_time.hour, flight.end_time.minute);
    else
      buffer.UnsafeFormat("----/--/-- %02u:%02u-%02u:%02u",
                          flight.start_time.hour, flight.start_time.minute,
                          flight.end_time.hour, flight.end_time.minute);

    combo.Append(i, buffer);
  }

  /* the flight just landed is the one the pilot wants most of the
     time */
  combo.current_index = FindNewestFlight(flight_list);

  // Show list of the flights
  int i = ComboPicker("Choose a flight",
                      combo, nullptr, false);

  return i < 0 ? nullptr : &flight_list[i];
}

/**
 * Read the list of flights from the logger.
 *
 * @param quiet no message box for an error or an empty logger (an
 * automatic download)
 * @return false on error, cancel or an empty logger
 */
static bool
ReadFlightList(DeviceDescriptor &device, RecordedFlightList &flight_list,
               bool quiet)
{
  try {
    switch (DoReadFlightList(device, flight_list)) {
    case TriStateJobResult::SUCCESS:
      break;

    case TriStateJobResult::ERROR:
      if (!quiet)
        ShowMessageBox(_("Failed to download flight list."),
                       _("Download flight"), MB_OK | MB_ICONERROR);
      return false;

    case TriStateJobResult::CANCELLED:
      return false;
    }
  } catch (OperationCancelled) {
    return false;
  } catch (...) {
    if (!quiet)
      ShowError(_("Failed to download flight list."),
                std::current_exception(),
                _("Download flight"));
    return false;
  }

  if (flight_list.empty()) {
    if (!quiet)
      ShowMessageBox(_("Logger is empty."),
                     _("Download flight"), MB_OK | MB_ICONINFORMATION);
    return false;
  }

  return true;
}

/**
 * Download one flight into the folder of the IGC files, named after
 * its header.
 *
 * @return the file, nullptr on error or cancel
 */
static AllocatedPath
DownloadOneFlight(DeviceDescriptor &device,
                  const RecordedFlightList &flight_list,
                  const RecordedFlightInfo &flight, bool quiet)
{
  const auto igc_dir = LocalPath(GetFileTypeDefaultDir(FileType::IGC));
  Directory::CreateRecursive(igc_dir);

  // Download chosen IGC file into temporary file
  FileTransaction transaction(AllocatedPath::Build(igc_dir, "temp.igc"));

  try {
    switch (DoDownloadFlight(device, flight, transaction.GetTemporaryPath())) {
    case TriStateJobResult::SUCCESS:
      break;

    case TriStateJobResult::ERROR:
      if (!quiet)
        ShowMessageBox(_("Failed to download flight."),
                       _("Download flight"), MB_OK | MB_ICONERROR);
      return nullptr;

    case TriStateJobResult::CANCELLED:
      return nullptr;
    }
  } catch (OperationCancelled) {
    return nullptr;
  } catch (...) {
    if (!quiet)
      ShowError(_("Failed to download flight."),
                std::current_exception(),
                _("Download flight"));
    return nullptr;
  }

  /* read the IGC header and build the final IGC file name with it */

  IGCHeader header;
  BrokenDate date;
  ReadIGCMetaData(transaction.GetTemporaryPath(), header, date);
  if (header.flight == 0)
    header.flight = GetFlightNumber(flight_list, flight);

  char name[64];
  FormatIGCFilenameLong(name, date, header.manufacturer, header.id,
                        header.flight);

  auto igc_path = AllocatedPath::Build(igc_dir, name);
  transaction.SetPath((Path)igc_path);

  try {
    transaction.Commit();
  } catch (...) {
    if (!quiet)
      ShowError(std::current_exception(), _("Download flight"));
    return nullptr;
  }

  return igc_path;
}

/**
 * After a download (and after the log book update, which the upload
 * needs to note the flight id): offer the upload to WeGlide, if the
 * pilot wants to be asked.
 */
static void
AfterDownload(Path igc_path)
{
  WeGlideSettings weglide_settings =
    CommonInterface::GetComputerSettings().weglide;
  if (weglide_settings.enabled && weglide_settings.automatic_upload &&
    weglide_settings.pilot_id > 0) {
    // ask whether this IGC should be uploaded to WeGlide
    if (ShowMessageBox(_("Do you want to upload this flight to WeGlide?"),
      _("Upload Flight"), MB_YESNO | MB_ICONQUESTION) == IDYES) {
      WeGlide::UploadIGCFile(igc_path);
    }
  }
}

class ScopeEnableSecondDeviceNMEA {
  DeviceDescriptor &device;
  OperationEnvironment &env;

public:
  ScopeEnableSecondDeviceNMEA(DeviceDescriptor &_device,
                              OperationEnvironment &_env) noexcept
    :device(_device), env(_env) {}

  ~ScopeEnableSecondDeviceNMEA() noexcept {
    (void)device.EnableSecondDeviceNMEA(env);
  }
};

void
ExternalLogger::DownloadFlightFrom(DeviceDescriptor &device)
{
  MessageOperationEnvironment env;
  const ScopeEnableSecondDeviceNMEA enable_second_device_nmea{device, env};

  // Download the list of flights that the logger contains
  RecordedFlightList flight_list;
  if (!ReadFlightList(device, flight_list, false))
    return;

  while (true) {
    // Show list of the flights
    const RecordedFlightInfo *flight = ShowFlightList(flight_list);
    if (!flight)
      break;

    const auto igc_path = DownloadOneFlight(device, flight_list, *flight,
                                            false);
    if (igc_path == nullptr)
      continue;

    /* the log book first, so the WeGlide upload finds the entry for
       its flight id */
    UpdateLogbook();
    AfterDownload(igc_path);

    if (ShowMessageBox(_("Do you want to download another flight?"),
                    _("Download flight"), MB_YESNO | MB_ICONQUESTION) != IDYES)
      break;
  }
}

bool
ExternalLogger::DownloadNewestFlight(DeviceDescriptor &device,
                                     const char *recorder)
{
  MessageOperationEnvironment env;
  const ScopeEnableSecondDeviceNMEA enable_second_device_nmea{device, env};

  RecordedFlightList flight_list;
  if (!ReadFlightList(device, flight_list, true))
    return false;

  const auto &flight = flight_list[FindNewestFlight(flight_list)];

  /* the file name follows from the recorder and the flight: if it is
     there, this flight was downloaded before */
  if (recorder != nullptr && strlen(recorder) >= 6 && flight.date.IsPlausible()) {
    char manufacturer[4] = {recorder[0], recorder[1], recorder[2], 0};
    char id[4] = {recorder[3], recorder[4], recorder[5], 0};
    char name[64];
    FormatIGCFilenameLong(name, flight.date, manufacturer, id,
                          GetFlightNumber(flight_list, flight));
    if (File::Exists(AllocatedPath::Build(LocalPath(GetFileTypeDefaultDir(FileType::IGC)),
                                          name)))
      return false;
  }

  const auto igc_path = DownloadOneFlight(device, flight_list, flight, true);
  if (igc_path == nullptr)
    return false;

  UpdateLogbook();
  AfterDownload(igc_path);
  return true;
}

/**
 * The device of Recorder 1 or 2, nullptr if none is set or there is no
 * such device.
 */
static DeviceDescriptor *
GetRecorderDevice(unsigned index) noexcept
{
  if (index >= 2 || backend_components == nullptr ||
      backend_components->devices == nullptr)
    return nullptr;

  const unsigned slot =
    CommonInterface::GetComputerSettings().logger.recorder_devices[index];
  if (slot == 0 || slot > NUMDEV)
    return nullptr;

  return &(*backend_components->devices)[slot - 1];
}

bool
ExternalLogger::IsRecorderReady(unsigned index) noexcept
{
  const DeviceDescriptor *device = GetRecorderDevice(index);
  return device != nullptr && device->IsLogger() &&
    device->GetState() == PortState::READY;
}

bool
ExternalLogger::DownloadFromRecorder(unsigned index, bool automatic)
{
  DeviceDescriptor *device = GetRecorderDevice(index);
  if (device == nullptr) {
    if (!automatic)
      ShowMessageBox(_("No device is set for this recorder in the logger "
                       "settings."),
                     _("Download flight"), MB_OK | MB_ICONINFORMATION);
    return false;
  }

  if (!device->IsLogger() || device->GetState() != PortState::READY) {
    if (!automatic)
      ShowMessageBox(_("Device is not connected"), _("Download flight"),
                     MB_OK | MB_ICONERROR);
    return false;
  }

  if (!device->Borrow()) {
    if (!automatic)
      ShowMessageBox(_("Device is occupied"), _("Download flight"),
                     MB_OK | MB_ICONERROR);
    return false;
  }

  MessageOperationEnvironment env;
  const ScopeReturnDevice return_device{*device, env};

  if (!automatic) {
    DownloadFlightFrom(*device);
    return false;
  }

  return DownloadNewestFlight(*device,
                              CommonInterface::GetComputerSettings().logger.recorders[index].c_str());
}
