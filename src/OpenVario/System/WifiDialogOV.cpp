// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#include "OpenVario/System/WifiDialogOV.hpp"
#include "OpenVario/System/WifiSupplicantOV.hpp"
#include "Dialogs/WidgetDialog.hpp"
#include "Dialogs/Message.hpp"
#include "Dialogs/Error.hpp"
#include "Dialogs/TextEntry.hpp"
#include "UIGlobals.hpp"
#include "Look/DialogLook.hpp"
#include "ui/canvas/Canvas.hpp"
#include "Screen/Layout.hpp"
#include "Renderer/TwoTextRowsRenderer.hpp"
#include "Language/Language.hpp"
#include "Widget/ListWidget.hpp"
#include "net/IPv4Address.hxx"
#include "ui/event/PeriodicTimer.hpp"
#include "util/HexFormat.hxx"
#include "util/StaticString.hxx"
#include "util/ConvertString.hpp"

#include "LocalPath.hpp"
#include "LogFile.hpp"
#include "system/Process.hpp"
#include "OpenVario/System/OpenVarioDevice.hpp"
#include "system/FileUtil.hpp"
#include "io/FileReader.hxx"
#include "io/ProgressReader.hpp"
#include "io/BufferedReader.hxx"
#include "io/StringConverter.hpp"
#include "io/ProgressReader.hpp"
#include "util/StringStrip.hxx"
#include "util/StringCompare.hxx"
#include "util/StringAPI.hxx"
// #include "Operation/ConsoleOperationEnvironment.hpp"
#include "Operation/Operation.hpp"
#include "Operation/ProgressListener.hpp"
// #include "lib/fmt/RuntimeError.hxx"

#include <filesystem>
#include <thread>
#if DEBUG_TEST_VERSION
# include <iostream>
#endif

using std::string_view_literals::operator""sv;

const char *const connmanctl = "/usr/bin/connmanctl";

#ifdef KOBO
#include "Model.hpp"
#else
static const char *
GetKoboWifiInterface() noexcept
{
  /* dummy implementation for builds on (regular) Linux so KoboMenu
     can be debugged there */
  return "dummy";
}
#endif

#ifdef WithWPA
/* workaround because OpenSSL has a typedef called "UI", which clashes
   with our "UI" namespace */
#define UI OPENSSL_UI
#include <openssl/evp.h> // for PKCS5_PBKDF2_HMAC_SHA1()
#undef UI
#endif // WithWPA

