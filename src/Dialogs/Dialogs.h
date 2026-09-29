// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#pragma once

namespace UI { class SingleWindow; }

void dlgBasicSettingsShowModal();

void dlgChecklistShowModal();

/**
 * Called from #SettingsLeave when the checklist file path in Site Files
 * was saved so the next checklist open uses the new profile entry.
 */
void dlgChecklistNotifySiteFileChanged() noexcept;

/**
 * The frequency card: see FrequencyDialog.cpp.
 */
void FrequencyDialogShowModal() noexcept;
void dlgConfigurationShowModal();
void dlgConfigFontsShowModal();

void ShowWindSettingsDialog();

void dlgStatusShowModal(int page);

void dlgCreditsShowModal(UI::SingleWindow &parent);

/**
 * Show the quick menu, or with @p submenu (the name of a mode such as
 * "MapDisplay") that submenu of it.
 */
void
dlgQuickMenuShowModal(UI::SingleWindow &parent,
                      const char *submenu=nullptr) noexcept;
