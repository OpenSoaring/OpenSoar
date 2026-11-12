// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#include "AudioDialog.hpp"
#include "Dialogs/WidgetDialog.hpp"
#include "Widget/WindowWidget.hpp"
#include "Form/Button.hpp"
#include "Look/DialogLook.hpp"
#include "Audio/VarioSettings.hpp"
#include "VarioSound.hpp"
#include "Interface.hpp"
#include "Profile/Keys.hpp"
#include "Profile/Profile.hpp"
#include "Screen/Layout.hpp"
#include "UIGlobals.hpp"
#include "Language/Language.hpp"
#include "ui/canvas/Canvas.hpp"
#include "ui/canvas/Pen.hpp"
#include "ui/event/KeyCode.hpp"
#include "ui/window/ContainerWindow.hpp"
#include "ui/window/PaintWindow.hpp"
#include "util/StaticString.hxx"

#include <algorithm>
#include <cmath>
#include <functional>

/** volumes in percent go in steps of 10, 3 dB each for the vario */
static constexpr unsigned PERCENT_STEP = 10;

/* the colours of a bar: the volume, and the range of the vario within
   the master volume */
static constexpr Color HEARD_COLOR{0x2e, 0x7d, 0x32};
static constexpr Color CUT_COLOR{0xa5, 0xd6, 0xa7};

/**
 * A volume as a bar that fills from the left.  A tap or a drag sets the
 * volume, the cursor keys left and right and the mouse wheel change it
 * by one step.  The bar
 * is grey and does not react while it is muted.  The percentage is
 * written into the bar, light on the filled part and dark on the empty
 * one, so that it can be read on both.
 *
 * The vario plays through the master volume, so its bar is a share of
 * the master bar: a light band as long as the master bar is its whole
 * range, and the dark part in it the vario volume, see SetScale().
 */
class VolumeBar final : public PaintWindow {
  const DialogLook &look;
  const std::function<void(unsigned)> on_change;

  unsigned value = 0, max_value = 100;

  /** how much a tap or a key changes #value */
  unsigned increment = PERCENT_STEP;

  /** show "5/15" instead of percent: the steps of the device */
  bool in_steps = false;

  /** the share of the track in percent that #max_value covers */
  unsigned scale = 100;

  /** draw the range up to #max_value as a light band? */
  bool band = false;

  bool greyed = false;

  /** does the bar ignore input?  Only while its own sound is muted */
  bool locked = false;

  bool dragging = false;

public:
  VolumeBar(const DialogLook &_look,
            std::function<void(unsigned)> _on_change) noexcept
    :look(_look), on_change(std::move(_on_change)) {}

  /**
   * @param _increment the change by a tap step or a key
   * @param _in_steps show the value as steps of @a _max_value instead
   * of percent
   */
  void SetMax(unsigned _max_value, unsigned _increment,
              bool _in_steps) noexcept {
    increment = std::max(_increment, 1u);
    max_value = std::max(_max_value / increment * increment, increment);
    in_steps = _in_steps;
    Invalidate();
  }

  void SetValue(unsigned _value) noexcept {
    value = std::min(_value, max_value);
    Invalidate();
  }

  /**
   * Draw the whole range as a light band over @a _scale percent of the
   * width, and the volume as the same share of it: the vario at 50
   * percent under a master volume of 60 percent fills 30 percent of the
   * width, half of the band, which is what is heard.
   */
  void SetScale(unsigned _scale) noexcept {
    scale = std::clamp(_scale, 1u, 100u);
    band = true;
    Invalidate();
  }

  /** the whole width for the whole range, without a band */
  void SetFullScale() noexcept {
    scale = 100;
    band = false;
    Invalidate();
  }

  /**
   * @param _greyed draw the bar grey: nothing of it is heard
   * @param _locked ignore input as well, because the bar's own sound
   * is muted
   */
  void SetGreyed(bool _greyed, bool _locked) noexcept {
    greyed = _greyed;
    locked = _locked;
    Invalidate();
  }

private:
  void Change(int new_value) noexcept {
    if (locked)
      return;

    const unsigned v = std::clamp(new_value, 0, (int)max_value);
    if (v == value)
      return;

    value = v;
    Invalidate();
    on_change(value);
  }

  [[gnu::pure]]
  PixelRect GetTrack() const noexcept {
    return GetClientRect().WithPadding(Layout::GetTextPadding());
  }

  /** the width that #max_value covers */
  [[gnu::pure]]
  int GetRangeWidth(const PixelRect &track) const noexcept {
    return std::max(1, (int)(track.GetWidth() * scale / 100));
  }

