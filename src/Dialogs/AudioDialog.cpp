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

/** volumes go in steps of 10 percent, 3 dB each for the vario */
static constexpr unsigned STEP = 10;

/**
 * A volume as a row of segments, one per 10 percent.  A tap or a drag
 * sets the volume, the cursor keys left and right change it by one
 * step.  The part of the volume that is not reached because of a
 * lower master volume is drawn lighter, and the whole bar is grey and
 * does not react while the sound is muted.
 */
class VolumeBar final : public PaintWindow {
  const DialogLook &look;
  const std::function<void(unsigned)> on_change;

  unsigned value = 0, max_value = 100;

  /**
   * The volume that fills the whole width.  Both bars of the dialog
   * get the same scale, so that 100 percent of the vario is drawn
   * under 100 percent of the master volume.
   */
  unsigned scale_max = 100;

  /** the part of #value that is heard, see SetLimit() */
  unsigned limit = 100;

  bool greyed = false;
  bool dragging = false;

public:
  VolumeBar(const DialogLook &_look,
            std::function<void(unsigned)> _on_change) noexcept
    :look(_look), on_change(std::move(_on_change)) {}

  void SetMax(unsigned _max_value, unsigned _scale_max) noexcept {
    max_value = std::max(_max_value / STEP * STEP, STEP);
    scale_max = std::max(_scale_max / STEP * STEP, max_value);
    Invalidate();
  }

  void SetValue(unsigned _value) noexcept {
    value = std::min(_value, max_value);
    Invalidate();
  }

  /**
   * How much of the volume is heard: a master volume below 100 percent
   * turns the vario down as well.
   */
  void SetLimit(unsigned _limit) noexcept {
    limit = _limit;
    Invalidate();
  }

  void SetGreyed(bool _greyed) noexcept {
    greyed = _greyed;
    Invalidate();
  }

private:
  void Change(int new_value) noexcept {
    if (greyed)
      return;

    const unsigned v = std::clamp(new_value, 0, (int)max_value);
    if (v == value)
      return;

    value = v;
    Invalidate();
    on_change(value);
  }

  /** the area of the segments; the percent text is right of it */
  [[gnu::pure]]
  PixelRect GetSegmentArea() const noexcept {
    PixelRect rc = GetClientRect().WithPadding(Layout::GetTextPadding());
    rc.right -= look.bold_font.TextSize("150 %").width +
      2 * Layout::GetTextPadding();
    return rc;
  }

  /** the distance from one segment to the next */
  [[gnu::pure]]
  int GetSegmentPitch(const PixelRect &area) const noexcept {
    return std::max(1, (int)area.GetWidth() / (int)(scale_max / STEP));
  }

  /** the step under the horizontal position @a x */
  [[gnu::pure]]
  unsigned ValueAt(int x) const noexcept {
    const PixelRect area = GetSegmentArea();
    const int pitch = GetSegmentPitch(area);

    /* a tap on a segment selects it, a tap left of the first one
       silences */
    const int segment = (x - area.left + pitch - 1) / pitch;
    return std::clamp(segment, 0, (int)(max_value / STEP)) * STEP;
  }

protected:
  /* virtual methods from class Window */
  bool OnKeyCheck(unsigned key_code) const noexcept override {
    return key_code == KEY_LEFT || key_code == KEY_RIGHT;
  }

  bool OnKeyDown(unsigned key_code) noexcept override {
    switch (key_code) {
    case KEY_LEFT:
      Change((int)value - (int)STEP);
      return true;

    case KEY_RIGHT:
      Change(value + STEP);
      return true;
    }

    return PaintWindow::OnKeyDown(key_code);
  }

  bool OnMouseDown(PixelPoint p) noexcept override {
    SetFocus();
    if (greyed)
      return true;

    dragging = true;
    SetCapture();
    Change(ValueAt(p.x));
    return true;
  }

  bool OnMouseMove(PixelPoint p, [[maybe_unused]] unsigned keys) noexcept override {
    if (dragging)
      Change(ValueAt(p.x));
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
    /* a frame, so the colours of the segments stay what they mean */
    const Pen pen(Layout::ScalePenWidth(2), look.focused.background_color);
    canvas.Select(pen);
    canvas.SelectHollowBrush();
    canvas.DrawRectangle(rc.WithPadding(Layout::ScalePenWidth(1)));
  }

  const PixelRect area = GetSegmentArea();
  const int steps = max_value / STEP;
  const int pitch = GetSegmentPitch(area);
  const int gap = std::clamp(pitch / 6, 1, (int)Layout::Scale(3u));

  /* heard, set but cut by the master volume, above 100 percent (the
     program's mixer may clip there), not set */
  const Color heard = greyed ? COLOR_GRAY : Color(0x2e, 0x7d, 0x32);
  const Color cut = greyed ? COLOR_LIGHT_GRAY : Color(0xa5, 0xd6, 0xa7);
  const Color loud = greyed ? COLOR_GRAY : Color(0xc6, 0x28, 0x28);

