// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#include "DownloadFilePicker.hpp"
#include "Error.hpp"
#include "WidgetDialog.hpp"
#include "ProgressDialog.hpp"
#include "Message.hpp"
#include "UIGlobals.hpp"
#include "Look/DialogLook.hpp"
#include "Renderer/TextRowRenderer.hpp"
#include "Form/Button.hpp"
#include "Widget/ListWidget.hpp"
#include "Language/Language.hpp"
#include "LocalPath.hpp"
#include "system/Path.hpp"
#include "io/FileLineReader.hpp"
#include "Repository/Glue.hpp"
#include "Repository/FileRepository.hpp"
#include "Repository/Parser.hpp"
#include "net/http/Features.hpp"
#include "net/http/DownloadManager.hpp"
#include "ui/event/Notify.hpp"
#include "ui/event/PeriodicTimer.hpp"
#include "thread/Mutex.hxx"
#include "Operation/ThreadedOperationEnvironment.hpp"

#include <vector>

#include <cassert>

/**
 * This class tracks a download and updates a #ProgressDialog.
 */
class DownloadProgress final : Net::DownloadListener {
  ProgressDialog &dialog;
  ThreadedOperationEnvironment env;
//  const Path path_relative;
  const std::string name;

  UI::PeriodicTimer update_timer{[this]{ Net::DownloadManager::Enumerate(*this); }};

  UI::Notify download_complete_notify{[this]{ OnDownloadCompleteNotification(); }};

  std::exception_ptr error;

  bool got_size = false, complete = false, success;

public:
  DownloadProgress(ProgressDialog &_dialog, const std::string_view _name)
                   //const Path _path_relative)
    :dialog(_dialog), env(_dialog), name(_name) { // path_relative(_path_relative) {
    update_timer.Schedule(std::chrono::seconds(1));
    Net::DownloadManager::AddListener(*this);
  }

  ~DownloadProgress() {
    Net::DownloadManager::RemoveListener(*this);
  }

  void Rethrow() const {
    if (error)
      std::rethrow_exception(error);
  }

private:
  /* virtual methods from class Net::DownloadListener */
  void OnDownloadAdded(const std::string_view _name, // const DownloadType type, // Path _path_relative,
                       size_t size, size_t position) noexcept override {
//    if (!complete && path_relative == _path_relative) {
    if (!complete && name == _name) {
      if (!got_size && size >= 0) {
        got_size = true;
        env.SetProgressRange(uint64_t(size) / 1024u);
      }

      if (got_size)
        env.SetProgressPosition(uint64_t(position) / 1024u);
    }
  }

  void OnDownloadComplete(const std::string_view _name) noexcept override {
    if (!complete && name == _name) {
      complete = true;
      success = true;
      download_complete_notify.SendNotification();
    }
  }

  void OnDownloadError(const std::string_view _name,
                       std::exception_ptr _error) noexcept override {
    //if (!complete && path_relative == _path_relative) {
    if (!complete && name == _name) {
      complete = true;
      success = false;
      error = std::move(_error);
      download_complete_notify.SendNotification();
    }
  }

  void OnDownloadCompleteNotification() noexcept {
    assert(complete);
    dialog.SetModalResult(success ? mrOK : mrCancel);
  }
};

/**
 * Throws on error.
 */
static AllocatedPath
DownloadFile(const char *uri, const char *base)
{
  assert(Net::DownloadManager::IsAvailable());

  if (!base)
    return nullptr;

  ProgressDialog dialog(UIGlobals::GetMainWindow(), UIGlobals::GetDialogLook(),
                        _("Download"));
  dialog.SetText(base);

  dialog.AddCancelButton();

  const DownloadProgress dp(dialog, base);

  Net::DownloadManager::Enqueue(uri, Path(base));

  int result = dialog.ShowModal();
  if (result != mrOK) {
    Net::DownloadManager::Cancel(base);
    dp.Rethrow();
    return nullptr;
  }

  return LocalPath(base);
}

class DownloadFilePickerWidget final
  : public ListWidget,
    Net::DownloadListener {

  WidgetDialog &dialog;

  UI::Notify download_complete_notify{[this]{ OnDownloadCompleteNotification(); }};

  const FileType file_type;

  unsigned font_height;

  Button *download_button;

  std::vector<AvailableFile> items;

  TextRowRenderer row_renderer;

  /**
   * This mutex protects the attribute "repository_modified".
   */
  mutable Mutex mutex;

  /**
   * Was the repository file modified, and needs to be reloaded by
   * RefreshList()?
   */
  bool repository_modified;

  /**
   * Has the repository file download failed?
   */
  bool repository_failed;

  std::exception_ptr repository_error;

  AllocatedPath path;

public:
  DownloadFilePickerWidget(WidgetDialog &_dialog, FileType _file_type)
    :dialog(_dialog), file_type(_file_type) {}

  AllocatedPath &&GetPath() {
    return std::move(path);
  }

  void CreateButtons();

protected:
  void RefreshList();

  void UpdateButtons() {
      download_button->SetEnabled(!items.empty());
  }

  void Download();
  void Cancel();

public:
  /* virtual methods from class Widget */
  void Prepare(ContainerWindow &parent, const PixelRect &rc) noexcept override;
  void Unprepare() noexcept override;

  /* virtual methods from class ListItemRenderer */
  void OnPaintItem(Canvas &canvas, const PixelRect rc,
                   unsigned idx) noexcept override;

  /* virtual methods from class ListCursorHandler */
  bool CanActivateItem([[maybe_unused]] unsigned index) const noexcept override {
    return true;
  }

  void OnActivateItem([[maybe_unused]] unsigned index) noexcept override {
    Download();
  }

  /* virtual methods from class Net::DownloadListener */
  // void OnDownloadAdded(Path path_relative,
  void OnDownloadAdded(const std::string_view name,
                       size_t size, size_t position) noexcept override;
  void OnDownloadComplete(const std::string_view name) noexcept override;
  void OnDownloadError(const std::string_view name,
                       std::exception_ptr error) noexcept override;

  void OnDownloadCompleteNotification() noexcept;
};