class WifiListWidget final
  : public ListWidget {

  struct NetworkInfo {
    NarrowString<64> mac_id;
    NarrowString<64> bssid;
    NarrowString<256> ssid;
    NarrowString<256> base_id;
    int signal_level;
    int id;

    enum WifiSecurity security;

    bool old_visible, old_configured;
    bool enabled = false;   // '*' - 1st char
    bool coupled = false;   // 'A' - 2nd char
    bool connected = false; // 'R' - 3rd char;
  };

  Button *scan_button;
  Button *reconnect_button;
  Button *connect_button;

  WifiStatus status;

  TrivialArray<NetworkInfo, 64> networks;

  TwoTextRowsRenderer row_renderer;

//  WPASupplicant wpa_supplicant;

  UI::PeriodicTimer update_timer{[this]{ UpdateList(); }};

public:
  void CreateButtons(WidgetDialog &dialog) {
    scan_button = dialog.AddButton(_("Scan"), [this](){
      try {
        EnsureConnected();
        // wpa_supplicant.Scan();
        ScanWifi();
        UpdateList();
      } catch (...) {
        ShowError(std::current_exception(), _("Error"));
      }
    });

    reconnect_button = dialog.AddButton(_("Re-Connect"), [this]() {
      try {
        ReConnect();
      } catch (...) {
        ShowError(std::current_exception(), _("Error"));
      }
    });

    connect_button = dialog.AddButton(_("Connect"), [this]() {
      try {
        Connect();
      } catch (...) {
        ShowError(std::current_exception(), _("Error"));
      }
    });
  }

  void UpdateButtons();
  void ScanWifi();

  /* virtual methods from class Widget */
  void Prepare(ContainerWindow &parent,
               const PixelRect &rc) noexcept override {
    const DialogLook &look = UIGlobals::GetDialogLook();

    CreateList(parent, look, rc,
               row_renderer.CalculateLayout(look.text_font,
                                            look.small_font));
    // insert from August2111
    ScanWifi();

    UpdateList();
    update_timer.Schedule(std::chrono::seconds(1));
  }

  /* virtual methods from class ListItemRenderer */
  void OnPaintItem(Canvas &canvas, const PixelRect rc,
                   unsigned idx) noexcept override;

  /* virtual methods from class ListCursorHandler */
  void OnCursorMoved([[maybe_unused]] unsigned index) noexcept override {
    UpdateButtons();
  }

  // add August2111:
  void WifiConnect(enum WifiSecurity security,
                                   // WPASupplicant &wpa_supplicant,
                                   const char *ssid, const char *psk);
  void WifiDisconnect(const char *ssid);

  private:
  /**
   * Ensure that we're connected to wpa_supplicant.
   *
   * Throws on error.
   */
  void EnsureConnected();

  [[gnu::pure]]
  NetworkInfo *FindByID(int id) noexcept;

  [[gnu::pure]]
  NetworkInfo *FindByBSSID(const char *bssid) noexcept;

  [[gnu::pure]]
  NetworkInfo *FindVisibleBySSID(const char *ssid) noexcept;

  [[gnu::pure]]
  NetworkInfo *Find(const WifiConfiguredNetworkInfo &c) noexcept;

  void MergeList(const WifiVisibleNetwork *p, unsigned n);
  void Append(const WifiConfiguredNetworkInfo &src);
  void Merge(const WifiConfiguredNetworkInfo &c);
  void MergeList(const WifiConfiguredNetworkInfo *p, unsigned n);
  void UpdateScanResults();
  void UpdateConfigured();
  void SweepList();
  void UpdateList();

  void Connect();
  void ReConnect();
};

void 
WifiListWidget::ScanWifi()
{
#ifdef __MSVC__
  char buffer[0x1000];
  auto file = Path(_T("connman-scan-results.txt"));
  File::ReadString(Path(_T("/Data/connman-services.txt")), buffer, sizeof(buffer));
  File::CreateExclusive(file);
  File::WriteExisting(file, buffer);
#else
  Run(Path(_T("connman-technologies.txt")), connmanctl, "technologies");
  Run(Path(_T("connman-enable.txt")), connmanctl, "enable", "wifi");
  Run(Path(_T("connman-scan.txt")), connmanctl, "scan", "wifi");
  Run(Path(_T("connman-scan-results.txt")), connmanctl, "services");
#endif
}

void
WifiListWidget::UpdateButtons()
{
  const unsigned cursor = GetList().GetCursorIndex();

  if (cursor < networks.size()) {
    const auto &info = networks[cursor];

//    if (info.id >= 0) {
    if (info.connected) {
      // connect_button->SetCaption(_("Remove"));
      connect_button->SetCaption(_("Disconnect"));
      connect_button->SetEnabled(true);
    } else if (info.signal_level >= 0) {
      connect_button->SetCaption(_("Connect"));
      connect_button->SetEnabled(true);
    }
  } else {
    connect_button->SetEnabled(false);
  }
}

