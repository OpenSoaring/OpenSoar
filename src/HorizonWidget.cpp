// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#include "HorizonWidget.hpp"
#include "UIGlobals.hpp"
#include "Look/Look.hpp"
#include "Look/DialogLook.hpp"
#include "Look/InfoBoxLook.hpp"
#include "Interface.hpp"
#include "Input/InputEvents.hpp"
#include "UIUtil/GestureManager.hpp"
#include "Screen/Layout.hpp"
#include "ui/window/AntiFlickerWindow.hpp"
#include "ui/canvas/Canvas.hpp"
#include "Renderer/HorizonRenderer.hpp"
#include "Formatter/AngleFormatter.hpp"
#include "Language/Language.hpp"

#include <algorithm>

/**
 * A Window which renders a terrain and airspace cross-section
 */
class HorizonWindow : public AntiFlickerWindow {
  const HorizonLook &look;
  const bool& inverse;

  AttitudeState attitude;

  GestureManager gestures;
  bool dragging = false;

public:
  /**
   * Constructor. Initializes most class members.
   */
  HorizonWindow(const HorizonLook &_look, const bool &_inverse) noexcept
    :look(_look),inverse(_inverse)
  {
    attitude.Reset();
  }

  void ReadBlackboard(const AttitudeState _attitude) noexcept {
    attitude = _attitude;
    Invalidate();
  }

private:
  void StopDragging() noexcept {
    if (!dragging)
      return;

    dragging = false;
    ReleaseCapture();
  }

protected:
  /* virtual methods from AntiFlickerWindow */
  void OnPaintBuffer(Canvas &canvas) noexcept override {
    if (inverse)
      canvas.Clear(COLOR_BLACK);
    else
      canvas.ClearWhite();

    if (!attitude.bank_angle_available && !attitude.pitch_angle_available)
      PaintNoData(canvas);
    else
      HorizonRenderer::Draw(canvas, canvas.GetRect(), look, attitude);

    /* the heading on its own is no attitude indicator, so it is shown
       even when the device withholds bank and pitch */
    if (attitude.heading_available)
      PaintHeading(canvas, attitude.heading);
  }

  /**
   * Without attitude data the page would stay an empty white (or black)
   * area, which looks like a broken display.  Name the page and say
   * why nothing is drawn, so the pilot knows the device sends no
   * attitude.
   */
  void PaintNoData(Canvas &canvas) noexcept {
    const DialogLook &dialog_look = UIGlobals::GetDialogLook();
    const char *title = _("Horizon");
    const char *text = _("No attitude data available");

    canvas.SetBackgroundTransparent();
    canvas.SetTextColor(inverse ? COLOR_WHITE : COLOR_BLACK);

    canvas.Select(dialog_look.bold_font);
    const PixelSize title_size = canvas.CalcTextSize(title);
    canvas.Select(dialog_look.text_font);
    const PixelSize text_size = canvas.CalcTextSize(text);

    const PixelRect rc = canvas.GetRect();
    const int gap = Layout::GetTextPadding();
    const int top = rc.GetCenter().y -
      int(title_size.height + gap + text_size.height) / 2;
    const int center_x = rc.GetCenter().x;

    canvas.Select(dialog_look.bold_font);
    canvas.DrawText({center_x - int(title_size.width) / 2, top}, title);
    canvas.Select(dialog_look.text_font);
    canvas.DrawText({center_x - int(text_size.width) / 2,
                     top + int(title_size.height) + gap},
                    text);
  }

