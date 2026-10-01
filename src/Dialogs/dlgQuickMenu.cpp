// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#include "Asset.hpp"
#include "Audio/Sound.hpp"
#include "Dialogs/Dialogs.h"
#include "Form/Button.hpp"
#include "Form/GridView.hpp"
#include "Input/InputEvents.hpp"
#include "Language/Language.hpp"
#include "LogFile.hpp"
#include "Look/DialogLook.hpp"
#include "Math/Util.hpp"
#include "Menu/ButtonLabel.hpp"
#include "Menu/MenuData.hpp"
#include "Menu/QuickMenuLayout.hpp"
#include "Form/Panel.hpp"
#include "Interface.hpp"
#include "UISettings.hpp"
#include "Renderer/ButtonRenderer.hpp"
#include "ui/canvas/Color.hpp"
#include "Renderer/TextButtonRenderer.hpp"
#include "Renderer/TextRenderer.hpp"
#include "Screen/Layout.hpp"
#include "UIGlobals.hpp"
#include "Widget/WindowWidget.hpp"
#include "ui/control/ScrollBar.hpp"
#include "WidgetDialog.hpp"
#include "ui/canvas/Canvas.hpp"
#include "ui/event/KeyCode.hpp"
#include "util/StaticString.hxx"

#include <boost/container/static_vector.hpp>
#include <cstdlib>
#include <functional>
#include <memory>
#include <type_traits>

class QuickMenuButtonRenderer final : public ButtonRenderer {
  const DialogLook &look;

  TextRenderer text_renderer;

  const StaticString<64> caption;

  /**
   * Is this button of the style "OpenSoar" unusable at the moment?
   * Unlike a disabled window it can still take the focus, so the
   * cursor keys move straight across the field instead of jumping
   * over it; see OpenSoarQuickMenu.
   */
  const bool inactive;

  /**
   * A small text in the upper left corner, e.g. the location in a
   * debug build; empty for none.
   */
  const StaticString<24> tag;

public:
  explicit QuickMenuButtonRenderer(const DialogLook &_look,
                                   const char *_caption,
                                   bool _inactive=false,
                                   const char *_tag="") noexcept
    :look(_look), caption(_caption), inactive(_inactive), tag(_tag) {
    text_renderer.SetCenter();
    text_renderer.SetVCenter();
    text_renderer.SetControl();
  }

  [[gnu::pure]]
  unsigned GetMinimumButtonWidth() const noexcept override;

  void DrawButton(Canvas &canvas, const PixelRect &rc,
                  ButtonState state) const noexcept override;
};

unsigned
QuickMenuButtonRenderer::GetMinimumButtonWidth() const noexcept
{
  return 2 * Layout::GetTextPadding() + look.button.font->TextSize(caption).width;
}

void
QuickMenuButtonRenderer::DrawButton(Canvas &canvas, const PixelRect &rc,
                                    ButtonState state) const noexcept
{
  // Draw focus rectangle
  switch (state) {
  case ButtonState::PRESSED:
    canvas.DrawFilledRectangle(rc, look.list.pressed.background_color);
    canvas.SetTextColor(look.list.pressed.text_color);
    break;

  case ButtonState::FOCUSED:
    if (inactive) {
      /* a focus of its own colour: the pilot sees at once that this
         one does nothing now */
      canvas.DrawFilledRectangle(rc, COLOR_DARK_GRAY);
      canvas.SetTextColor(COLOR_WHITE);
      break;
    }

    canvas.DrawFilledRectangle(rc, look.focused.background_color);
    canvas.SetTextColor(look.focused.text_color);
    break;

  case ButtonState::SELECTED:
  case ButtonState::ENABLED:
    if (HaveClipping())
      canvas.DrawFilledRectangle(rc, look.background_brush);
    canvas.SetTextColor(inactive ? look.button.disabled.color
                                 : look.text_color);
    break;

  case ButtonState::DISABLED:
    if (HaveClipping())
      canvas.DrawFilledRectangle(rc, look.background_brush);
    canvas.SetTextColor(look.button.disabled.color);
    break;
  }

  canvas.Select(*look.button.font);
  canvas.SetBackgroundTransparent();

  text_renderer.Draw(canvas, rc, caption);

  if (!tag.empty()) {
    canvas.Select(look.text_font);
    canvas.DrawText(rc.GetTopLeft() + PixelSize{(int)Layout::GetTextPadding(), 0},
                    tag);
  }
}

/**
 * A button of the flight phase row; the phase that is shown looks
 * pressed, like a switch that is on.
 */
class PhaseButtonRenderer final : public TextButtonRenderer {
  const bool active;

public:
  PhaseButtonRenderer(const ButtonLook &_look, const char *_caption,
                      bool _active) noexcept
    :TextButtonRenderer(_look, _caption), active(_active) {}

  void DrawButton(Canvas &canvas, const PixelRect &rc,
                  ButtonState state) const noexcept override {
    if (active &&
        (state == ButtonState::ENABLED || state == ButtonState::SELECTED))
      state = ButtonState::PRESSED;

    TextButtonRenderer::DrawButton(canvas, rc, state);
  }
};

using QuickMenuPhase = InputEvents::QuickMenuPhase;

/**
 * The flight phases the quick menu can switch between, in the order
 * of their buttons.
 */