  /** the horizontal position of the volume @a v */
  [[gnu::pure]]
  int ToX(const PixelRect &track, unsigned v) const noexcept {
    return track.left + GetRangeWidth(track) * (int)v / (int)max_value;
  }

  /** the step nearest to the horizontal position @a x */
  [[gnu::pure]]
  unsigned ValueAt(int x) const noexcept {
    const PixelRect track = GetTrack();
    const int width = GetRangeWidth(track);
    const int steps = max_value / increment;
    const int step = ((x - track.left) * steps + width / 2) / width;
    return std::clamp(step, 0, steps) * increment;
  }

protected:
  /* virtual methods from class Window */
  bool OnKeyCheck(unsigned key_code) const noexcept override {
    return key_code == KEY_LEFT || key_code == KEY_RIGHT;
  }

  bool OnKeyDown(unsigned key_code) noexcept override {
    switch (key_code) {
    case KEY_LEFT:
      Change((int)value - (int)increment);
      return true;

    case KEY_RIGHT:
      Change(value + increment);
      return true;
    }

    return PaintWindow::OnKeyDown(key_code);
  }

  bool OnMouseDown(PixelPoint p) noexcept override {
    SetFocus();
    if (locked)
      return true;

    dragging = true;
    SetCapture();
    Change(ValueAt(p.x));
    return true;
  }

  bool OnMouseMove(PixelPoint p,
                   [[maybe_unused]] unsigned keys) noexcept override {
    if (dragging)
      Change(ValueAt(p.x));
    return true;
  }

  /* a mouse wheel or a rotary encoder changes the volume like the
     cursor keys, one step per notch, away from the user is louder */
  bool OnMouseWheel([[maybe_unused]] PixelPoint p,
                    int delta) noexcept override {
    Change((int)value + delta * (int)increment);
    return true;
  }

  bool OnMouseUp([[maybe_unused]] PixelPoint p) noexcept override {
    if (dragging) {
      dragging = false;
      ReleaseCapture();
    }
    return true;
  }

  void OnCancelMode() noexcept override {
    dragging = false;
    PaintWindow::OnCancelMode();
  }

  void OnSetFocus() noexcept override {
    PaintWindow::OnSetFocus();
    Invalidate();
  }

  void OnKillFocus() noexcept override {
    PaintWindow::OnKillFocus();
    Invalidate();
  }

  void OnPaint(Canvas &canvas) noexcept override;
};

void
VolumeBar::OnPaint(Canvas &canvas) noexcept
{
  const PixelRect rc = GetClientRect();
  canvas.DrawFilledRectangle(rc, look.background_color);

  if (HasFocus()) {
    /* a frame, so the colours of the bar keep their meaning */
    const Pen pen(Layout::ScalePenWidth(2), look.focused.background_color);
    canvas.Select(pen);
    canvas.SelectHollowBrush();
    canvas.DrawRectangle(rc.WithPadding(Layout::ScalePenWidth(1)));
  }

  const PixelRect track = GetTrack();
  canvas.DrawFilledRectangle(track, COLOR_WHITE);

  const int heard_x = ToX(track, value);

  PixelRect part = track;
  if (band) {
    part.right = ToX(track, max_value);
    canvas.DrawFilledRectangle(part, greyed ? COLOR_LIGHT_GRAY : CUT_COLOR);
  }

  part.right = heard_x;
  canvas.DrawFilledRectangle(part, greyed ? COLOR_GRAY : HEARD_COLOR);

  canvas.DrawOutlineRectangle(track, COLOR_GRAY);

  /* the percentage in the middle of the bar: dark everywhere, then
     light again over the dark (heard) part of the bar */
  StaticString<16> text;
  if (in_steps)
    text.Format("%u/%u", value, max_value);
  else
    text.Format("%u %%", value);
  canvas.Select(look.bold_font);
  canvas.SetBackgroundTransparent();
  const PixelSize size = canvas.CalcTextSize(text.c_str());
  const PixelPoint p = track.CenteredTopLeft(size);

  canvas.SetTextColor(COLOR_BLACK);
  canvas.DrawText(p, text.c_str());

  if (heard_x > p.x) {
    canvas.SetTextColor(COLOR_WHITE);
    canvas.DrawClippedText(p, (unsigned)(heard_x - p.x), text.c_str());
  }
}

/**
 * A small button that stays pressed while its sound is muted, with a
 * loudspeaker that is crossed out then.
 */