  /**
   * The direction of the longitudinal axis, from a compass or, without
   * one, from the GPS track corrected for the wind (see
   * ComputeHeading() in BasicComputer).  A box at the bottom centre,
   * white on black, so it stays readable over sky and ground alike.
   */
  void PaintHeading(Canvas &canvas, Angle heading) noexcept {
    const InfoBoxLook &info_box_look = UIGlobals::GetLook().info_box;
    const char *label = _("Heading");
    const auto value_buffer = FormatBearing(heading);
    const char *value = value_buffer.c_str();

    canvas.Select(info_box_look.title_font);
    const PixelSize label_size = canvas.CalcTextSize(label);
    canvas.Select(info_box_look.value_font);
    const PixelSize value_size = canvas.CalcTextSize(value);

    const int padding = Layout::GetTextPadding();
    const int width = int(std::max(label_size.width, value_size.width)) +
      4 * padding;
    const int height = int(label_size.height + value_size.height) +
      2 * padding;

    const PixelRect rc = canvas.GetRect();
    const int center_x = rc.GetCenter().x;
    const PixelRect box{center_x - width / 2, rc.bottom - padding - height,
                        center_x + width / 2, rc.bottom - padding};
    canvas.DrawFilledRectangle(box, COLOR_BLACK);

    canvas.SetBackgroundTransparent();
    canvas.SetTextColor(COLOR_WHITE);
    canvas.Select(info_box_look.title_font);
    canvas.DrawText({center_x - int(label_size.width) / 2,
                     box.top + padding},
                    label);
    canvas.Select(info_box_look.value_font);
    canvas.DrawText({center_x - int(value_size.width) / 2,
                     box.top + padding + int(label_size.height)},
                    value);
  }

  /* virtual methods from Window */
  bool OnMouseDown(PixelPoint p) noexcept override;
  bool OnMouseUp(PixelPoint p) noexcept override;
  bool OnMouseMove(PixelPoint p, unsigned keys) noexcept override;
  bool OnMouseDouble(PixelPoint p) noexcept override;
  bool OnKeyDown(unsigned key_code) noexcept override;
  void OnCancelMode() noexcept override;
};

bool
HorizonWindow::OnMouseDouble([[maybe_unused]] PixelPoint p) noexcept
{
  StopDragging();
  InputEvents::ShowMenu();
  return true;
}

bool
HorizonWindow::OnMouseDown(PixelPoint p) noexcept
{
  if (!dragging) {
    dragging = true;
    SetCapture();
    gestures.Start(p, Layout::Scale(20));
  }

  return true;
}

bool
HorizonWindow::OnMouseUp([[maybe_unused]] PixelPoint p) noexcept
{
  if (dragging) {
    StopDragging();

    const char *gesture = gestures.Finish();
    if (gesture && InputEvents::processGesture(gesture))
      return true;
  }

  return false;
}

bool
HorizonWindow::OnMouseMove(PixelPoint p,
                           [[maybe_unused]] unsigned keys) noexcept
{
  if (dragging)
    gestures.Update(p);

  return true;
}

void
HorizonWindow::OnCancelMode() noexcept
{
  AntiFlickerWindow::OnCancelMode();
  StopDragging();
}

bool
HorizonWindow::OnKeyDown(unsigned key_code) noexcept
{
  return InputEvents::processKey(key_code);
}

void
HorizonWidget::Update(const MoreData &basic) noexcept
{
  HorizonWindow &w = (HorizonWindow &)GetWindow();
  w.ReadBlackboard(basic.attitude);
  w.Invalidate();
}

void
HorizonWidget::Prepare(ContainerWindow &parent, const PixelRect &rc) noexcept
{
  const Look &look = UIGlobals::GetLook();

  WindowStyle style;
  style.Hide();

  auto w = std::make_unique<HorizonWindow>(look.horizon, look.info_box.inverse);
  w->Create(parent, rc, style);
  SetWindow(std::move(w));
}

void
HorizonWidget::Show(const PixelRect &rc) noexcept
{
  Update(CommonInterface::Basic());
  CommonInterface::GetLiveBlackboard().AddListener(*this);

  WindowWidget::Show(rc);
}

void
HorizonWidget::Hide() noexcept
{
  WindowWidget::Hide();

  CommonInterface::GetLiveBlackboard().RemoveListener(*this);
}

void
HorizonWidget::OnGPSUpdate(const MoreData &basic) noexcept
{
  Update(basic);
}