static constexpr struct {
  QuickMenuPhase phase;
  const char *label;
} quick_menu_phases[] = {
  { QuickMenuPhase::GROUND, N_("Ground") },
  { QuickMenuPhase::FLIGHT, N_("Flight") },
  { QuickMenuPhase::AFTER, N_("After") },
  { QuickMenuPhase::ALL, N_("All") },
};

/**
 * The modal result of the phase buttons: this plus the index in
 * #quick_menu_phases.
 */
static constexpr int mrPhase = 1000;

/**
 * What ShowQuickMenu() returns for a phase button: this minus the
 * index in #quick_menu_phases; an event number is never negative.
 */
static constexpr int PHASE_RESULT = -2;

class QuickMenu final : public WindowWidget {
  WndForm &dialog;
  const Menu &menu;

  /**
   * The name of the shown phase for the caption; nullptr if there are
   * no phase lists.
   */
  const char *const phase_name;

  boost::container::static_vector<Button, GridView::MAX_ITEMS> buttons;

  unsigned row_height = 0;
  unsigned titlebar_height = 0;
  unsigned column_width = 0;

  Button *previous_button = nullptr;
  Button *next_button = nullptr;

  static constexpr unsigned MIN_COLUMNS = 3;
  static constexpr unsigned DESIRED_COLUMNS = 3;
  static const unsigned DESIRED_COLUMN_WIDTH;

  void CalculateColumnWidth(unsigned available_width) noexcept;
  PixelRect CalculateConstrainedGridRect(const PixelRect &rc) const noexcept;

public:
  unsigned clicked_event;

  QuickMenu(WndForm &_dialog, const Menu &_menu,
            const char *_phase_name) noexcept
    :dialog(_dialog), menu(_menu), phase_name(_phase_name) {}

  auto &GetWindow() noexcept {
    return (GridView &)WindowWidget::GetWindow();
  }

  void NavigatePage(GridView::Direction direction) noexcept;
  void UpdateCaption() noexcept;
  void SetNavigationButtons(Button *prev, Button *next) noexcept {
    previous_button = prev;
    next_button = next;
  }

  bool Focus() noexcept {
    return SetFocus();
  }

  bool IsWindowReady() const noexcept {
    return IsDefined();
  }

protected:
  /* virtual methods from class Widget */
  void Prepare(ContainerWindow &parent, const PixelRect &rc) noexcept override;
  void Show(const PixelRect &rc) noexcept override;
  void Move(const PixelRect &rc) noexcept override;
  bool SetFocus() noexcept override;
  bool KeyPress(unsigned key_code) noexcept override;
};
void
QuickMenu::Prepare(ContainerWindow &parent, [[maybe_unused]] const PixelRect &rc) noexcept
{
  WindowStyle grid_view_style;
  grid_view_style.ControlParent();
  grid_view_style.Hide();

  const auto &dialog_look = UIGlobals::GetDialogLook();

  const auto &font = *dialog_look.button.font;

  titlebar_height = dialog_look.caption.font->GetHeight();

  PixelRect client_rc = dialog.GetClientAreaWindow().GetClientRect();
  const unsigned original_height = client_rc.GetHeight();
  if (original_height > titlebar_height) {
    client_rc.bottom = client_rc.top + (original_height - titlebar_height);
  }

  const unsigned available_width = client_rc.GetWidth();
  unsigned num_cols = std::max(MIN_COLUMNS,
                                std::min(DESIRED_COLUMNS,
                                        available_width / DESIRED_COLUMN_WIDTH));
  if (num_cols == 0) num_cols = 1;
  column_width = available_width / num_cols;

  row_height =
    std::max(2 * (Layout::GetTextPadding() + font.GetHeight()),
             Layout::GetMaximumControlHeight());

  const PixelRect constrained_grid_rc = CalculateConstrainedGridRect(client_rc);
  auto grid_view = std::make_unique<GridView>();
  grid_view->Create(parent, dialog_look, constrained_grid_rc, grid_view_style,
                    column_width, row_height);

  WindowStyle buttonStyle;
  buttonStyle.TabStop();

  grid_view->RefreshLayout();

  for (unsigned i = 0; i < menu.MAX_ITEMS; ++i) {
    if (buttons.size() >= buttons.max_size())
      continue;

    const auto &menuItem = menu[i];
    if (!menuItem.IsDefined())
      continue;

    char buffer[100];
    const auto expanded =
      ButtonLabel::Expand(menuItem.label, std::span{buffer});
    if (!expanded.visible)
      continue;

    PixelRect button_rc;
    button_rc.left = 0;
    button_rc.top = 0;
    button_rc.right = Layout::Scale(80);
    button_rc.bottom = Layout::Scale(30);

    auto &button = buttons.emplace_back(*grid_view, button_rc, buttonStyle,
                                        std::make_unique<QuickMenuButtonRenderer>(dialog_look,
                                                                                  expanded.text),
                                        [this, &menuItem](){
                                          clicked_event = menuItem.event;
                                          dialog.SetModalResult(mrOK);
                                        });
    button.SetEnabled(expanded.enabled);

    grid_view->AddItem(button);
  }

  grid_view->RefreshLayout();

  SetWindow(std::move(grid_view));
  UpdateCaption();
}

const unsigned QuickMenu::DESIRED_COLUMN_WIDTH = Layout::PtScale(160);

