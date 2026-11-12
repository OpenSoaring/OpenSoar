// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#include "AudioConfigPanel.hpp"
#include "Audio/Features.hpp"

#ifdef HAVE_PCM_PLAYER

#include <algorithm>

#include "Audio/VarioGlue.hpp"
#include "Audio/VarioSettings.hpp"
#include "Form/DataField/Boolean.hpp"
#include "Form/DataField/Enum.hpp"
#include "Form/DataField/Float.hpp"
#include "Form/DataField/Listener.hpp"
#include "Formatter/UserUnits.hpp"
#include "Interface.hpp"
#include "Language/Language.hpp"
#include "Profile/Keys.hpp"
#include "UIGlobals.hpp"
#include "Units/Units.hpp"
#include "Widget/RowFormWidget.hpp"

#ifdef HAVE_VOLUME_CONTROLLER
#include "Audio/VolumeController.hpp"
#endif

#ifdef ANDROID
#include "Android/Main.hpp"
#include "Android/SoundUtil.hpp"
#include "Android/Context.hpp"
#endif

#if defined(HAVE_VOLUME_CONTROLLER) || defined(ANDROID)
/* a master volume: the program's own mixer, or on Android the volume
   of the device itself */
#define HAVE_MASTER_VOLUME_ROW
#endif

/**
 * The volume rows go in steps of 10 percent; the vario's curve gives
 * each step 3 dB (see ToneSynthesiser::VolumeToGain()), finer steps
 * could hardly be told apart.
 */
static constexpr unsigned VOLUME_STEP = 10;

static constexpr unsigned
RoundVolume(unsigned percent) noexcept
{
  return VarioSoundSettings::RoundVolume(percent);
}

enum ControlIndex {
#ifdef HAVE_MASTER_VOLUME_ROW
  MASTER_VOLUME,
  MASTER_VOLUME_SPACER,
#endif
#ifdef IS_OPENVARIO
  SOURCE,
#endif
  ENABLED,
  VOLUME,
  SWITCHING_MODE,
  DEAD_BAND_ENABLED,
  SPACER,
  MIN_FREQUENCY,
  ZERO_FREQUENCY,
  MAX_FREQUENCY,
  SPACER2,
  DEAD_BAND_MIN,
  DEAD_BAND_MAX,
};

#ifdef IS_OPENVARIO
static constexpr StaticEnumChoice sources[] = {
  { VarioSoundSource::INTERNAL, N_("Program"),
    N_("The program plays the vario sound; variod is set to 0.") },
  { VarioSoundSource::OPENVARIO, N_("OpenVario (variod)"),
    N_("The vario daemon of the OpenVario plays the vario sound with the "
       "volume set here; the program's own vario sound is off.") },
  nullptr
};
#endif

static constexpr StaticEnumChoice switching_modes[] = {
  { VarioSoundSwitchingMode::MANUAL, NC_("Setting", "Manual") },
  { VarioSoundSwitchingMode::AUTO, NC_("Setting", "Auto") },
  nullptr
};

class AudioConfigPanel final
  : public RowFormWidget, DataFieldListener {
public:
  AudioConfigPanel()
    :RowFormWidget(UIGlobals::GetDialogLook()) {}

  /**
   * Show the rows that apply to the selected source: variod has only
   * the volume, the program's own sound all of them.
   */
  void UpdateVisibility() noexcept;

  void Prepare(ContainerWindow &parent, const PixelRect &rc) noexcept override;
  bool Save(bool &changed) noexcept override;

private:
#ifdef ANDROID
  /** the Android volume when the page opened, or -1 */
  int initial_android_volume = -1;
#endif

  void OnModified(DataField &df) noexcept override;
};