void
WifiListWidget::OnPaintItem(Canvas &canvas, const PixelRect rc,
                            unsigned idx) noexcept
{
  const auto &info = networks[idx];

  static char wifi_security[][20] = {
    "WPA",
    "WEP",
    "Open",
  };

  row_renderer.DrawFirstRow(canvas, rc, _W(info.ssid));
  row_renderer.DrawSecondRow(canvas, rc,_W(info.bssid));

  const TCHAR *state = nullptr;
  StaticString<40> state_buffer;

  /* found the currently connected wifi network? */
  if (StringIsEqual(info.bssid.c_str(), status.bssid.c_str())) {
    state = _("Connected");

    /* look up ip address for wlan0 or eth0 */
#ifdef _WIN32
    const auto addr = IPv4Address(192, 168, 0, 1, 0);
    if (addr.IsDefined()) { /* valid address? */
      // StaticString<40> addr_str;
      StaticString<40> addr_str;
      addr_str = _T("192.186.0.1");
      state_buffer.Format(_T("%s (%s)"), state, addr_str.c_str());
      state = state_buffer;
    }
#else
    // const auto addr = IPv4Address::GetDeviceAddress(GetKoboWifiInterface());
    const auto addr = IPv4Address::GetDeviceAddress("wlan0");
    if (addr.IsDefined()) { /* valid address? */
      // StaticString<40> addr_str;
      StaticString<40> addr_str;
      if (addr.ToString(addr_str.buffer(), addr_str.capacity()) != nullptr) {
        state_buffer.Format(_T("%s (%s)"), state, addr_str.c_str());
        state = state_buffer;
      }
    }
#endif
  }
#if 0
  else if (info.id >= 0)
    state = info.signal_level >= 0
      ? _("Saved and visible")
      : _("Saved, but not visible");
  else if (info.signal_level >= 0)
    state = _("Visible");
#else
  else if (info.connected)
    state = _("Connected");
  else if (info.coupled)
    state = _("Saved and visible");
  else if (info.enabled) 
    state =  _("Saved, but not visible");  // ?
  else 
    state = _("Visible");
#endif

  if (state != nullptr)
    row_renderer.DrawRightFirstRow(canvas, rc, state);

  if (info.signal_level >= 0) {
    StaticString<32> text;
    text.UnsafeFormat(_T("%s %u"), _W(wifi_security[info.security]),
                      info.signal_level);
    row_renderer.DrawRightSecondRow(canvas, rc, text);
  }
}

// TODO August2111: coming fro WPA-Sup..??
// static 
void WifiListWidget::WifiDisconnect( const char *ssid) {
  auto network = FindVisibleBySSID(ssid);
//  StaticString<0x100> base_id;
//  base_id.Format(_T("wifi_%s_%s_managed_"), _W(network->mac_id.c_str()),
//                 _W(network->bssid.c_str()));
#if defined(IS_OPENVARIO_CB2)
  // disconnect port
  Run(Path(_T("wifi-disconnect.txt")), connmanctl, "disconnect", network->base_id.c_str());
#endif
  ShowMessageBox(_W(network->base_id.c_str()), _T("Disconnected"), MB_OK);
}