void
QuickMenu::CalculateColumnWidth(unsigned available_width) noexcept
{
  unsigned num_cols = std::max(MIN_COLUMNS,
                               std::min(DESIRED_COLUMNS,
                                       available_width / DESIRED_COLUMN_WIDTH));
  if (num_cols == 0) num_cols = 1;
  column_width = available_width / num_cols;
}

void
QuickMenu::NavigatePage(GridView::Direction direction) noexcept
{
  if (!IsWindowReady())
    return;
  auto &grid_view = GetWindow();
  grid_view.RefreshLayout();
  grid_view.ShowNextPage(direction);
  Focus();
  UpdateCaption();
}

PixelRect
QuickMenu::CalculateConstrainedGridRect(const PixelRect &rc) const noexcept
{
  if (row_height == 0)
    return rc;

  const unsigned available_height = rc.GetHeight();
  const unsigned constrained_height = available_height > row_height / 2
    ? available_height - row_height / 2
    : available_height;
  return PixelRect(0, 0, std::max(1u, rc.GetWidth()),
                    std::max(1u, constrained_height));
}

void
QuickMenu::Show(const PixelRect &rc) noexcept
{
  CalculateColumnWidth(rc.GetWidth());

  const PixelRect constrained_rc = CalculateConstrainedGridRect(rc);
  WindowWidget::Show(constrained_rc);

  auto &grid_view = GetWindow();
  grid_view.SetColumnWidth(column_width);
  grid_view.RefreshLayout();

  UpdateCaption();
}

void
QuickMenu::Move(const PixelRect &rc) noexcept
{
  CalculateColumnWidth(rc.GetWidth());

  const PixelRect constrained_rc = CalculateConstrainedGridRect(rc);
  WindowWidget::Move(constrained_rc);

  auto &grid_view = GetWindow();
  grid_view.SetColumnWidth(column_width);
  grid_view.RefreshLayout();

  UpdateCaption();
}



void
QuickMenu::UpdateCaption() noexcept
{
  auto &grid_view = GetWindow();
  StaticString<64> buffer;
  unsigned pageSize = grid_view.GetNumColumns() * grid_view.GetNumRows();
  unsigned lastPage = std::max<unsigned>(1, DivideRoundUp(buttons.size(), pageSize));
  unsigned currentPage = std::min(grid_view.GetCurrentPage(), lastPage - 1u);

  buffer = "Quick Menu";
  if (phase_name != nullptr)
    buffer.AppendFormat(" - %s", phase_name);
  if (lastPage > 1)
    buffer.AppendFormat("  %d/%d", currentPage + 1, lastPage);
  dialog.SetCaption(buffer);

  if (previous_button != nullptr) {
    previous_button->SetEnabled(lastPage > 1);
  }
  if (next_button != nullptr) {
    next_button->SetEnabled(lastPage > 1);
  }
}

bool
QuickMenu::SetFocus() noexcept
{
  auto &grid_view = GetWindow();

  grid_view.RefreshLayout();

  unsigned numColumns = grid_view.GetNumColumns();
  unsigned numRows = grid_view.GetNumRows();
  unsigned pageSize = numColumns * numRows;
  unsigned lastPage = buttons.size() > 0
    ? DivideRoundUp(buttons.size(), pageSize) - 1
    : 0;
  unsigned currentPage = std::min(grid_view.GetCurrentPage(), lastPage);
  unsigned currentPageSize = currentPage == lastPage
    ? buttons.size() % pageSize
    : pageSize;
  if (currentPageSize == 0 && buttons.size() > 0)
    currentPageSize = pageSize;
  
  unsigned maxRowsOnPage = DivideRoundUp(currentPageSize, numColumns);
  unsigned centerCol = numColumns / 2;
  unsigned centerRow = maxRowsOnPage / 2;

  const unsigned pageStart = currentPage * pageSize;
  const unsigned pageEnd = std::min(pageStart + currentPageSize, (unsigned)buttons.size());
  unsigned focusIndex = buttons.size(); // not found

  auto is_focusable = [](const Button &b) noexcept {
    return b.IsVisible() && b.IsEnabled() && b.IsTabStop();
  };

  auto manhattan = [numColumns, pageSize](unsigned idx, unsigned cCol, unsigned cRow) noexcept {
    unsigned pagePos = idx % pageSize;
    unsigned col = pagePos % numColumns;
    unsigned row = pagePos / numColumns;
    return std::abs((int)col - (int)cCol) + std::abs((int)row - (int)cRow);
  };

  int bestDist = INT_MAX;
  for (unsigned i = pageStart; i < pageEnd; ++i) {
    if (!is_focusable(buttons[i]))
      continue;
    int dist = manhattan(i, centerCol, centerRow);
    if (dist < bestDist) {
      bestDist = dist;
      focusIndex = i;
      if (dist == 0) break; // perfect center, stop searching
    }
  }

  if (focusIndex >= buttons.size()) {
    for (unsigned page = 0; page <= lastPage; ++page) {
      unsigned pStart = page * pageSize;
      unsigned pSize = page == lastPage ? buttons.size() % pageSize : pageSize;
      if (pSize == 0 && buttons.size() > 0)
        pSize = pageSize;
      unsigned pEnd = std::min(pStart + pSize, (unsigned)buttons.size());
      for (unsigned i = pStart; i < pEnd; ++i) {
        if (is_focusable(buttons[i])) {
          while (grid_view.GetCurrentPage() != page) {
            if (grid_view.GetCurrentPage() < page)
              grid_view.ShowNextPage(GridView::Direction::RIGHT);
            else
              grid_view.ShowNextPage(GridView::Direction::LEFT);
          }
          buttons[i].SetFocus();
          return true;
        }
      }
    }
    return false;
  }

  buttons[focusIndex].SetFocus();
  return true;
}

