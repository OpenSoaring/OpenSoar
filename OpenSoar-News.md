# OpenSoar News

OpenSoar is built ON TOP of XCSoar: every OpenSoar version is the
current XCSoar master plus a small, well-defined stack of OpenSoar
additions (see `OpenSoar-AddOn.md` for the complete list of
differences).  This file therefore lists ONLY the OpenSoar additions
and changes per version - everything the XCSoar base brings along is
documented upstream in `NEWS.txt`.

House rule: as soon as a feature listed here is merged into XCSoar
itself, it stops being an OpenSoar item - it is removed from
`OpenSoar-AddOn.md` and future entries here simply ride along with the
XCSoar base.  Entries marked `[upstream PR]` are already submitted to
XCSoar and expected to disappear from this list.

The pre-7.45 history (OpenSoar 7.43/7.44 with its merge-based
bookkeeping) lives in the old repository and is not repeated here -
7.45.25 is the first release built with the rebase workflow.

---

## OpenSoar 7.45.25 - not released yet

Base: XCSoar master, state of 2026-08-27 (past the 7.45 feature set:
OpenGL rendering on all targets, upstream SkySight, reworked
data-file layout, PEV, dark mode, ...).

### Test versions

v7.45.25.t2 (not released yet)

* rebased onto current XCSoar master (2026-08-28) - the base now
  includes the two OpenSoar fixes merged upstream (startup exit code,
  migration-before-profile-load); they are no longer OpenSoar deltas
* the CUPX binary-mode fix was retired: upstream fixed the underlying
  problem centrally in April 2026 (FileDescriptor opens O_BINARY),
  the local workaround is obsolete
* startup: the profile dialog is the start screen - it always
  appears, with a countdown (10 s, configurable as "Startup timeout"
  in System > Site Files > System; any input stops it, zero waits);
  `-profile=X` preselects X instead of skipping the dialog; the
  fly/simulator prompt appears only on request (`-ask`; `-simulator`
  still works)
* leaving the program: one power dialog offering Quit / Restart /
  Reboot / Shutdown, each only where the target can actually do it -
  Restart now also on Android, where the app launches a fresh
  instance of itself; quitting gives immediate feedback instead of a
  frozen screen, and the Windows system-menu close uses the same
  dialog.  Wherever XCSoar asks the user to quit and restart by hand
  (profile switch, changed system settings), OpenSoar offers the
  internal restart directly
* device settings out of the profile:
  - system settings of the device (XCSoar behaviour on/off, devices
    in the profile on/off) live in a file outside the data folder,
    so they survive profile changes and do not travel when the data
    folder is copied to another device; settings page under
    System > System
  - the NMEA devices and their ports live in `device_ports.xcd`,
    which is now a JSON file with only the non-default fields; an old
    key=value `device_ports.xcd` is converted automatically, and a
    per-device switch brings back the XCSoar way (ports in the
    profile) for machines that fly with several instrument sets
* file manager: downloads are checked - a repository entry whose
  name has no extension matching its type is refused with a message
  before the download (the file would never appear in any list), and
  after the download the content is compared with the declared type,
  so a "404" web page, an empty file or a wrong file type produces a
  clear error message instead of silent nothing
* frequency card: the OpenSoar frequency list is back as a second way
  next to the checklist links - one small file per competition
  (System > Site Files > Radio frequencies, `*.xcf`), one tap to the
  standby or active frequency.  The plain text list ("Stuttgart
  Info : 128.950") is read unchanged; alternatively the file can be
  JSON with comments shown as a second line.  Reachable via the new
  Info page 4/4, the RemoteStick menu and the `FrequencyCard` event
* German translations for everything above

v7.45.25.t1 (first test version of the rebased OpenSoar)

* complete rebuild on current XCSoar master; all OpenSoar features
  below are re-applied as a clean patch stack on top
* devices
  - SteFly device family: RemoteStick (auto-detected on its own
    device slot, never occupying a user-configurable port) and
    RotaryPanel, incl. Manage dialog (Send/Receive/Restart)
  - Anemoi (RS-Flight) realtime wind driver
  - Becker AR62xx radio driver
  - FreeVario driver
* branding / user interface
  - OpenSoar name, graphics and icons on all screens; start screen
    shows the full version number prominently
  - test versions (vX.Y.Z.tN) use the red "testing" flavor including
    red application icons; releases are green
  - the "what's new" quick guide only appears when the XCSoar base
    version (major.minor) changes - not for every OpenSoar update
* fixes (also submitted to XCSoar)
  - CUPX waypoint archives: open in binary mode on Windows
    [upstream PR]
  - data-layout migration runs before the profile is loaded - no more
    silent settings loss on the first start after an upgrade
    [upstream PR]
  - quitting during startup exits with code 0 instead of an error
    code [upstream PR]
  - fix undefined behaviour when a terminal/grid widget grows from
    empty (port monitor crash on MSVC debug builds) [upstream PR]
* infrastructure
  - Windows: native CMake/MSVC build (Visual Studio 2022/2026),
    OpenGL-only; third-party libraries build automatically
  - GitHub CI: every release tag builds and publishes the Windows
    package automatically; testing tags become pre-releases