void WifiListWidget::WifiConnect(enum WifiSecurity security,
                                        // WPASupplicant &wpa_supplicant,
                                        const char *ssid, const char *psk)
{
  {
//    ShowMessageBox(_W(psk), _T("Wifi-Passphrase"), MB_OK);

    auto network = FindVisibleBySSID(ssid);
    std::cout << "Test: " << 1 << std::endl;
//    StaticString<0x100> base_id;
//    base_id.Format(_T("wifi_%s_%s_managed_"), _W(network->mac_id.c_str()),
//                   _W(network->bssid.c_str()));

//    std::cout << "Test: " << 2 << std::endl;
//    switch (network->security) {
//    case WPA_SECURITY:
//      base_id.append(_T("psk"));
//      break;
//    case WEP_SECURITY:
//      base_id.append(_T("wep"));
//      break;
//    case OPEN_SECURITY:
//    default:
//      base_id.append(_T("none"));
//      break;
//    }

#if 0  // content of 'settings' file:
      [wifi_8c883b0078ce_537573616e6e65204361626c652047617374_managed_psk]
      Name=Susanne Cable Gast
      SSID=537573616e6e65204361626c652047617374
      Frequency=2412
      Favorite=true
      AutoConnect=true
      Modified=2024-01-31T13:55:30Z
      Passphrase=LibeLLe7B
      IPv4.method=dhcp
      IPv4.DHCP.LastAddress=192.168.179.2
      IPv6.method=off
      IPv6.privacy=disabled
#endif // WithWPA
    StaticString<0x1000> buffer;
    buffer.Format(_T("[%s]\n"), _W(network->base_id.c_str()));
    buffer.AppendFormat(_T("Type=%s\n"), _W("wifi"));
    buffer.AppendFormat(_T("Name=%s\n"), _W(ssid));
    buffer.AppendFormat(_T("SSID=%s\n"),
                        _W(network->bssid.c_str())); // _W(network->bssid);
    buffer.AppendFormat(_T("Frequency=%d\n"), 2412);
    // buffer.AppendFormat(_T("Favorite=true\n"));
    buffer.append(_T("Favorite=true\n"));
    buffer.append(_T("AutoConnect=true\n"));
    buffer.AppendFormat(_T("Passphrase=%s\n"), _W(psk));
    //    buffer.AppendFormat(_T("Modified=2024-02-01T12:38:31Z\n"));
    buffer.AppendFormat(_T("IPv4.method=%s\n"), _T("dhcp"));
    //    buffer.AppendFormat(_T("IPv4.DHCP.LastAddress=192.168.178.32\n"));
    buffer.append(_T("IPv6.method=off\n"));
    buffer.append(_T("IPv6.privacy=disabled\n"));
    std::cout << "Test: " << 6 << std::endl;
    std::cout << _A(buffer.c_str()) << std::endl;
    ShowMessageBox(buffer.c_str(), _T("WifiConnect"), MB_OK);
    std::cout << "Test: " << 7 << std::endl;

    Path base_id(_W(network->base_id.c_str()));

#if defined(IS_OPENVARIO_CB2)
    // save on the connman setting location:
    auto ssid_path =
        AllocatedPath::Build(Path(_T("/var/lib/connman")), base_id);
#else
    auto ssid_path = AllocatedPath::Build(ovdevice.GetDataPath(),
                                          base_id);
    ssid_path = AllocatedPath::Build(ssid_path, base_id);
#endif
    auto setting_file = AllocatedPath::Build(ssid_path, Path(_T("settings")));
    if (File::Exists(setting_file))
      File::Delete(setting_file);
    else
      Directory::Create(ssid_path);
    File::CreateExclusive(setting_file);
    File::WriteExisting(setting_file, _A(buffer.c_str()));

#if defined(IS_OPENVARIO_CB2)
    // disable wifi
    Run(Path(_T("wifi-disable.txt")), connmanctl, "disable", "wifi");
    // wait a second
    std::this_thread::sleep_for(std::chrono::seconds(1));
    // enable wifi again for AutoConnect
    Run(Path(_T("wifi-enable.txt")), connmanctl, "enable", "wifi");
    // wait a second after wifi enabling (?)
    std::this_thread::sleep_for(std::chrono::seconds(1));
    // ask state of the wifi connection
    Run(Path(_T("wifi-connect.txt")), connmanctl, "services",
        network->base_id.c_str());
#endif
  }
#ifdef WithWPA
  const unsigned id = wpa_supplicant.AddNetwork();
  char *endPsk_ptr;

  wpa_supplicant.SetNetworkSSID(id, ssid);

  if (security == WPA_SECURITY) {
    std::array<std::byte, 32> pmk;
    PKCS5_PBKDF2_HMAC_SHA1(psk, strlen(psk),
                           (const unsigned char *)ssid, strlen(ssid),
                           4096,
                           pmk.size(), (unsigned char *)pmk.data());

    std::array<char, sizeof(pmk) * 2 + 1> hex;
    *HexFormat(hex.data(), pmk) = 0;

    wpa_supplicant.SetNetworkPSK(id, hex.data());
  } else if (security == WEP_SECURITY) {
    wpa_supplicant.SetNetworkID(id, "key_mgmt", "NONE");

    /*
     * If psk is all hexidecimal should SetNetworkID, assuming user provided key in hex.
     * Use strtoll to confirm the psk is entirely in hex.
     * Also to need to check that it does not begin with 0x which WPA supplicant does not like.
     */

    (void) strtoll(psk, &endPsk_ptr, 16);

    if ((*endPsk_ptr == '\0') &&                                   // confirm strtoll processed all of psk
        (strlen(psk) >= 2) && (psk[0] != '0') && (psk[1] != 'x'))  // and the first two characters were no "0x"
      wpa_supplicant.SetNetworkID(id, "wep_key0", psk);
    else
      wpa_supplicant.SetNetworkString(id, "wep_key0", psk);

    wpa_supplicant.SetNetworkID(id, "auth_alg", "OPEN\tSHARED");
  } else if (security == OPEN_SECURITY){
    wpa_supplicant.SetNetworkID(id, "key_mgmt", "NONE");
  } else
    throw std::runtime_error{"Unsupported Wifi security type"};

  wpa_supplicant.EnableNetwork(id);
  wpa_supplicant.SaveConfig();
#endif // WithWPA
}