bool
QuickMenu::KeyPress(unsigned key_code) noexcept
{
  auto &grid_view = GetWindow();

  switch (key_code) {
  case KEY_LEFT:
    grid_view.MoveFocus(GridView::Direction::LEFT);
    break;

  case KEY_RIGHT:
    grid_view.MoveFocus(GridView::Direction::RIGHT);
    break;

  case KEY_UP:
    grid_view.MoveFocus(GridView::Direction::UP);
    break;

  case KEY_DOWN:
    grid_view.MoveFocus(GridView::Direction::DOWN);
    break;

  case KEY_MENU:
    grid_view.ShowNextPage();
    SetFocus();
    break;

  default:
    return false;
  }

  UpdateCaption();
  return true;
}

/**
 * The window of the quick menu in the style "OpenSoar": it holds the
 * buttons and a scroll bar at the right edge while the field is
 * taller than the screen.
 */
class QuickMenuField final : public PanelControl {
  ScrollBar scroll_bar{UIGlobals::GetDialogLook().button};

  /** the rows of the field, the rows shown and the first one shown */
  unsigned rows = 0, shown = 0, top = 0;

  /** called with the new first row when the scroll bar moves */
  std::function<void(unsigned)> on_scroll;

public:
  /** a small text in the corner of an empty cell (debug builds) */
  struct Tag {
    PixelRect rc;
    StaticString<24> text;
  };

private:
  std::vector<Tag> empty_tags;

public:
  void SetEmptyTags(std::vector<Tag> &&tags) noexcept {
    empty_tags = std::move(tags);
    Invalidate();
  }

  void SetScrollHandler(std::function<void(unsigned)> handler) noexcept {
    on_scroll = std::move(handler);
  }

  /**
   * @return the width the scroll bar takes (0 if there is none)
   */
  unsigned SetScroll(unsigned _rows, unsigned _shown,
                     unsigned _top) noexcept {
    rows = _rows;
    shown = _shown;
    top = _top;

    if (rows > shown) {
      scroll_bar.SetSize(GetSize());
      scroll_bar.SetSlider(rows, shown, top);
    } else
      scroll_bar.Reset();

    Invalidate();
    return scroll_bar.IsDefined() ? scroll_bar.GetWidth() : 0;
  }

protected:
  void OnPaint(Canvas &canvas) noexcept override {
    ContainerWindow::OnPaint(canvas);

    if (!empty_tags.empty()) {
      const auto &look = UIGlobals::GetDialogLook();
      canvas.Select(look.text_font);
      canvas.SetTextColor(look.button.disabled.color);
      canvas.SetBackgroundTransparent();
      for (const auto &t : empty_tags)
        canvas.DrawText(t.rc.GetTopLeft() +
                        PixelSize{(int)Layout::GetTextPadding(), 0},
                        t.text);
    }

    if (scroll_bar.IsDefined())
      scroll_bar.Paint(canvas);
  }

  bool OnMouseDown(PixelPoint p) noexcept override {
    if (!scroll_bar.IsInside(p))
      return ContainerWindow::OnMouseDown(p);

    /* the arrows move by one row, which is easier to hit exactly
       with a finger than a position on the bar; otherwise like the
       lists: grab the slider, or move it to the pointer */
    if (scroll_bar.IsInsideUpArrow(p.y)) {
      if (top > 0)
        Scroll(top - 1);
    } else if (scroll_bar.IsInsideDownArrow(p.y)) {
      if (top + shown < rows)
        Scroll(top + 1);
    } else if (scroll_bar.IsInsideSlider(p))
      scroll_bar.DragBegin(this, p.y);
    else {
      scroll_bar.DragBeginCentred(this);
      DragTo(p.y);
    }

    return true;
  }

  bool OnMouseMove(PixelPoint p, unsigned keys) noexcept override {
    if (scroll_bar.IsDragging()) {
      DragTo(p.y);
      return true;
    }

    return ContainerWindow::OnMouseMove(p, keys);
  }

  bool OnMouseUp(PixelPoint p) noexcept override {
    if (scroll_bar.IsDragging()) {
      scroll_bar.DragEnd(this);
      return true;
    }

    return ContainerWindow::OnMouseUp(p);
  }

  bool OnMouseWheel(PixelPoint p, int delta) noexcept override {
    if (rows <= shown)
      return ContainerWindow::OnMouseWheel(p, delta);

    if (delta > 0 && top > 0)
      Scroll(top - 1);
    else if (delta < 0 && top + shown < rows)
      Scroll(top + 1);
    return true;
  }

  void OnCancelMode() noexcept override {
    scroll_bar.DragEnd(this);
    ContainerWindow::OnCancelMode();
  }

private:
  void DragTo(int y) noexcept {
    Scroll(scroll_bar.DragMove(rows, shown, y));
  }

