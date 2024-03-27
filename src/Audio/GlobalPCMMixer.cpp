// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#include "GlobalPCMMixer.hpp"
#include "PCMMixer.hpp"
#ifndef _WIN32
#include "PCMPlayerFactory.hpp"
#endif
#include "GlobalPCMResourcePlayer.hpp"
#include "GlobalVolumeController.hpp"

#include "event/Loop.hxx"

#include <cassert>
#include <memory>

PCMMixer *pcm_mixer = nullptr;
ScopeGlobalPCMMixer *global_pcm_mixer = nullptr;
ScopeGlobalPCMResourcePlayer *global_pcm_resouce_player = nullptr;
ScopeGlobalVolumeController *global_volume_controller = nullptr;

#ifndef _WIN32
void
InitialisePCMMixer(EventLoop &event_loop)
{
  assert(nullptr == pcm_mixer);

  pcm_mixer =
      new PCMMixer(44100,
                   std::unique_ptr<PCMPlayer>(PCMPlayerFactory::CreateInstanceForDirectAccess(event_loop)));
}

void
DeinitialisePCMMixer()
{
  assert(nullptr != pcm_mixer);

  delete pcm_mixer;
  pcm_mixer = nullptr;
}

#endif

void InitAudio(EventLoop *_loop) {
  // How I get this variables to live up to the end?
  EventLoop *loop = _loop;
  global_pcm_mixer = new ScopeGlobalPCMMixer(*loop);
  global_pcm_resouce_player = new ScopeGlobalPCMResourcePlayer;
  global_volume_controller = new ScopeGlobalVolumeController;
}

void ShutdownAudio() {
  // How I get this variables to live up to the end?
  delete global_pcm_mixer;
  delete global_pcm_resouce_player;
  delete global_volume_controller;
}