class MuteButton final : public PaintWindow {
  const DialogLook &look;
  const std::function<void(bool)> on_change;
  bool muted = false;

public:
  MuteButton(const DialogLook &_look,
             std::function<void(bool)> _on_change) noexcept
    :look(_look), on_change(std::move(_on_change)) {}

  void SetMuted(bool _muted) noexcept {
    muted = _muted;
    Invalidate();
  }

private:
  void Toggle() noexcept {
    muted = !muted;
    Invalidate();
    on_change(muted);
  }

protected:
  bool OnKeyCheck(unsigned key_code) const noexcept override {
    return key_code == KEY_RETURN || key_code == ' ';
  }

  bool OnKeyDown(unsigned key_code) noexcept override {
    if (key_code == KEY_RETURN || key_code == ' ') {
      Toggle();
      return true;
    }

    return PaintWindow::OnKeyDown(key_code);
  }

  bool OnMouseDown([[maybe_unused]] PixelPoint p) noexcept override {
    SetFocus();
    Toggle();
    return true;
  }

  void OnSetFocus() noexcept override {
    PaintWindow::OnSetFocus();
    Invalidate();
  }

  void OnKillFocus() noexcept override {
    PaintWindow::OnKillFocus();
    Invalidate();
  }

  void OnPaint(Canvas &canvas) noexcept override;
};

void
MuteButton::OnPaint(Canvas &canvas) noexcept
{
  const PixelRect rc = GetClientRect();
  canvas.DrawFilledRectangle(rc, look.background_color);

  /* pressed in while muted, like a latching switch */
  const PixelRect face = rc.WithPadding(Layout::GetTextPadding());
  canvas.DrawFilledRectangle(face, muted ? COLOR_DARK_GRAY : COLOR_LIGHT_GRAY);
  if (HasFocus()) {
    const Pen pen(Layout::ScalePenWidth(2), look.focused.background_color);
    canvas.Select(pen);
    canvas.SelectHollowBrush();
    canvas.DrawRectangle(rc.WithPadding(Layout::ScalePenWidth(1)));
  }

  const Color ink = muted ? COLOR_WHITE : COLOR_BLACK;
  const PixelPoint c = face.GetCenter();
  const int s = std::max(2, std::min((int)face.GetWidth() / 5,
                                     (int)face.GetHeight() / 4));

  /* the loudspeaker: a box and a cone, left of the centre */
  const BulkPixelPoint speaker[] = {
    {c.x - 2 * s, c.y - s / 2},
    {c.x - s, c.y - s / 2},
    {c.x, c.y - s * 3 / 2},
    {c.x, c.y + s * 3 / 2},
    {c.x - s, c.y + s / 2},
    {c.x - 2 * s, c.y + s / 2},
  };

  canvas.SelectNullPen();
  canvas.Select(Brush(ink));
  canvas.DrawPolygon(speaker, std::size(speaker));

  const Pen pen(std::max(2u, Layout::Scale(2u)), muted ? COLOR_RED : ink);
  canvas.Select(pen);

  if (muted) {
    /* crossed out */
    canvas.DrawLine({c.x + s / 2, c.y - s}, {c.x + 2 * s, c.y + s});
    canvas.DrawLine({c.x + s / 2, c.y + s}, {c.x + 2 * s, c.y - s});
  } else {
    /* two sound waves, arcs around the cone */
    for (int r = s; r <= 2 * s; r += s * 3 / 4) {
      BulkPixelPoint wave[9];
      for (int i = 0; i < 9; ++i) {
        const double a = (i - 4) * 0.2;
        wave[i] = {c.x - s / 2 + (int)(r * std::cos(a)),
                   c.y + (int)(r * std::sin(a))};
      }
      canvas.DrawPolyline(wave, std::size(wave));
    }
  }
}

/**
 * The content of the dialog: a row with label, bar and mute button for
 * the master volume and one for the vario, and, on the OpenVario with
 * variod running, the switch of the source.
 */
/** is variod the chosen source of the vario sound? */
[[gnu::pure]]
static bool
IsVariodChosen() noexcept
{
#ifdef IS_OPENVARIO
  return CommonInterface::GetUISettings().sound.vario.source ==
    VarioSoundSource::OPENVARIO;
#else
  return false;
#endif
}

class AudioWindow final : public ContainerWindow {
  const DialogLook &look;

  const bool have_master = HasMasterVolume();
  const bool variod_found = IsVariodAvailable();

  /**
   * The switch of the source: while variod is there, and also while it
   * is chosen but missing, or the vario would stay silent without a
   * way back to the program's own sound.
   */
  const bool have_source = variod_found || IsVariodChosen();