inline void
WifiListWidget::ReConnect()
{
  EnsureConnected();

  const unsigned i = GetList().GetCursorIndex();
  if (i >= networks.size())
    return;

#if defined(IS_OPENVARIO_CB2)
  // disable wifi
  Run(Path(_T("wifi-disable.txt")), connmanctl, "disable", "wifi");
  // wait a second
  std::this_thread::sleep_for(std::chrono::seconds(1));
  // enable wifi again for AutoConnect
  Run(Path(_T("wifi-enable.txt")), connmanctl, "enable", "wifi");
  // wait a second after wifi enabling (?)
  std::this_thread::sleep_for(std::chrono::seconds(1));
//  // ask state of the wifi connection
//  Run(Path(_T("wifi-connect.txt")), connmanctl, "services",
//      network->base_id.c_str());
#endif

  UpdateList();
}

inline void
WifiListWidget::Connect()
{
  EnsureConnected();

  const unsigned i = GetList().GetCursorIndex();
  if (i >= networks.size())
    return;

  const auto &info = networks[i];
//  if (info.id < 0) {
  if (info.connected) {
    WifiDisconnect(info.ssid);
  } else {
    const auto ssid = info.ssid;

    StaticString<256> caption;
    caption.Format(_("Passphrase of network '%s'"), _W(ssid.c_str()));

    StaticString<32> passphrase;
    passphrase.clear();
    if (info.security == OPEN_SECURITY)
      passphrase.clear();
    else if (!TextEntryDialog(passphrase, caption, false))
      return;

    WifiConnect(info.security, /* wpa_supplicant, */ info.ssid,
        _A(passphrase.c_str()));
//  } else {
//    wpa_supplicant.RemoveNetwork(info.id);
//    wpa_supplicant.SaveConfig();
  }

  UpdateList();
}

void
WifiListWidget::EnsureConnected()
{
  char path[64];
  sprintf(path, "/var/run/wpa_supplicant/%s", GetKoboWifiInterface());
//  wpa_supplicant.EnsureConnected(path);
}

inline WifiListWidget::NetworkInfo *
WifiListWidget::FindByID(int id) noexcept
{
  auto f = std::find_if(networks.begin(), networks.end(),
                        [id](const NetworkInfo &info) {
                          return info.id == id;
                        });
  if (f == networks.end())
    return nullptr;

#ifdef __MSVC__
  return &(*f);
#else
  return f;
#endif
}

WifiListWidget::NetworkInfo *
WifiListWidget::FindByBSSID(const char *bssid) noexcept
{
  auto f = std::find_if(networks.begin(), networks.end(),
                        [bssid](const NetworkInfo &info) {
                          return info.bssid == bssid;
                        });
  if (f == networks.end())
    return nullptr;
#ifdef __MSVC__
  return &(*f);
#else
  return f;
#endif
}

WifiListWidget::NetworkInfo *
WifiListWidget::FindVisibleBySSID(const char *ssid) noexcept
{
  auto f = std::find_if(networks.begin(), networks.end(),
                        [ssid](const NetworkInfo &info) {
                          return info.signal_level >= 0 && info.ssid == ssid;
                        });
  if (f == networks.end())
    return nullptr;

#ifdef __MSVC__
  return &(*f);
#else
  return f;
#endif
}