void
DownloadFilePickerWidget::Prepare(ContainerWindow &parent,
                                  const PixelRect &rc) noexcept
{
  const DialogLook &look = UIGlobals::GetDialogLook();

  CreateList(parent, look, rc,
             row_renderer.CalculateLayout(*look.list.font));
  RefreshList();

  Net::DownloadManager::AddListener(*this);
  Net::DownloadManager::Enumerate(*this);

  EnqueueRepositoryDownload();
}

void
DownloadFilePickerWidget::Unprepare() noexcept
{
  Net::DownloadManager::RemoveListener(*this);
}

void
DownloadFilePickerWidget::RefreshList()
try {
  {
    const std::lock_guard lock{mutex};
    repository_modified = false;
    repository_failed = false;
  }

  FileRepository repository;

  const auto path = LocalPath("repository");
  FileLineReaderA reader(path);

  ParseFileRepository(repository, reader);

  items.clear();
  for (auto &i : repository)
    if (i.type == file_type)
      items.emplace_back(std::move(i));

  ListControl &list = GetList();
  list.SetLength(items.size());
  list.Invalidate();

  UpdateButtons();
} catch (const std::runtime_error & /* e */) {
}

void
DownloadFilePickerWidget::CreateButtons()
{
  download_button = dialog.AddButton(_("Download"), [this](){ Download(); });

  UpdateButtons();
}

void
DownloadFilePickerWidget::OnPaintItem(Canvas &canvas, const PixelRect rc,
                                      unsigned i) noexcept
{
  const auto &file = items[i];

  row_renderer.DrawTextRow(canvas, rc, file.GetName());
}

void
DownloadFilePickerWidget::Download()
{
  assert(Net::DownloadManager::IsAvailable());

  const unsigned current = GetList().GetCursorIndex();
  assert(current < items.size());

  const auto &file = items[current];

  try {
    path = DownloadFile(file.GetURI(), file.GetName());
    if (path != nullptr)
      dialog.SetModalResult(mrOK);
  } catch (...) {
    ShowError(std::current_exception(), _("Error"));
  }
}

void
DownloadFilePickerWidget::OnDownloadAdded([[maybe_unused]] const std::string_view name,
                                          [[maybe_unused]] size_t size,
                                          [[maybe_unused]] size_t position) noexcept
{
}

void
DownloadFilePickerWidget::OnDownloadComplete(const std::string_view name) noexcept
{
//  const auto name = path_relative.GetBase();
  if (name.empty())
//  if (name == nullptr)
    return;

//  if (name == Path("repository")) {
  if (name == "repository") {
    const std::lock_guard lock{mutex};
    repository_failed = false;
    repository_modified = true;
  }

  download_complete_notify.SendNotification();
}

void
DownloadFilePickerWidget::OnDownloadError(const std::string_view name,
                                          std::exception_ptr error) noexcept
{
//  const auto name = path_relative.GetBase();
  if (name.empty())
    return;

  //if (name == Path("repository")) {
  if (name == "repository") {
    const std::lock_guard lock{mutex};
    repository_failed = true;
    repository_error = std::move(error);
  }

  download_complete_notify.SendNotification();
}

void
DownloadFilePickerWidget::OnDownloadCompleteNotification() noexcept
{
  bool repository_modified2, repository_failed2;
  std::exception_ptr repository_error2;

  {
    const std::lock_guard lock{mutex};
    repository_modified2 = std::exchange(repository_modified, false);
    repository_failed2 = std::exchange(repository_failed, false);
    repository_error2 = std::move(repository_error);
  }

  if (repository_error2)
    ShowError(std::move(repository_error2),
              _("Failed to download the repository index."));
  else if (repository_failed2)
    ShowMessageBox(_("Failed to download the repository index."),
                   _("Error"), MB_OK);
  else if (repository_modified2)
    RefreshList();
}

AllocatedPath
DownloadFilePicker(FileType file_type)
{
  if (!Net::DownloadManager::IsAvailable()) {
    const char *message =
      _("The file manager is not available on this device.");
    ShowMessageBox(message, _("File Manager"), MB_OK);
    return nullptr;
  }

  TWidgetDialog<DownloadFilePickerWidget>
    dialog(WidgetDialog::Full{}, UIGlobals::GetMainWindow(),
           UIGlobals::GetDialogLook(), _("Download"));
  dialog.AddButton(_("Cancel"), mrCancel);
  dialog.SetWidget(dialog, file_type);
  dialog.GetWidget().CreateButtons();
  dialog.ShowModal();

  return dialog.GetWidget().GetPath();
}