  VolumeBar master_bar{look, [this](unsigned v){ OnMaster(v); }};
  MuteButton master_mute{look, [this](bool m){ OnMasterMute(m); }};
  VolumeBar vario_bar{look, [this](unsigned v){ OnVario(v); }};
  MuteButton vario_mute{look, [this](bool m){ OnVarioMute(m); }};
  Button source_button;

  PixelPoint master_label, vario_label;

public:
  /** has anything been stored in the profile? */
  bool changed = false;

  explicit AudioWindow(const DialogLook &_look) noexcept
    :look(_look) {}

  void FocusFirst() noexcept {
    vario_bar.SetFocus();
  }

  [[gnu::pure]]
  static PixelSize GetSize() noexcept {
    const unsigned row = Layout::GetMaximumControlHeight();
    const unsigned padding = Layout::GetTextPadding() * 2;
    const unsigned rows = 1 + HasMasterVolume() +
      (IsVariodAvailable() || IsVariodChosen());
    return {Layout::Scale(280u), rows * row + (rows + 1) * padding};
  }

private:
  VarioSoundSettings &Settings() noexcept {
    return CommonInterface::SetUISettings().sound.vario;
  }

  /**
   * Is the vario silent: muted, or the program's own sound is the
   * source and switched off in the settings?
   */
  [[gnu::pure]]
  bool IsVarioSilent() noexcept {
    const auto &s = Settings();
    return IsVarioMuted() ||
      (s.source == VarioSoundSource::INTERNAL && !s.enabled);
  }

  void Update() noexcept;

  void OnMaster(unsigned v) noexcept {
    SetMasterVolume(v);
    changed = true;
    Update();
  }

  void OnMasterMute(bool muted) noexcept {
    SetMasterMuted(muted);
    Update();
  }

  void OnVario(unsigned v) noexcept {
    auto &s = Settings();
    s.volume = v;
    Profile::Set(ProfileKeys::SoundVolume, v);
    changed = true;
    ApplyVarioSound(s);
    Update();
  }

  void OnVarioMute(bool muted) noexcept {
    auto &s = Settings();
    SetVarioMuted(muted);

    if (!muted && s.source == VarioSoundSource::INTERNAL && !s.enabled) {
      /* the program's vario sound was switched off in the settings;
         "unmute" means the pilot wants to hear it */
      s.enabled = true;
      Profile::Set(ProfileKeys::SoundAudioVario, true);
      changed = true;
    }

    ApplyVarioSound(s);
    Update();
  }

  void OnSource() noexcept {
    auto &s = Settings();
    s.source = s.source == VarioSoundSource::INTERNAL
      ? VarioSoundSource::OPENVARIO
      : VarioSoundSource::INTERNAL;
    Profile::Set(ProfileKeys::VarioSoundSource, (unsigned)s.source);
    changed = true;
    ApplyVarioSound(s);
    Update();
  }

  void LayoutControls() noexcept;

protected:
  void OnCreate() override;
  void OnResize(PixelSize new_size) noexcept override;
  void OnPaint(Canvas &canvas) noexcept override;
};

void
AudioWindow::Update() noexcept
{
  const auto &s = Settings();

  /* the master volume is Android's media volume, or on Linux and
     Windows the gain of the program's own mixer, which leaves the
     volume of the system alone; variod plays past that mixer, so it
     does not turn variod down */
  const bool through_master =
    have_master && s.source == VarioSoundSource::INTERNAL;

  bool master_silent = false;
  if (have_master) {
    const unsigned master = GetMasterVolume();
    const bool muted = IsMasterMuted();
    master_silent = through_master && (muted || master == 0);
    master_bar.SetValue(master);
    master_bar.SetGreyed(muted, muted);
    master_mute.SetMuted(muted);

    /* the vario bar is drawn within the length of the master bar, an
       approximation of what is heard; a master volume of 0 would leave
       nothing to tap on, so the vario keeps the full width then */
    if (through_master)
      vario_bar.SetScale(master > 0
                         ? master * 100 / GetMasterVolumeMax()
                         : 100);
    else
      vario_bar.SetFullScale();
  }

  vario_bar.SetValue(s.volume);

  /* a silent master silences the vario as well, although its own
     button is not pressed; it stays adjustable then */
  const bool silent = IsVarioSilent();
  /* a chosen variod that is not there plays nothing either */
  const bool variod_missing =
    s.source != VarioSoundSource::INTERNAL && !variod_found;
  vario_bar.SetGreyed(silent || master_silent || variod_missing, silent);
  vario_mute.SetMuted(silent);

  if (have_source)
    source_button.SetCaption(s.source == VarioSoundSource::INTERNAL
                             ? _("Vario: Program")
                             : (variod_found
                                ? _("Vario: OpenVario")
                                : _("Vario: OpenVario (not found)")));
}