void
AudioConfigPanel::UpdateVisibility() noexcept
{
#ifdef IS_OPENVARIO
  const bool internal =
    GetValueEnum(SOURCE) == (unsigned)VarioSoundSource::INTERNAL;
#else
  constexpr bool internal = true;
#endif

  const bool enabled = GetValueBoolean(ENABLED);
  const bool own_sound = internal && enabled;
  const bool dead_band = GetValueBoolean(DEAD_BAND_ENABLED);

  SetRowAvailable(ENABLED, internal);
  SetRowAvailable(VOLUME, !internal || enabled);
  SetRowAvailable(SWITCHING_MODE, own_sound);
  SetRowAvailable(DEAD_BAND_ENABLED, own_sound);
  SetRowAvailable(SPACER, own_sound);
  SetRowAvailable(MIN_FREQUENCY, own_sound);
  SetRowAvailable(ZERO_FREQUENCY, own_sound);
  SetRowAvailable(MAX_FREQUENCY, own_sound);
  SetRowAvailable(SPACER2, own_sound && dead_band);
  SetRowAvailable(DEAD_BAND_MIN, own_sound && dead_band);
  SetRowAvailable(DEAD_BAND_MAX, own_sound && dead_band);
}

void
AudioConfigPanel::OnModified(DataField &df) noexcept
{
  if (IsDataField(ENABLED, df) || IsDataField(DEAD_BAND_ENABLED, df)
#ifdef IS_OPENVARIO
      || IsDataField(SOURCE, df)
#endif
      )
    UpdateVisibility();
}

void
AudioConfigPanel::Prepare(ContainerWindow &parent,
                          const PixelRect &rc) noexcept
{
  RowFormWidget::Prepare(parent, rc);

#ifdef HAVE_VOLUME_CONTROLLER
  const auto &sound = CommonInterface::GetUISettings().sound;

  AddInteger(_("Master Volume"),
             _("The overall audio output volume."),
             "%d %%", "%d",
             0, VolumeController::GetMaxValue(), 1, sound.master_volume);
#elif defined(ANDROID)
  /* the volume of the device, read each time the page opens, so that
     it also shows what the volume buttons have set; not stored in the
     profile, because Android keeps it */
  initial_android_volume =
    SoundUtil::GetMasterVolume(Java::GetEnv(), context->Get());
  AddInteger(_("Master Volume"),
             _("The volume of the device for media and notifications, "
               "which the audio vario and the spoken messages use.  The "
               "same as the volume buttons and the sound settings of "
               "Android."),
             "%d %%", "%d", 0, 100, VOLUME_STEP,
             RoundVolume(std::max(initial_android_volume, 0)));
#endif

  if (!AudioVarioGlue::HaveAudioVario())
    return;

#ifdef HAVE_MASTER_VOLUME_ROW
  AddSpacer();
#endif

  const auto &settings = CommonInterface::GetUISettings().sound.vario;

#ifdef IS_OPENVARIO
  /* both play on the same audio output, and two vario tones at once
     cannot be told apart */
  AddEnum(_("Vario sound"),
          _("Which program plays the vario sound.  Only one of them gets "
            "the volume, the other is set to 0."),
          sources, (unsigned)settings.source, this);
#endif

  AddBoolean(_("Audio Vario"),
             _("Emulate the sound of an electronic vario."),
             settings.enabled, this);

  AddInteger(_("Vario Volume"),
             _("The audio vario sound volume.  Each step of 10 percent is "
               "3 dB: 50 percent sounds clearly softer than 100 percent, "
               "10 percent is quiet but still audible."),
             "%u %%", "%u", 0, 100, VOLUME_STEP, settings.volume);

  AddEnum(C_("Setting", "Mode switching"),
      _("Choose whether the audio vario stays in manual mode or switches automatically between Vario in circling and STF in cruise. Manual mode starts in Vario after each restart and can be changed by external input events. In the built-in simulator, STF audio needs valid airspeed and total-energy vario input; without those, manual STF is silent and auto cruise falls back to vario."),
          switching_modes, (unsigned)settings.switching_mode);

  AddBoolean(_("Enable Deadband"),
             _("Mute the audio output in when the current lift is in a "
               "certain range around zero"), settings.dead_band_enabled,
             this);

  AddSpacer();
  SetExpertRow(SPACER);

  AddInteger(_("Min. Frequency"),
             _("The tone frequency that is played at maximum sink rate."),
             "%u Hz", "%u",
             50, 3000, 50, settings.min_frequency);
  SetExpertRow(MIN_FREQUENCY);

  AddInteger(_("Zero Frequency"),
             _("The tone frequency that is played at zero climb rate."),
             "%u Hz", "%u",
             50, 3000, 50, settings.zero_frequency);
  SetExpertRow(ZERO_FREQUENCY);

  AddInteger(_("Max. Frequency"),
             _("The tone frequency that is played at maximum climb rate."),
             "%u Hz", "%u",
             50, 3000, 50, settings.max_frequency);
  SetExpertRow(MAX_FREQUENCY);

  AddSpacer();
  SetExpertRow(SPACER2);

  AddFloat(_("Deadband min. lift"),
           _("Below this lift threshold the vario will start to play sounds if the 'Deadband' feature is enabled."),
           "%.1f %s", "%.1f",
           Units::ToUserVSpeed(-5), 0,
           GetUserVerticalSpeedStep(), false, UnitGroup::VERTICAL_SPEED,
           settings.min_dead);
  SetExpertRow(DEAD_BAND_MIN);
  DataFieldFloat &db_min = (DataFieldFloat &)GetDataField(DEAD_BAND_MIN);
  db_min.SetFormat(GetUserVerticalSpeedFormat(false, true));

  AddFloat(_("Deadband max. lift"),
           _("Above this lift threshold the vario will start to play sounds if the 'Deadband' feature is enabled."),
           "%.1f %s", "%.1f",
           0, Units::ToUserVSpeed(2),
           GetUserVerticalSpeedStep(), false, UnitGroup::VERTICAL_SPEED,
           settings.max_dead);
  SetExpertRow(DEAD_BAND_MAX);
  DataFieldFloat &db_max = (DataFieldFloat &)GetDataField(DEAD_BAND_MAX);
  db_max.SetFormat(GetUserVerticalSpeedFormat(false, true));

  UpdateVisibility();
}