  for (int i = 0; i < steps; ++i) {
    const unsigned level = (i + 1) * STEP;
    PixelRect segment = area;
    segment.left = area.left + i * pitch;
    segment.right = segment.left + pitch - gap;

    if (level <= value) {
      Color color = level > 100 ? loud : (level <= limit ? heard : cut);
      canvas.DrawFilledRectangle(segment, color);
    } else {
      canvas.DrawFilledRectangle(segment, COLOR_WHITE);
      canvas.DrawOutlineRectangle(segment, COLOR_GRAY);
    }
  }

  StaticString<16> text;
  text.Format("%u %%", value);
  canvas.Select(look.bold_font);
  canvas.SetBackgroundTransparent();
  canvas.SetTextColor(greyed ? COLOR_DARK_GRAY : COLOR_BLACK);
  const PixelSize size = canvas.CalcTextSize(text.c_str());
  canvas.DrawText({rc.right - (int)Layout::GetTextPadding() * 2 - (int)size.width,
                   rc.top + ((int)rc.GetHeight() - (int)size.height) / 2},
                  text.c_str());
}

/**
 * A button that stays pressed while the vario is muted, with a
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

  /* pressed in while muted, like a latching switch */
  canvas.DrawFilledRectangle(rc, muted ? COLOR_DARK_GRAY : COLOR_LIGHT_GRAY);
  if (HasFocus())
    canvas.DrawFilledRectangle(rc.WithPadding(Layout::Scale(2u)),
                               muted
                               ? COLOR_GRAY
                               : look.focused.background_color);

  const Color ink = muted ? COLOR_WHITE : COLOR_BLACK;
  const PixelPoint c = rc.GetCenter();
  const int s = std::min(rc.GetWidth() / 5, rc.GetHeight() / 4);

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
 * The content of the dialog: labelled bars, the mute button and, on the
 * OpenVario with variod running, the switch of the source.
 */
class AudioWindow final : public ContainerWindow {
  const DialogLook &look;

  const bool have_master = HasMasterVolume();
  const bool have_source = IsVariodAvailable();

  VolumeBar master_bar{look, [this](unsigned v){ OnMaster(v); }};
  VolumeBar vario_bar{look, [this](unsigned v){ OnVario(v); }};
  MuteButton mute_button{look, [this](bool m){ OnMute(m); }};
  Button source_button;

  unsigned master_volume = 100;

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
  static PixelSize GetSize(bool with_master) noexcept {
    const unsigned row = Layout::GetMaximumControlHeight();
    const unsigned padding = Layout::GetTextPadding() * 2;
    const unsigned rows = with_master ? 3 : 2;
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
  bool IsSilent() noexcept {
    const auto &s = Settings();
    return IsVarioMuted() ||
      (s.source == VarioSoundSource::INTERNAL && !s.enabled);
  }

  void Update() noexcept;

  void OnMaster(unsigned v) noexcept {
    master_volume = v;
    SetMasterVolume(v);
    changed = true;
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

  void OnMute(bool muted) noexcept {
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
  const bool internal = s.source == VarioSoundSource::INTERNAL;
  const bool silent = IsSilent();

  vario_bar.SetValue(s.volume);

  /* the master volume is the whole output: Android's media volume,
     or on Linux the program's mixer and the mixer of the sound card,
     which variod plays on as well */
  unsigned limit = s.volume;
  if (have_master)
    limit = s.volume * std::min(master_volume, 100u) / 100;
  vario_bar.SetLimit(limit);
  vario_bar.SetGreyed(silent);

  mute_button.SetMuted(silent);

  if (have_source)
    source_button.SetCaption(internal
                             ? _("Vario: Program")
                             : _("Vario: OpenVario"));
}

void
AudioWindow::OnCreate()
{
  ContainerWindow::OnCreate();

  WindowStyle style;
  style.TabStop();

  const PixelRect dummy{0, 0, 1, 1};

  if (have_master) {
    master_volume = GetMasterVolume();
    master_bar.Create(*this, dummy, style);
    master_bar.SetMax(GetMasterVolumeMax(), GetMasterVolumeMax());
    master_bar.SetValue(master_volume);
    master_bar.SetLimit(master_volume);
  }

  vario_bar.Create(*this, dummy, style);
  vario_bar.SetMax(100, have_master ? GetMasterVolumeMax() : 100);
  mute_button.Create(*this, dummy, style);

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

  int y = rc.top + padding;
  const int label_y = (row - (int)look.text_font.GetHeight()) / 2;

  if (have_master) {
    master_label = {rc.left + padding, y + label_y};
    master_bar.Move({rc.left + label_width, y, rc.right - padding, y + row});
    y += row + padding;
  }

  vario_label = {rc.left + padding, y + label_y};
  vario_bar.Move({rc.left + label_width, y, rc.right - padding, y + row});
  y += row + padding;

  const int mute_width = row * 2;
  mute_button.Move({rc.left + padding, y, rc.left + padding + mute_width,
                    y + row});

  if (have_source)
    source_button.Move({rc.left + 2 * padding + mute_width, y,
                        rc.right - padding, y + row});
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
    return AudioWindow::GetSize(HasMasterVolume());
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

void
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
  if (dialog.GetWidget().HasChanged())
    Profile::Save();
}
