// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#include "Asset.hpp"
#include "Dialogs/Dialogs.h"
#include "Form/Button.hpp"
#include "Form/GridView.hpp"
#include "Input/InputEvents.hpp"
#include "Language/Language.hpp"
#include "Look/DialogLook.hpp"
#include "Math/Util.hpp"
#include "Menu/ButtonLabel.hpp"
#include "Menu/MenuData.hpp"
#include "Menu/QuickMenuLayout.hpp"
#include "Form/Panel.hpp"
#include "Interface.hpp"
#include "UISettings.hpp"
#include "ui/canvas/Color.hpp"
#include "Renderer/ButtonRenderer.hpp"
#include "Renderer/TextButtonRenderer.hpp"
#include "Renderer/TextRenderer.hpp"
#include "Screen/Layout.hpp"
#include "UIGlobals.hpp"
#include "Widget/WindowWidget.hpp"
#include "WidgetDialog.hpp"
#include "ui/canvas/Canvas.hpp"
#include "ui/event/KeyCode.hpp"
#include "util/StaticString.hxx"

#include <boost/container/static_vector.hpp>
#include <cstdlib>
#include <memory>

class QuickMenuButtonRenderer final : public ButtonRenderer {
  const DialogLook &look;

  TextRenderer text_renderer;

  const StaticString<64> caption;

public:
  explicit QuickMenuButtonRenderer(const DialogLook &_look,
                                   const char *_caption) noexcept
    :look(_look), caption(_caption) {
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
    canvas.DrawFilledRectangle(rc, look.focused.background_color);
    canvas.SetTextColor(look.focused.text_color);
    break;

  case ButtonState::SELECTED:
  case ButtonState::ENABLED:
    canvas.SetTextColor(look.text_color);
    break;

  case ButtonState::DISABLED:
    canvas.SetTextColor(look.button.disabled.color);
    break;
  }

  canvas.Select(*look.button.font);
  canvas.SetBackgroundTransparent();