bool
AudioConfigPanel::Save(bool &changed) noexcept
{
#ifdef HAVE_VOLUME_CONTROLLER
  auto &sound = CommonInterface::SetUISettings().sound;

  changed |= SaveValueInteger(MASTER_VOLUME, ProfileKeys::MasterAudioVolume,
                              sound.master_volume);
#elif defined(ANDROID)
  if (const unsigned volume = GetValueInteger(MASTER_VOLUME);
      volume != RoundVolume(std::max(initial_android_volume, 0)))
    SoundUtil::SetMasterVolume(Java::GetEnv(), context->Get(), volume);
#endif

  if (!AudioVarioGlue::HaveAudioVario())
    return true;

  auto &settings = CommonInterface::SetUISettings().sound.vario;

#ifdef IS_OPENVARIO
  changed |= SaveValueEnum(SOURCE, ProfileKeys::VarioSoundSource,
                           settings.source);
#endif

  changed |= SaveValue(ENABLED, ProfileKeys::SoundAudioVario,
                       settings.enabled);

  changed |= SaveValueInteger(VOLUME, ProfileKeys::SoundVolume,
                              settings.volume);

  changed |= SaveValueEnum(SWITCHING_MODE, ProfileKeys::VarioSoundSwitchingMode,
                           settings.switching_mode);

  changed |= SaveValue(DEAD_BAND_ENABLED, ProfileKeys::VarioDeadBandEnabled,
                       settings.dead_band_enabled);

  changed |= SaveValueInteger(MIN_FREQUENCY, ProfileKeys::VarioMinFrequency,
                              settings.min_frequency);

  changed |= SaveValueInteger(ZERO_FREQUENCY, ProfileKeys::VarioZeroFrequency,
                              settings.zero_frequency);

  changed |= SaveValueInteger(MAX_FREQUENCY, ProfileKeys::VarioMaxFrequency,
                              settings.max_frequency);

  changed |= SaveValue(DEAD_BAND_MIN, UnitGroup::VERTICAL_SPEED,
                       ProfileKeys::VarioDeadBandMin, settings.min_dead);

  changed |= SaveValue(DEAD_BAND_MAX, UnitGroup::VERTICAL_SPEED,
                       ProfileKeys::VarioDeadBandMax, settings.max_dead);

  return true;
}

std::unique_ptr<Widget>
CreateAudioConfigPanel()
{
  return std::make_unique<AudioConfigPanel>();
}

#endif /* HAVE_PCM_PLAYER */