inline void
WifiListWidget::MergeList(const WifiVisibleNetwork *p, unsigned n)
{
  for (unsigned i = 0; i < unsigned(n); ++i) {
    const auto &found = p[i];

    auto info = FindByBSSID(found.bssid);
    if (info == nullptr) {
      info = &networks.append();
      info->bssid = found.bssid;
      info->id = -1;
    }

    info->mac_id = found.mac_id;
    info->ssid = found.ssid;
    info->signal_level = found.signal_level;
    info->base_id = found.base_id;
    info->security = found.security;
    info->enabled = found.enabled;
    info->coupled = found.coupled; 
    info->connected = found.connected;
    info->old_visible = false;
  }

#ifndef WithWPA

#endif


}

size_t 
ParseConnmanScan(WifiVisibleNetwork *dest, BufferedReader &reader,
                      [[maybe_unused]] OperationEnvironment &env, size_t max) {
  StringConverter string_converter;

  bool ignore = false;
  const char *line;
  size_t n = 0;

  // Iterate through the lines
  while ((line = reader.ReadLine()) != nullptr) {
    StripRight(line);

    // Skip empty line
    if (StringIsEmpty(line))
      continue;
    if (StringLength(line) < 4)
      continue;

    auto info = &dest[n];
    std::string_view state{line, 4};
    line += 4;
    auto pos = StringFind(line, " wifi_");
    if (pos == nullptr)
      continue;
    //-------------------------------------
    std::string_view base_id{pos + 1};
    info->base_id = StripRight(base_id);

    info->enabled = state[0] == '*'; // '*' - 1st char
    info->coupled = state[1] == 'A'; // 'A' - 2nd char
    info->connected = state[2] == 'R';  // 'R' - 3rd char;
    // SSID
    if (pos - line > 2) {
      std::string_view ssid{line, (size_t)(pos - line)};
      info->ssid = StripRight(ssid);
    }
    line = pos + StringLength(" wifi_");

    pos = StringFind(line, "_");
    if (pos - line > 2) {
      std::string_view bssid{line, (size_t)(pos - line)};
      info->bssid = StripRight(bssid);
      std::string_view mac_id{line, (size_t)(pos - line)};
      info->mac_id = StripRight(mac_id);
    }
    line = pos +1;

    pos = StringFind(line, "_managed");
    if (pos - line > 2) {
      std::string_view bssid{line, (size_t)(pos - line)};
      info->bssid = StripRight(bssid);
    }
    line = pos + StringLength("_managed") +1;

    info->signal_level = 1;  // signal level not supported up to now ?-1;


    if (line == "psk"sv)
      info->security = WPA_SECURITY;
      // info->security = OPEN_SECURITY;
    else if (line == "wep"sv)
      info->security = WEP_SECURITY;
    else if (line ==  "open"sv)
      info->security = OPEN_SECURITY;

    if (++n >= max)
        break;
  }
  return n;
}

static size_t 
ParseConnmanScan(WifiVisibleNetwork *dest, Path path,
                             OperationEnvironment &env, size_t max) noexcept
try {
  FileReader file_reader{path};
  ProgressReader progress_reader{file_reader, file_reader.GetSize(), env };
  BufferedReader buffered_reader{progress_reader};
  size_t n = 0;
  try {
    n = ParseConnmanScan(dest, buffered_reader, env, max);
  } catch (...) {
//    std::throw_with_nested(FmtRuntimeError("Error in file {}", path));
  }

  return n;
} catch (...) {
  LogError(std::current_exception());
  // operation.SetError(std::current_exception());
  return 0;
}