  text_renderer.Draw(canvas, rc, caption);
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
  grid_view->Create(parent, constrained_grid_rc, grid_view_style,
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
 * The window of the dynamic quick menu: it holds the buttons and
 * draws the frame around the fixed block.
 */
class DynamicQuickMenuWindow final : public PanelControl {
  /** the outer edge of the frame; empty if there is none */
  PixelRect frame{};
  unsigned frame_width = 0;
  Color frame_color = COLOR_GRAY;

public:
  void SetFrame(const PixelRect &_frame, unsigned width,
                Color color) noexcept {
    frame = _frame;
    frame_width = width;
    frame_color = color;
    Invalidate();
  }

  void ClearFrame() noexcept {
    frame_width = 0;
    Invalidate();
  }

protected:
  void OnPaint(Canvas &canvas) noexcept override {
    ContainerWindow::OnPaint(canvas);

    if (frame_width == 0)
      return;

    /* four bands; the layout keeps the room for them free, so they
       never cover a button */
    const int w = frame_width;
    canvas.DrawFilledRectangle({frame.left, frame.top,
                                frame.right, frame.top + w}, frame_color);
    canvas.DrawFilledRectangle({frame.left, frame.bottom - w,
                                frame.right, frame.bottom}, frame_color);
    canvas.DrawFilledRectangle({frame.left, frame.top,
                                frame.left + w, frame.bottom}, frame_color);
    canvas.DrawFilledRectangle({frame.right - w, frame.top,
                                frame.right, frame.bottom}, frame_color);
  }
};

/**
 * The quick menu in the style UISettings::QuickMenuStyle::DYNAMIC: a
 * framed block of 3 x 5 buttons in the middle of the first page and
 * the other buttons around it, see #QuickMenuLayout.
 */
class DynamicQuickMenu final : public WindowWidget {
  WndForm &dialog;
  const Menu &menu;
  const char *const phase_name;

  boost::container::static_vector<Button, Menu::MAX_ITEMS> buttons;

  /** the location of each button, see #QuickMenuLayout::Arrange() */
  std::vector<unsigned> locations;

  QuickMenuLayout::Grid grid{QuickMenuLayout::BLOCK_COLUMNS,
                             QuickMenuLayout::BLOCK_ROWS};
  std::vector<QuickMenuLayout::Cell> cells;
  unsigned current_page = 0, page_count = 1;

  Button *previous_button = nullptr;
  Button *next_button = nullptr;

  /**
   * The narrowest useful button in points; it decides how many
   * columns fit side by side.
   */
  static constexpr unsigned MIN_COLUMN_WIDTH_PT = 110;

  /**
   * The width of the frame around the block, about 2 mm.
   */
  static int FrameWidth() noexcept {
    return std::max(2u, Layout::PtScale(6));
  }

public:
  unsigned clicked_event;

  DynamicQuickMenu(WndForm &_dialog, const Menu &_menu,
                   const char *_phase_name) noexcept
    :dialog(_dialog), menu(_menu), phase_name(_phase_name) {}

  auto &GetWindow() noexcept {
    return (DynamicQuickMenuWindow &)WindowWidget::GetWindow();
  }

  void SetNavigationButtons(Button *prev, Button *next) noexcept {
    previous_button = prev;
    next_button = next;
  }

  void NavigatePage(GridView::Direction direction) noexcept;
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
   * another page (after turning the screen).
   */
  void Relayout() noexcept;

  void ShowPage(unsigned page) noexcept;

  /** Focus the block centre, or the button nearest to the page
      centre. */
  void FocusDefault() noexcept;
};

void
DynamicQuickMenu::Prepare(ContainerWindow &parent,
                          [[maybe_unused]] const PixelRect &rc) noexcept
{
  WindowStyle style;
  style.ControlParent();
  style.Hide();

  const auto &dialog_look = UIGlobals::GetDialogLook();

  auto window = std::make_unique<DynamicQuickMenuWindow>();
  window->Create(parent, rc, style);

  WindowStyle button_style;
  button_style.TabStop();

  for (unsigned i = 0; i < menu.MAX_ITEMS; ++i) {
    const auto &item = menu[i];
    if (!item.IsDefined())
      continue;

    char buffer[100];
    const auto expanded = ButtonLabel::Expand(item.label, std::span{buffer});
    if (!expanded.visible)
      continue;

    auto &button =
      buttons.emplace_back(*window, PixelRect{0, 0, 1, 1}, button_style,
                           std::make_unique<QuickMenuButtonRenderer>(dialog_look,
                                                                     expanded.text),
                           [this, &item](){
                             clicked_event = item.event;
                             dialog.SetModalResult(mrOK);
                           });
    button.SetEnabled(expanded.enabled);
    locations.push_back(i);
  }

  SetWindow(std::move(window));
}

void
DynamicQuickMenu::Show(const PixelRect &rc) noexcept
{
  WindowWidget::Show(rc);
  Relayout();
  UpdateCaption();
}

void
DynamicQuickMenu::Move(const PixelRect &rc) noexcept
{
  WindowWidget::Move(rc);
  Relayout();
  UpdateCaption();
}

int
DynamicQuickMenu::GetFocusedIndex() const noexcept
{
  for (unsigned i = 0; i < buttons.size(); ++i)
    if (buttons[i].HasFocus())
      return i;

  return -1;
}

void
DynamicQuickMenu::Relayout() noexcept
{
  const int focused = GetFocusedIndex();

  const PixelRect rc = GetWindow().GetClientRect();
  const auto &font = *UIGlobals::GetDialogLook().button.font;
  const unsigned min_row_height =
    std::max(2 * (Layout::GetTextPadding() + font.GetHeight()),
             Layout::GetMaximumControlHeight());

  grid = QuickMenuLayout::ChooseGrid(rc.GetWidth(), rc.GetHeight(),
                                     Layout::PtScale(MIN_COLUMN_WIDTH_PT),
                                     min_row_height);
  cells = QuickMenuLayout::Arrange(locations, grid);
  page_count = QuickMenuLayout::CountPages(cells);

  if (focused >= 0)
    current_page = cells[focused].page;
  else if (current_page >= page_count)
    current_page = 0;

  ShowPage(current_page);

  if (focused >= 0)
    buttons[focused].SetFocus();
}

void
DynamicQuickMenu::ShowPage(unsigned page) noexcept
{
  current_page = page;

  auto &window = GetWindow();
  const PixelRect rc = window.GetClientRect();

  /* the frame takes room of its own on both sides of the block, so
     the cells around it are not covered */
  const int f = FrameWidth();
  const int w = (rc.GetWidth() - 2 * f) / grid.columns;
  const int h = (rc.GetHeight() - 2 * f) / grid.rows;
  const unsigned block_left = grid.GetBlockLeft();
  const unsigned block_top = grid.GetBlockTop();
  const unsigned block_right = block_left + QuickMenuLayout::BLOCK_COLUMNS;
  const unsigned block_bottom = block_top + QuickMenuLayout::BLOCK_ROWS;

  auto x = [&](unsigned column) noexcept {
    return rc.left + (int)column * w +
      (column >= block_left ? f : 0) + (column >= block_right ? f : 0);
  };

  auto y = [&](unsigned row) noexcept {
    return rc.top + (int)row * h +
      (row >= block_top ? f : 0) + (row >= block_bottom ? f : 0);
  };

  for (unsigned i = 0; i < buttons.size(); ++i) {
    auto &button = buttons[i];
    const auto &cell = cells[i];

    if (cell.page != page) {
      button.Hide();
      continue;
    }

    button.Move({x(cell.column), y(cell.row),
                 x(cell.column) + w, y(cell.row) + h});
    button.Show();
  }

  if (page == 0)
    window.SetFrame({x(block_left) - f, y(block_top) - f,
                     x(block_right - 1) + w + f, y(block_bottom - 1) + h + f},
                    f, COLOR_GRAY);
  else
    window.ClearFrame();
}

void
DynamicQuickMenu::FocusDefault() noexcept
{
  /* the block centre on the first page */
  if (current_page == 0)
    for (unsigned i = 0; i < buttons.size(); ++i)
      if (locations[i] == QuickMenuLayout::CENTER_LOCATION &&
          buttons[i].IsEnabled()) {
        buttons[i].SetFocus();
        return;
      }

  /* otherwise the enabled button nearest to the page centre */
  int best = -1;
  unsigned best_distance = UINT_MAX;
  for (unsigned i = 0; i < buttons.size(); ++i) {
    if (cells[i].page != current_page || !buttons[i].IsEnabled())
      continue;

    const int dx = (int)cells[i].column - (int)grid.columns / 2;
    const int dy = (int)cells[i].row - (int)grid.rows / 2;
    const unsigned distance = dx * dx + dy * dy;
    if (distance < best_distance) {
      best_distance = distance;
      best = i;
    }
  }

  if (best >= 0)
    buttons[best].SetFocus();
}

bool
DynamicQuickMenu::SetFocus() noexcept
{
  if (GetFocusedIndex() < 0)
    FocusDefault();
  return true;
}

bool
DynamicQuickMenu::KeyPress(unsigned key_code) noexcept
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
    NavigatePage(GridView::Direction::RIGHT);
    return true;

  default:
    return false;
  }