  void Scroll(unsigned new_top) noexcept {
    if (new_top != top && on_scroll)
      on_scroll(new_top);
  }
};

/**
 * Append a place like " l5:12" (landscape, place 12 in five columns)
 * to the debug tag of a button.
 */
static void
AppendPlace(StaticString<24> &tag, char orientation,
            unsigned columns, unsigned location) noexcept
{
  if (columns > 0)
    tag.AppendFormat(" %c%u:%u", orientation, columns, location);
  else
    tag.AppendFormat(" %c%u", orientation, location);
}

/**
 * The quick menu in the style UISettings::QuickMenuStyle::OPENSOAR:
 * one field of buttons that grows downwards and scrolls, as many
 * columns as fit, every button at its location (see
 * #QuickMenuLayout), with a place of its own in portrait and in
 * landscape if the list gives one.
 */
class OpenSoarQuickMenu final : public WindowWidget {
  WndForm &dialog;
  const Menu &menu;
  const char *const phase_name;

  /**
   * The number of columns a bare location is counted in: 3 for the
   * phase lists, 0 (those of the screen) for the complete list, which
   * then fills the whole width.
   */
  const unsigned default_columns;

  boost::container::static_vector<Button, Menu::MAX_ITEMS> buttons;

  /** the item of each button, an index into #menu */
  std::vector<unsigned> items;

  /** the index of the button to focus first, or -1 */
  int center = -1;

  unsigned columns = QuickMenuLayout::MIN_COLUMNS;
  std::vector<QuickMenuLayout::Cell> cells;

  /** the rows of the field, the rows shown and the first one shown */
  unsigned rows = 0, shown = 0, top = 0;

  /**
   * The narrowest useful button in points; it decides how many
   * columns fit side by side.
   */
  static constexpr unsigned MIN_COLUMN_WIDTH_PT = 110;

public:
  unsigned clicked_event;

  OpenSoarQuickMenu(WndForm &_dialog, const Menu &_menu,
                    const char *_phase_name,
                    unsigned _default_columns) noexcept
    :dialog(_dialog), menu(_menu), phase_name(_phase_name),
     default_columns(_default_columns) {}

  auto &GetWindow() noexcept {
    return (QuickMenuField &)WindowWidget::GetWindow();
  }

  void UpdateCaption() noexcept;

protected:
  /* virtual methods from class Widget */
  void Prepare(ContainerWindow &parent, const PixelRect &rc) noexcept override;
  void Show(const PixelRect &rc) noexcept override;
  void Move(const PixelRect &rc) noexcept override;
  bool SetFocus() noexcept override;
  bool KeyPress(unsigned key_code) noexcept override;

private:
  [[gnu::pure]]
  int GetFocusedIndex() const noexcept;

  /**
   * Arrange the buttons for the current size of the window.  The
   * button that has the focus keeps it, even when it moves to
   * another place (after turning the screen).
   */
  void Relayout() noexcept;

  /** Show the rows from @p new_top on. */
  void ScrollTo(unsigned new_top) noexcept;

  /**
   * The location tags of the empty cells shown (debug builds only),
   * see QuickMenuField::SetEmptyTags().
   */
  [[gnu::pure]]
  std::vector<QuickMenuField::Tag> MakeEmptyTags(const PixelRect &rc,
                                                 int w, int h) const noexcept;

  /** Focus button @p i and scroll so that it and one more row show. */
  void FocusButton(unsigned i) noexcept;

  /**
   * Focus the button marked as the centre, otherwise the button
   * nearest to the middle of the rows shown.
   */
  void FocusDefault() noexcept;

  /** Focus the button nearest to the middle of the rows shown. */
  void FocusNearestShown() noexcept;
};

void
OpenSoarQuickMenu::Prepare(ContainerWindow &parent,
                           [[maybe_unused]] const PixelRect &rc) noexcept
{
  WindowStyle style;
  style.ControlParent();
  style.Hide();

  const auto &dialog_look = UIGlobals::GetDialogLook();

  auto window = std::make_unique<QuickMenuField>();
  window->Create(parent, dialog_look, rc, style);
  window->SetScrollHandler([this](unsigned new_top){ ScrollTo(new_top); });

  WindowStyle button_style;
  button_style.TabStop();

  for (unsigned i = 0; i < menu.MAX_ITEMS; ++i) {
    const auto &item = menu[i];
    if (!item.IsDefined())
      continue;

    /* a hidden button leaves its cell empty; the others stay where
       they are */
    char buffer[100];
    const auto expanded = ButtonLabel::Expand(item.label, std::span{buffer});
    if (!expanded.visible)
      continue;

    /* a button that is unusable at the moment stays focusable (see
       QuickMenuButtonRenderer::inactive); a click only beeps */
    const bool inactive = !expanded.enabled;

    /* in a debug build, each button shows its location (and its own
       places, if it has any), to check a list against the screen */
    StaticString<24> tag;
    if (IsDebug()) {
      tag.Format("#%u", i);
      if (item.portrait > 0)
        AppendPlace(tag, 'p', item.portrait_columns, item.portrait);
      if (item.landscape > 0)
        AppendPlace(tag, 'l', item.landscape_columns, item.landscape);
    }

    auto renderer =
      std::make_unique<QuickMenuButtonRenderer>(dialog_look, expanded.text,
                                                inactive, tag);
    buttons.emplace_back(*window, PixelRect{0, 0, 1, 1}, button_style,
                         std::move(renderer),
                         [this, &item, inactive](){
                           if (inactive) {
                             PlayResource("IDR_WAV_DRIP");
                             return;
                           }

                           clicked_event = item.event;
                           dialog.SetModalResult(mrOK);
                         });
    items.push_back(i);

    if (item.center)
      center = buttons.size() - 1;
  }

  SetWindow(std::move(window));
}