void
AudioWindow::OnCreate()
{
  ContainerWindow::OnCreate();

  WindowStyle style;
  style.TabStop();

  const PixelRect dummy{0, 0, 1, 1};

  if (have_master) {
    master_bar.Create(*this, dummy, style);
    master_bar.SetMax(GetMasterVolumeMax(), GetMasterVolumeIncrement(),
                      IsMasterVolumeInSteps());
    master_mute.Create(*this, dummy, style);
  }

  vario_bar.Create(*this, dummy, style);
  vario_mute.Create(*this, dummy, style);

  if (have_source)
    source_button.Create(*this, look.button, _("Vario: Program"), dummy,
                         style, [this]{ OnSource(); });

  Update();
  LayoutControls();
}

void
AudioWindow::LayoutControls() noexcept
{
  const PixelRect rc = GetClientRect();
  const int row = Layout::GetMaximumControlHeight();
  const int padding = Layout::GetTextPadding() * 2;

  const char *const master_text = _("Master");
  const char *const vario_text = _("Vario");
  const int label_width =
    std::max(look.text_font.TextSize(master_text).width,
             look.text_font.TextSize(vario_text).width) + 2 * padding;
  const int label_y = (row - (int)look.text_font.GetHeight()) / 2;

  /* the mute button right of its bar, a little smaller than a row */
  const int mute_width = row * 5 / 4;
  const int bar_right = rc.right - padding - mute_width;

  int y = rc.top + padding;

  if (have_master) {
    master_label = {rc.left + padding, y + label_y};
    master_bar.Move({rc.left + label_width, y, bar_right, y + row});
    master_mute.Move({bar_right, y, rc.right - padding, y + row});
    y += row + padding;
  }

  vario_label = {rc.left + padding, y + label_y};
  vario_bar.Move({rc.left + label_width, y, bar_right, y + row});
  vario_mute.Move({bar_right, y, rc.right - padding, y + row});
  y += row + padding;

  if (have_source)
    source_button.Move({rc.left + label_width, y, rc.right - padding,
                        y + row});
}

void
AudioWindow::OnResize(PixelSize new_size) noexcept
{
  ContainerWindow::OnResize(new_size);
  if (vario_bar.IsDefined())
    LayoutControls();
}

void
AudioWindow::OnPaint(Canvas &canvas) noexcept
{
  canvas.DrawFilledRectangle(GetClientRect(), look.background_color);

  canvas.Select(look.text_font);
  canvas.SetTextColor(look.text_color);
  canvas.SetBackgroundTransparent();
  if (have_master)
    canvas.DrawText(master_label, _("Master"));
  canvas.DrawText(vario_label, _("Vario"));

  ContainerWindow::OnPaint(canvas);
}

class AudioWidget final : public WindowWidget {
  const DialogLook &look;

public:
  explicit AudioWidget(const DialogLook &_look) noexcept
    :look(_look) {}

  bool HasChanged() const noexcept {
    return ((const AudioWindow &)GetWindow()).changed;
  }

  /* virtual methods from class Widget */
  PixelSize GetMinimumSize() const noexcept override {
    return AudioWindow::GetSize();
  }

  PixelSize GetMaximumSize() const noexcept override {
    PixelSize size = GetMinimumSize();
    size.width = Layout::Scale(400u);
    return size;
  }

  void Prepare(ContainerWindow &parent,
               const PixelRect &rc) noexcept override {
    WindowStyle style;
    style.Hide();
    style.ControlParent();

    auto w = std::make_unique<AudioWindow>(look);
    w->Create(parent, rc, style);
    SetWindow(std::move(w));
  }

  bool SetFocus() noexcept override {
    ((AudioWindow &)GetWindow()).FocusFirst();
    return true;
  }
};

bool
ShowAudioDialog() noexcept
{
  const DialogLook &look = UIGlobals::GetDialogLook();
  TWidgetDialog<AudioWidget>
    dialog(WidgetDialog::Auto{}, UIGlobals::GetMainWindow(), look,
           _("Audio"));
  dialog.AddButton(_("Close"), mrOK);
  dialog.SetWidget(look);
  dialog.ShowModal();

  /* the changes were applied and set in the profile at once; write the
     file once */
  const bool changed = dialog.GetWidget().HasChanged();
  if (changed)
    Profile::Save();
  return changed;
}