  const int focused = GetFocusedIndex();
  if (focused < 0) {
    FocusDefault();
    return true;
  }

  /* skip disabled buttons, like the grid of the XCSoar style */
  int i = focused;
  while ((i = QuickMenuLayout::Navigate(cells, i, direction)) >= 0)
    if (buttons[i].IsEnabled()) {
      buttons[i].SetFocus();
      break;
    }

  return true;
}

void
DynamicQuickMenu::NavigatePage(GridView::Direction direction) noexcept
{
  if (page_count < 2)
    return;

  const unsigned page = direction == GridView::Direction::LEFT
    ? (current_page + page_count - 1) % page_count
    : (current_page + 1) % page_count;

  ShowPage(page);
  FocusDefault();
  UpdateCaption();
}

void
DynamicQuickMenu::UpdateCaption() noexcept
{
  StaticString<64> buffer;
  buffer = "Quick Menu";
  if (phase_name != nullptr)
    buffer.AppendFormat(" - %s", phase_name);
  if (page_count > 1)
    buffer.AppendFormat("  %u/%u", current_page + 1, page_count);
  dialog.SetCaption(buffer);

  if (previous_button != nullptr)
    previous_button->SetEnabled(page_count > 1);
  if (next_button != nullptr)
    next_button->SetEnabled(page_count > 1);
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

  /* only phases with a list of their own get a button; without any
     phase list the menu is the same as before there were phases */
  bool has_phases = false;
  for (const auto &i : quick_menu_phases)
    if (i.phase != QuickMenuPhase::ALL && GetPhaseMenu(i.phase) != nullptr)
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
  Button *prev_button = dialog.AddSymbolButton("<", [&quick_menu]() {
    quick_menu.NavigatePage(GridView::Direction::LEFT);
  });

  Button *next_button = dialog.AddSymbolButton(">", [&quick_menu]() {
    quick_menu.NavigatePage(GridView::Direction::RIGHT);
  });

  dialog.AddButton(_("Close"), mrCancel);

  quick_menu.SetNavigationButtons(prev_button, next_button);

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
  const bool dynamic = CommonInterface::GetUISettings().quick_menu_style ==
    UISettings::QuickMenuStyle::DYNAMIC;

  int result;
  while ((result = dynamic
          ? ShowQuickMenu<DynamicQuickMenu>(parent, *menu)
          : ShowQuickMenu<QuickMenu>(parent, *menu)) <= PHASE_RESULT)
    InputEvents::SetQuickMenuPhase(quick_menu_phases[PHASE_RESULT - result].phase);

  if (result >= 0)
    InputEvents::ProcessEvent(result);
}