void
OpenSoarQuickMenu::Show(const PixelRect &rc) noexcept
{
  WindowWidget::Show(rc);
  Relayout();

  /* open with the centre in the middle of the rows shown */
  if (center >= 0)
    ScrollTo(QuickMenuLayout::ScrollToCenter(shown, rows,
                                             cells[center].row));

  UpdateCaption();
}

void
OpenSoarQuickMenu::Move(const PixelRect &rc) noexcept
{
  WindowWidget::Move(rc);
  Relayout();
  UpdateCaption();
}

int
OpenSoarQuickMenu::GetFocusedIndex() const noexcept
{
  for (unsigned i = 0; i < buttons.size(); ++i)
    if (buttons[i].HasFocus())
      return i;

  return -1;
}

void
OpenSoarQuickMenu::Relayout() noexcept
{
  const int focused = GetFocusedIndex();

  const PixelRect rc = GetWindow().GetClientRect();
  const auto &font = *UIGlobals::GetDialogLook().button.font;
  const unsigned min_row_height =
    std::max(2 * (Layout::GetTextPadding() + font.GetHeight()),
             Layout::GetMaximumControlHeight());

  columns =
    QuickMenuLayout::ChooseColumns(rc.GetWidth(),
                                   Layout::PtScale(MIN_COLUMN_WIDTH_PT));

  /* the list may place an item elsewhere in portrait or landscape,
     because the number of columns differs */
  const bool portrait = rc.GetHeight() > rc.GetWidth();
  std::vector<QuickMenuLayout::Place> places;
  places.reserve(items.size());
  for (const unsigned i : items) {
    const auto &item = menu[i];
    const unsigned placed = portrait ? item.portrait : item.landscape;
    const unsigned placed_columns = portrait
      ? item.portrait_columns
      : item.landscape_columns;

    if (placed > 0)
      places.push_back({placed_columns > 0 ? placed_columns : default_columns,
                        placed});
    else
      places.push_back({default_columns, i});
  }

  cells = QuickMenuLayout::Arrange(places, columns);
  rows = QuickMenuLayout::CountRows(cells);
  shown = QuickMenuLayout::CountShownRows(rows, rc.GetHeight(),
                                          min_row_height);

  const unsigned row = focused >= 0 ? cells[focused].row : 0;
  ScrollTo(QuickMenuLayout::ScrollToShow(std::min(top, rows), shown, rows,
                                         row));

  if (focused >= 0)
    buttons[focused].SetFocus();
}

void
OpenSoarQuickMenu::ScrollTo(unsigned new_top) noexcept
{
  top = new_top;

  auto &window = GetWindow();
  const PixelRect rc = window.GetClientRect();
  const unsigned bar = window.SetScroll(rows, shown, top);

  const int w = (rc.GetWidth() - (int)bar) / (int)columns;
  const int h = rc.GetHeight() / (int)shown;

  for (unsigned i = 0; i < buttons.size(); ++i) {
    auto &button = buttons[i];
    const auto &cell = cells[i];

    if (cell.row < top || cell.row >= top + shown) {
      button.Hide();
      continue;
    }

    const int x = rc.left + (int)cell.column * w;
    const int y = rc.top + (int)(cell.row - top) * h;
    button.Move({x, y, x + w, y + h});
    button.Show();
  }

  if (IsDebug())
    window.SetEmptyTags(MakeEmptyTags(rc, w, h));

  UpdateCaption();
}

std::vector<QuickMenuField::Tag>
OpenSoarQuickMenu::MakeEmptyTags(const PixelRect &rc,
                                 int w, int h) const noexcept
{
  /* the location an empty cell would have, counted like a bare
     location of this list; none outside its columns */
  const unsigned c = default_columns > 0 ? default_columns : columns;
  const int offset = ((int)columns - (int)c) / 2;

  std::vector<bool> used(shown * columns, false);
  for (const auto &cell : cells)
    if (cell.row >= top && cell.row < top + shown)
      used[(cell.row - top) * columns + cell.column] = true;

  std::vector<QuickMenuField::Tag> tags;
  for (unsigned r = 0; r < shown; ++r) {
    for (unsigned column = 0; column < columns; ++column) {
      const int n = (int)column - offset;
      if (used[r * columns + column] || n < 0 || n >= (int)c)
        continue;

      QuickMenuField::Tag tag;
      const int x = rc.left + (int)column * w;
      const int y = rc.top + (int)r * h;
      tag.rc = {x, y, x + w, y + h};
      tag.text.Format("#%u", (top + r) * c + n + 1);
      tags.push_back(tag);
    }
  }

  return tags;
}

