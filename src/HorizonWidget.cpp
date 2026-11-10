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
#include "util/StaticString.hxx"

#include <algorithm>
#include <cmath>

/**
 * A Window which renders a terrain and airspace cross-section
 */
class HorizonWindow : public AntiFlickerWindow {
  const HorizonLook &look;
  const bool& inverse;

  AttitudeState attitude;

  GestureManager gestures;
  bool dragging = false;

  const Pen tape_pen{Layout::ScalePenWidth(1), COLOR_WHITE};
  const Brush index_brush{COLOR_WHITE};

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
   * ComputeHeading() in BasicComputer), as a compass tape at the
   * bottom: the scale runs through under a fixed index and covers
   * #TAPE_RANGE degrees to either side, with a mark every 5 degrees
   * and a label every 10.  The value itself stands in a box above the
   * index.  White on black, so it stays readable over sky and ground
   * alike.
   */
  static constexpr int TAPE_RANGE = 20;

  void PaintHeading(Canvas &canvas, Angle heading) noexcept {
    const InfoBoxLook &info_box_look = UIGlobals::GetLook().info_box;
    const Font &label_font = info_box_look.title_font;
    const Font &value_font = info_box_look.value_font;

    const PixelRect rc = canvas.GetRect();
    const int padding = Layout::GetTextPadding();
    const int label_height = int(label_font.GetHeight());
    const int tick_long = label_height * 2 / 3;
    const int tick_short = tick_long / 2;

    /* the tape: marks at the top, labels below them */
    const int tape_width = int(rc.GetWidth()) - 2 * padding;
    const int tape_height = padding + tick_long + label_height + padding;
    const PixelRect tape{rc.left + padding, rc.bottom - padding - tape_height,
                         rc.left + padding + tape_width, rc.bottom - padding};
    const int center_x = tape.GetCenter().x;
    const double pixels_per_degree = tape_width / (2. * TAPE_RANGE);

    canvas.DrawFilledRectangle(tape, COLOR_BLACK);
    canvas.Select(tape_pen);
    canvas.SetBackgroundTransparent();
    canvas.SetTextColor(COLOR_WHITE);
    canvas.Select(label_font);

    const double degrees = heading.AsBearing().Degrees();
    for (int mark = int(std::ceil((degrees - TAPE_RANGE) / 5)) * 5;
         mark <= degrees + TAPE_RANGE; mark += 5) {
      const int x = center_x +
        int(std::lround((mark - degrees) * pixels_per_degree));
      const bool major = mark % 10 == 0;
      canvas.DrawLine({x, tape.top},
                      {x, tape.top + (major ? tick_long : tick_short)});
      if (!major)
        continue;

      const auto label_buffer = MarkLabel((mark % 360 + 360) % 360);
      const char *const label = label_buffer.c_str();
      const PixelSize size = canvas.CalcTextSize(label);
      const int label_x = x - int(size.width) / 2;
      /* a label cut off at the edge of the tape would be misread */
      if (label_x >= tape.left && label_x + int(size.width) <= tape.right)
        canvas.DrawText({label_x, tape.top + tick_long}, label);
    }

    /* the value in a box above the fixed index */
    const auto value_buffer = FormatBearing(heading);
    const char *value = value_buffer.c_str();
    canvas.Select(value_font);
    const PixelSize value_size = canvas.CalcTextSize(value);
    const int box_width = int(value_size.width) + 4 * padding;
    const int box_height = int(value_size.height) + padding;
    const PixelRect box{center_x - box_width / 2, tape.top - box_height,
                        center_x + box_width / 2, tape.top};
    canvas.DrawFilledRectangle(box, COLOR_BLACK);
    canvas.DrawText({center_x - int(value_size.width) / 2, box.top}, value);

    /* the index: a triangle pointing down onto the scale */
    const int half = tick_short;
    const BulkPixelPoint index[3] = {
      {center_x - half, tape.top},
      {center_x + half, tape.top},
      {center_x, tape.top + tick_long},
    };
    canvas.Select(index_brush);
    canvas.DrawPolygon(index, 3);
  }

  /**
   * The label of a 10 degree mark: the cardinal points as letters, as
   * on a compass card, the rest in degrees.
   */
  static StaticString<8> MarkLabel(int degrees) noexcept {
    StaticString<8> label;
    switch (degrees) {
    case 0: label = "N"; break;
    case 90: label = "E"; break;
    case 180: label = "S"; break;
    case 270: label = "W"; break;
    default: label.UnsafeFormat("%d", degrees); break;
    }
    return label;
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
