// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#pragma once

// changed only in config or by user interface
// used in settings dialog
extern bool DevicePortChanged;
extern bool AirspaceFileChanged;
extern bool WaypointFileChanged;
extern bool AirfieldFileChanged;
extern bool InputFileChanged;
extern bool MapFileChanged;
extern bool FlarmFileChanged;
#if 0  // FLARM_FILE2_SETTING,  GLB-FLARM-DeviceDatabase-UNITED is solving this merge
extern bool FlarmFile2Changed;
#endif  // GLB-FLARM-DeviceDatabase-UNITED is solving this merge
extern bool RaspFileChanged;
extern bool LanguageChanged;
extern bool require_restart;

void
SystemConfiguration();