void
OpenSoarQuickMenu::FocusButton(unsigned i) noexcept
{
  const unsigned new_top =
    QuickMenuLayout::ScrollToShow(top, shown, rows, cells[i].row);
  if (new_top != top)
    ScrollTo(new_top);

  buttons[i].SetFocus();
}

void
OpenSoarQuickMenu::FocusDefault() noexcept
{
  if (center >= 0)
    FocusButton(center);
  else
    FocusNearestShown();
}

void
OpenSoarQuickMenu::FocusNearestShown() noexcept
{
  /* the button nearest to the middle of the rows shown */
  int best = -1;
  unsigned best_distance = UINT_MAX;
  for (unsigned i = 0; i < buttons.size(); ++i) {
    if (cells[i].row < top || cells[i].row >= top + shown)
      continue;

    const int dx = 2 * (int)cells[i].column - ((int)columns - 1);
    const int dy = 2 * (int)(cells[i].row - top) - ((int)shown - 1);
    const unsigned distance = dx * dx + dy * dy;
    if (distance < best_distance) {
      best_distance = distance;
      best = i;
    }
  }

  if (best >= 0)
    FocusButton(best);
}

bool
OpenSoarQuickMenu::SetFocus() noexcept
{
  if (GetFocusedIndex() < 0)
    FocusDefault();

  /* the focus is sometimes not visible when the menu opens (seen on
     the device, not reproduced under X11); log what happened to find
     out why */
  if (IsDebug())
    LogFmt("QuickMenu: initial focus on button {} of {}",
           GetFocusedIndex(), buttons.size());

  return true;
}

bool
OpenSoarQuickMenu::KeyPress(unsigned key_code) noexcept
{
  QuickMenuLayout::Direction direction;

  switch (key_code) {
  case KEY_LEFT:
    direction = QuickMenuLayout::Direction::LEFT;
    break;

  case KEY_RIGHT:
    direction = QuickMenuLayout::Direction::RIGHT;
    break;

  case KEY_UP:
    direction = QuickMenuLayout::Direction::UP;
    break;

  case KEY_DOWN:
    direction = QuickMenuLayout::Direction::DOWN;
    break;

  case KEY_MENU:
    /* one screen further down, and from the end back to the top */
    if (rows > shown) {
      ScrollTo(top + shown < rows ? std::min(top + shown, rows - shown) : 0);

      /* the focus follows into the rows shown */
      const int focused = GetFocusedIndex();
      if (focused < 0 || cells[focused].row < top ||
          cells[focused].row >= top + shown)
        FocusNearestShown();
    }
    return true;

  default:
    return false;
  }

  const int focused = GetFocusedIndex();
  if (focused < 0) {
    if (IsDebug())
      LogFmt("QuickMenu: no button had the focus at a cursor key");
    FocusDefault();
    return true;
  }

  /* unusable buttons take the focus as well, so the focus moves
     straight in the direction of the key; XCSoar jumps over them,
     which can lead it off to the side */
  const int i = QuickMenuLayout::Navigate(cells, focused, direction);
  if (i >= 0)
    FocusButton(i);

  return true;
}

void
OpenSoarQuickMenu::UpdateCaption() noexcept
{
  StaticString<64> buffer;
  buffer = "Quick Menu";
  if (phase_name != nullptr)
    buffer.AppendFormat(" - %s", phase_name);

  /* where the pilot is in a field that scrolls: the first and last
     row shown and the number of rows */
  if (rows > shown)
    buffer.AppendFormat("  %u-%u/%u", top + 1, top + shown, rows);

  dialog.SetCaption(buffer);
}

/**
 * @param W the widget class of the quick menu style
 */
template<class W>
class QuickMenuDialog final : public WidgetDialog {
  W *quick_menu_widget = nullptr;

  /**
   * The number of phase buttons, which come first in the button
   * panel.
   */
  unsigned n_phase_buttons = 0;

public:
  QuickMenuDialog(Full, UI::SingleWindow &parent, const DialogLook &look,
                  const char *caption) noexcept
    :WidgetDialog(Full{}, parent, look, caption) {}

  template<typename... Args>
  void SetWidget(Args&&... args) {
    auto widget = std::make_unique<W>(std::forward<Args>(args)...);
    quick_menu_widget = widget.get();
    FinishPreliminary(std::move(widget));
  }

  void SetPhaseButtonCount(unsigned n) noexcept {
    n_phase_buttons = n;
  }

  // Intentionally hides WidgetDialog::GetWidget() to return the quick
  // menu widget instead of Widget& for type-specific behavior
  W &GetWidget() noexcept {
    return *quick_menu_widget;
  }

  // Intentionally hides WndForm::ShowModal() to provide custom modal
  // behavior with auto-sizing and button layout
  int ShowModal() {
    if (IsAutoSize())
      AutoSize();
    else
      widget.Move(LayoutButtons());

    widget.Show();
    int result = WndForm::ShowModal();
    widget.Hide();
    return result;
  }

protected:
  void OnResize(PixelSize new_size) noexcept override {
    WndForm::OnResize(new_size);

    if (IsAutoSize())
      return;

    widget.Move(LayoutButtons());
  }

private:
  /**
   * In landscape all buttons share the bottom row (as far as they
   * fit); in portrait the phase buttons get a row of their own above
   * the page and close buttons, so that none of them gets too narrow.
   */
  PixelRect LayoutButtons() noexcept {
    const PixelRect rc = GetClientAreaWindow().GetClientRect();
    if (n_phase_buttons > 0 && rc.GetWidth() < rc.GetHeight())
      return buttons.TwoRowBottomLayout(rc, n_phase_buttons);

    return buttons.BottomLayout(rc);
  }
};