#ifndef WithWPA
unsigned 
ScanResults(WifiVisibleNetwork *dest, unsigned max) 
{
  const Path file = Path(_T("connman-scan-results.txt"));
  if (File::Exists(file)) {
    // ConsoleOperationEnvironment env;
    NullOperationEnvironment env;
    auto n = ParseConnmanScan(dest, file, env, max);

    auto file2 = Path(_T("connman-services.txt"));
    if (File::Exists(file2))
        File::Delete(file2);
    File::Rename(file, file2);
    //    File::Delete(file);
    return n;
  } else {
    return 0;
  }
}
#endif

inline void
WifiListWidget::UpdateScanResults()
{
  WifiVisibleNetwork *buffer = new WifiVisibleNetwork[networks.capacity()];

#ifdef WithWPA
  int n = wpa_supplicant.ScanResults(buffer, networks.capacity());
#else
  auto n = ScanResults(buffer, networks.capacity());
#endif
  // int n = networks.capacity();
  if (n >= 0)
    MergeList(buffer, n);

  delete[] buffer;
}

inline WifiListWidget::NetworkInfo *
WifiListWidget::Find(const WifiConfiguredNetworkInfo &c) noexcept
{
  auto found = FindByID(c.id);
  if (found != nullptr)
    return found;

  return c.bssid == "any"
    ? FindVisibleBySSID(c.ssid)
    : FindByBSSID(c.bssid);
}

inline void
WifiListWidget::Append(const WifiConfiguredNetworkInfo &src)
{
  auto &dest = networks.append();
  dest.bssid = src.bssid;
  dest.ssid = src.ssid;
  dest.id = src.id;
  dest.signal_level = -1;
  dest.old_configured = false;
}

inline void
WifiListWidget::Merge(const WifiConfiguredNetworkInfo &c)
{
  auto found = Find(c);
  if (found != nullptr) {
    found->id = c.id;
    found->old_configured = false;
  } else
    Append(c);
}

inline void
WifiListWidget::MergeList(const WifiConfiguredNetworkInfo *p, unsigned n)
{
  for (unsigned i = 0; i < unsigned(n); ++i)
    Merge(p[i]);
}

inline void
WifiListWidget::UpdateConfigured()
{
  WifiConfiguredNetworkInfo *buffer =
    new WifiConfiguredNetworkInfo[networks.capacity()];
  int n = 0;
    //wpa_supplicant.ListNetworks(buffer, networks.capacity());
  if (n >= 0)
    MergeList(buffer, n);

  delete[] buffer;
}

inline void
WifiListWidget::SweepList()
{
  unsigned cursor = GetList().GetCursorIndex();

  for (int i = networks.size() - 1; i >= 0; --i) {
    NetworkInfo &info = networks[i];

    if (info.old_visible && info.old_configured) {
      networks.remove(i);
      if (cursor > unsigned(i))
        --cursor;
    } else {
      if (info.old_visible)
        info.signal_level = -1;

      if (info.old_configured)
        info.id = -1;
    }
  }

  GetList().SetCursorIndex(cursor);
}

void
WifiListWidget::UpdateList()
{
  // std::cout << "UpdateList!" << std::endl;
  status.Clear();

  try {
// WPA?    EnsureConnected();
// WPA?    wpa_supplicant.Status(status);

    for (auto &i : networks)
      i.old_visible = i.old_configured = true;

// WPA? 
   UpdateScanResults();
// WPA?       UpdateConfigured();
    /* remove items that are still marked as "old" */
    // but not without WPA SweepList();
    // std::cout << "After SweepList!" << std::endl;
  } catch (...) {
    networks.clear();
  }

  GetList().SetLength(networks.size());
  // std::cout << "After GetList.SetLength!" << std::endl;

  UpdateButtons();
  // std::cout << "After UpdateButton!" << std::endl;
}

void
ShowWifiDialog()
{
  const DialogLook &look = UIGlobals::GetDialogLook();
  TWidgetDialog<WifiListWidget>
    dialog(WidgetDialog::Full{}, UIGlobals::GetMainWindow(),
           look, _("Wifi"));
  dialog.AddButton(_("Close"), mrOK);
  dialog.SetWidget();
  dialog.GetWidget().CreateButtons(dialog);
  dialog.ShowModal();
}