/**
 * @param W the widget class of the quick menu style
 * @return the event of the chosen item, -1 if the dialog was closed,
 * or #PHASE_RESULT minus the index of a phase button
 */
/**
 * The quick menu of a flight phase, or nullptr if there is no list
 * for it.
 */
[[gnu::pure]]
static const Menu *
GetPhaseMenu(QuickMenuPhase phase) noexcept
{
  const Menu *menu =
    InputEvents::GetMenu(InputEvents::GetQuickMenuMode(phase));
  return menu != nullptr && !menu->IsEmpty() ? menu : nullptr;
}

template<class W>
static int
ShowQuickMenu(UI::SingleWindow &parent, const Menu &all_menu) noexcept
{
  const auto &dialog_look = UIGlobals::GetDialogLook();

  /* the phase lists belong to the style "OpenSoar"; the XCSoar style
     always shows the complete list, as XCSoar does.  Only phases with
     a list of their own get a button. */
  bool has_phases = false;
  if constexpr (std::is_same_v<W, OpenSoarQuickMenu>)
    for (const auto &i : quick_menu_phases)
      if (i.phase != QuickMenuPhase::ALL &&
          GetPhaseMenu(i.phase) != nullptr)
        has_phases = true;

  QuickMenuPhase phase = InputEvents::GetQuickMenuPhase();
  const Menu *menu = GetPhaseMenu(phase);
  if (!has_phases || phase == QuickMenuPhase::ALL || menu == nullptr) {
    /* no list for this phase: show all buttons */
    phase = QuickMenuPhase::ALL;
    menu = &all_menu;
  }

  const char *phase_name = nullptr;
  if (has_phases)
    for (const auto &i : quick_menu_phases)
      if (i.phase == phase)
        phase_name = gettext(i.label);

  QuickMenuDialog<W> dialog(WidgetDialog::Full{},
                         parent,
                         dialog_look, nullptr);

  if constexpr (std::is_same_v<W, OpenSoarQuickMenu>)
    /* the phase lists are written for three columns and keep that
       picture in the middle of a wider screen; the complete list
       fills the whole width */
    dialog.SetWidget(dialog, *menu, phase_name,
                     phase == QuickMenuPhase::ALL
                     ? 0u : QuickMenuLayout::MIN_COLUMNS);
  else
    dialog.SetWidget(dialog, *menu, phase_name);

  dialog.PrepareWidget();

  auto &quick_menu = dialog.GetWidget();

  /* the phase buttons come first: where the bottom row is too narrow
     (portrait), the button panel wraps them into a row of their own
     above the page and close buttons */
  if (has_phases) {
    unsigned n = 0;
    for (unsigned i = 0; i < std::size(quick_menu_phases); ++i) {
      const auto &p = quick_menu_phases[i];
      if (p.phase != QuickMenuPhase::ALL && GetPhaseMenu(p.phase) == nullptr)
        continue;

      dialog.AddButton(std::make_unique<PhaseButtonRenderer>(dialog_look.button,
                                                             gettext(p.label),
                                                             p.phase == phase),
                       dialog.MakeModalResultCallback(mrPhase + i));
      ++n;
    }

    dialog.SetPhaseButtonCount(n);
  }
  /* the style "OpenSoar" scrolls instead of turning pages */
  if constexpr (!std::is_same_v<W, OpenSoarQuickMenu>) {
    Button *prev_button = dialog.AddSymbolButton("<", [&quick_menu]() {
      quick_menu.NavigatePage(GridView::Direction::LEFT);
    });

    Button *next_button = dialog.AddSymbolButton(">", [&quick_menu]() {
      quick_menu.NavigatePage(GridView::Direction::RIGHT);
    });

    quick_menu.SetNavigationButtons(prev_button, next_button);
  }

  dialog.AddButton(_("Close"), mrCancel);

  quick_menu.UpdateCaption();

  const int result = dialog.ShowModal();
  if (result >= mrPhase &&
      result < mrPhase + (int)std::size(quick_menu_phases))
    return PHASE_RESULT - (result - mrPhase);

  if (result != mrOK)
    return -1;

  return dialog.GetWidget().clicked_event;
}

void
dlgQuickMenuShowModal(UI::SingleWindow &parent) noexcept
{
  const auto *menu =
    InputEvents::GetMenu(InputEvents::GetQuickMenuMode(QuickMenuPhase::ALL));
  if (menu == nullptr)
    return;

  /* a phase button chooses the phase and opens the menu again with
     its list */
  const bool opensoar = CommonInterface::GetUISettings().quick_menu_style ==
    UISettings::QuickMenuStyle::OPENSOAR;

  int result;
  while ((result = opensoar
          ? ShowQuickMenu<OpenSoarQuickMenu>(parent, *menu)
          : ShowQuickMenu<QuickMenu>(parent, *menu)) <= PHASE_RESULT)
    InputEvents::SetQuickMenuPhase(quick_menu_phases[PHASE_RESULT - result].phase);

  if (result >= 0)
    InputEvents::ProcessEvent(result);
}
