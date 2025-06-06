// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#include "FileManager.hpp"
#include "WidgetDialog.hpp"
#include "Message.hpp"
#include "UIGlobals.hpp"
#include "Look/DialogLook.hpp"
#include "Renderer/TextRowRenderer.hpp"
#include "Renderer/TwoTextRowsRenderer.hpp"
#include "Widget/ListWidget.hpp"
#include "Language/Language.hpp"
#include "LocalPath.hpp"
#include "system/FileUtil.hpp"
#include "system/Path.hpp"
#include "io/FileLineReader.hpp"
#include "Formatter/ByteSizeFormatter.hpp"
#include "Formatter/TimeFormatter.hpp"
#include "time/BrokenDateTime.hpp"
#include "net/http/Features.hpp"
#include "util/Macros.hpp"
#include "Repository/FileRepository.hpp"
#include "Repository/Parser.hpp"

#ifdef HAVE_DOWNLOAD_MANAGER
#include "Repository/Glue.hpp"
#include "ListPicker.hpp"
#include "Form/Button.hpp"
#include "net/http/DownloadManager.hpp"
#include "ui/event/Notify.hpp"
#include "thread/Mutex.hxx"
#include "ui/event/PeriodicTimer.hpp"

#include <map>
#include <set>
#include <vector>
#endif

#include <cassert>

static AllocatedPath
LocalPath(const AvailableFile &file)
{
  if (!file.GetName())
    return nullptr;

  return LocalPath(file.GetName());
}

#ifdef HAVE_DOWNLOAD_MANAGER

[[gnu::pure]]
static const AvailableFile *
FindRemoteFile(const FileRepository &repository, const char *name)
{
  return repository.FindByName(name);
}


[[gnu::pure]]
static bool
CanDownload(const FileRepository &repository, const char *name)
{
  return FindRemoteFile(repository, name) != nullptr;
}

static bool
UpdateAvailable(const FileRepository &repository, const char *name)
{
  const AvailableFile *remote_file = FindRemoteFile(repository, name);

  if (remote_file == nullptr)
    return false;

  BrokenDate remote_changed = remote_file->update_date;

  const auto path = LocalPath(name);
  BrokenDate local_changed = BrokenDateTime{File::GetLastModification(path)};

  return local_changed < remote_changed;
}
#endif

class ManagedFileListWidget
  : public ListWidget
#ifdef HAVE_DOWNLOAD_MANAGER
  , private Net::DownloadListener
#endif
{
  struct DownloadStatus {
    size_t size, position;
  };

  struct FileItem {
    StaticString<64u> name;
    StaticString<32u> size;
    StaticString<32u> last_modified;

    bool downloading, failed, out_of_date;

    DownloadStatus download_status;

    void Set(const char *_name, const DownloadStatus *_download_status,
             bool _failed, bool _out_of_date) {
      name = _name;

      const auto path = LocalPath(name);

      if (File::Exists(path)) {
        FormatByteSize(size.buffer(), size.capacity(),
                       File::GetSize(path));
        FormatISO8601(last_modified.buffer(),
                      BrokenDateTime{File::GetLastModification(path)});
      } else {
        size.clear();
        last_modified.clear();
      }

      downloading = _download_status != nullptr;
      if (downloading)
        download_status = *_download_status;

      failed = _failed;

      out_of_date = _out_of_date;
    }
  };

  TwoTextRowsRenderer row_renderer;

#ifdef HAVE_DOWNLOAD_MANAGER
  Button *download_button, *add_button, *cancel_button, *update_button, *delete_button;

  /**
  * Whether at least one file is out of date.
  * Used to activate "Update All" button.
  */
  bool some_out_of_date;
#endif

  FileRepository repository;

#ifdef HAVE_DOWNLOAD_MANAGER
  /**
   * This mutex protects the attributes "downloads" and
   * "repository_modified".
   */
  mutable Mutex mutex;

  /**
   * The list of file names (base names) that are currently being
   * downloaded.
   */
  std::map<std::string, DownloadStatus, std::less<>> downloads;

  /**
   * Each item in this set is a failed download.
   */
  std::set<std::string> failures;

  UI::PeriodicTimer refresh_download_timer{[this]{ OnTimer(); }};

  UI::Notify download_notify{[this]{ OnDownloadNotification(); }};

  /**
   * Was the repository file modified, and needs to be reloaded by
   * LoadRepositoryFile()?
   */
  bool repository_modified;

  /**
   * Has the repository file download failed?
   */
  bool repository_failed;
#endif

  TrivialArray<FileItem, 64u> items;

public:
  void CreateButtons(WidgetDialog &dialog) noexcept;

protected:
  [[gnu::pure]]
  bool IsDownloading(const char *name) const noexcept {
#ifdef HAVE_DOWNLOAD_MANAGER
    const std::lock_guard lock{mutex};
    return downloads.find(name) != downloads.end();
#else
    return false;
#endif
  }

  [[gnu::pure]]
  bool IsDownloading(const AvailableFile &file) const noexcept {
    return IsDownloading(file.GetName());
  }

  bool IsDownloading(const char *name,
                     DownloadStatus &status_r) const noexcept {
#ifdef HAVE_DOWNLOAD_MANAGER
    const std::lock_guard lock{mutex};
    auto i = downloads.find(name);
    if (i == downloads.end())
      return false;

    status_r = i->second;
    return true;
#else
    return false;
#endif
  }

  bool IsDownloading(const AvailableFile &file,
                     DownloadStatus &status_r) const noexcept {
    return IsDownloading(file.GetName(), status_r);
  }

  [[gnu::pure]]
  bool HasFailed(const char *name) const noexcept {
#ifdef HAVE_DOWNLOAD_MANAGER
    const std::lock_guard lock{mutex};
    return failures.find(name) != failures.end();
#else
    return false;
#endif
  }

  [[gnu::pure]]
  bool HasFailed(const AvailableFile &file) const noexcept {
    return HasFailed(file.GetName());
  }

  [[gnu::pure]]
  int FindItem(const char *name) const noexcept;

  void LoadRepositoryFile();
  void RefreshList();
  void UpdateButtons();

  void Download();
  void Delete();
  void Add();
  void Cancel();
  void UpdateFiles();

public:
  /* virtual methods from class Widget */
  void Prepare(ContainerWindow &parent, const PixelRect &rc) noexcept override;
  void Unprepare() noexcept override;

  /* virtual methods from class List::Handler */
  void OnPaintItem(Canvas &canvas, const PixelRect rc,
                   unsigned idx) noexcept override;
  void OnCursorMoved(unsigned index) noexcept override;

#ifdef HAVE_DOWNLOAD_MANAGER
  void OnTimer();

  /* virtual methods from class Net::DownloadListener */
  void OnDownloadAdded(const std::string_view uri,
                       size_t size, size_t position) noexcept override;
  void OnDownloadComplete(const std::string_view uri) noexcept override;
  void OnDownloadError(const std::string_view uri,
                       std::exception_ptr error) noexcept override;

  void OnDownloadNotification() noexcept;
#endif
};

void
ManagedFileListWidget::Prepare(ContainerWindow &parent,
                               const PixelRect &rc) noexcept
{
  const DialogLook &look = UIGlobals::GetDialogLook();

  CreateList(parent, look, rc,
             row_renderer.CalculateLayout(*look.list.font_bold,
                                          look.small_font));

  LoadRepositoryFile();
  RefreshList();
  UpdateButtons();

#ifdef HAVE_DOWNLOAD_MANAGER
  if (Net::DownloadManager::IsAvailable()) {
    Net::DownloadManager::AddListener(*this);
    Net::DownloadManager::Enumerate(*this);

    EnqueueRepositoryDownload();
  }
#endif
}

void
ManagedFileListWidget::Unprepare() noexcept
{
#ifdef HAVE_DOWNLOAD_MANAGER
  if (Net::DownloadManager::IsAvailable())
    Net::DownloadManager::RemoveListener(*this);
#endif
}

int
ManagedFileListWidget::FindItem(const char *name) const noexcept
{
  for (auto i = items.begin(), end = items.end(); i != end; ++i)
    if (StringIsEqual(i->name, name))
      return std::distance(items.begin(), i);

  return -1;
}

void
ManagedFileListWidget::LoadRepositoryFile()
try {
#ifdef HAVE_DOWNLOAD_MANAGER
  {
    const std::lock_guard lock{mutex};
    repository_modified = false;
    repository_failed = false;
  }
#endif

  repository.Clear();

  const auto path = LocalPath("repository");
  FileLineReaderA reader(path);
  ParseFileRepository(repository, reader);
} catch (const std::runtime_error & /* e */) {
}

void
ManagedFileListWidget::RefreshList()
{
  items.clear();

  some_out_of_date = false;

  bool download_active = false;
  for (auto i = repository.begin(), end = repository.end(); i != end; ++i) {
    const auto &remote_file = *i;
    DownloadStatus download_status;
    const bool is_downloading = IsDownloading(remote_file, download_status);

    const auto path = LocalPath(remote_file);
    const bool file_exists = File::Exists(path);

    if (path != nullptr &&
        (is_downloading || file_exists)) {
      download_active |= is_downloading;

      const Path base = path.GetBase();
      if (base == nullptr)
        continue;

      bool is_out_of_date = false;
      if (file_exists) {
        BrokenDate local_changed = BrokenDateTime{File::GetLastModification(path)};
        is_out_of_date = (local_changed < remote_file.update_date);

        if (is_out_of_date)
          some_out_of_date = true;
      }

      items.append().Set(base.c_str(),
                         is_downloading ? &download_status : nullptr,
                         HasFailed(remote_file), is_out_of_date);
    }
  }

  ListControl &list = GetList();
  list.SetLength(items.size());
  list.Invalidate();

#ifdef HAVE_DOWNLOAD_MANAGER
  if (download_active && !refresh_download_timer.IsActive())
    refresh_download_timer.Schedule(std::chrono::seconds(1));
#endif
}

void
ManagedFileListWidget::CreateButtons(WidgetDialog &dialog) noexcept
{
#ifdef HAVE_DOWNLOAD_MANAGER
  if (Net::DownloadManager::IsAvailable()) {
    // download_button = dialog.AddButton(_("Download"), [this](){ Download(); });
    download_button = dialog.AddButton(_("Update"), [this](){ Download(); });
    add_button = dialog.AddButton(_("Add"), [this](){ Add(); });
    delete_button = dialog.AddButton(_("Delete"), [this](){ Delete(); });
    cancel_button = dialog.AddButton(_("Cancel"), [this](){ Cancel(); });
    update_button = dialog.AddButton(_("Update all"), [this](){
      UpdateFiles();
    });
  }
#endif
}

void
ManagedFileListWidget::UpdateButtons()
{
#ifdef HAVE_DOWNLOAD_MANAGER
  if (Net::DownloadManager::IsAvailable()) {
    const unsigned current = GetList().GetCursorIndex();

    download_button->SetEnabled(!items.empty() &&
                                CanDownload(repository, items[current].name));
    cancel_button->SetEnabled(!items.empty() && items[current].downloading);
    update_button->SetEnabled(!items.empty() && some_out_of_date);
  }
#endif
}

void
ManagedFileListWidget::OnPaintItem(Canvas &canvas, const PixelRect rc,
                                   unsigned i) noexcept
{
  const FileItem &file = items[i];

  row_renderer.DrawFirstRow(canvas, rc, file.name.c_str());

  if (file.downloading) {
    StaticString<64> text;
    if (file.download_status.position < 0) {
      text = _("Queued");
    } else if (file.download_status.size > 0) {
      text.Format("%s (%u%%)", _("Downloading"),
                    unsigned(file.download_status.position * 100
                             / file.download_status.size));
    } else {
      char size[32];
      FormatByteSize(size, ARRAY_SIZE(size), file.download_status.position);
      text.Format("%s (%s)", _("Downloading"), size);
    }

    row_renderer.DrawRightFirstRow(canvas, rc, text);
  } else if (file.failed) {
    const char *text = _("Error");
    row_renderer.DrawRightFirstRow(canvas, rc, text);
  }

  row_renderer.DrawSecondRow(canvas, rc, file.size.c_str());

  if (file.out_of_date) {
    row_renderer.DrawRightSecondRow(canvas, rc, _("Update available"));
  } else {
    row_renderer.DrawRightSecondRow(canvas, rc, file.last_modified.c_str());
  }
}

void
ManagedFileListWidget::OnCursorMoved([[maybe_unused]] unsigned index) noexcept
{
  UpdateButtons();
}

void
ManagedFileListWidget::Delete()
{
#ifdef HAVE_DOWNLOAD_MANAGER
  assert(Net::DownloadManager::IsAvailable());

  if (items.empty())
    return;

  const unsigned current = GetList().GetCursorIndex();
  assert(current < items.size());

  auto path = LocalPath(items[current].name);
  // TODO(aug):
  // Please delete only w/o asking if the file in no profile inserted
  auto r = File::Delete(path);
  if (r)
    RefreshList();
  // else not deleted???
#endif
}

void
ManagedFileListWidget::Download()
{
#ifdef HAVE_DOWNLOAD_MANAGER
  assert(Net::DownloadManager::IsAvailable());

  if (items.empty())
    return;

  const unsigned current = GetList().GetCursorIndex();
  assert(current < items.size());

  const FileItem &item = items[current];
  const AvailableFile *remote_file_p = FindRemoteFile(repository, item.name);
  if (remote_file_p == nullptr)
    return;

  const AvailableFile &remote_file = *remote_file_p;
  if (!remote_file.GetName())
    return;

  Net::DownloadManager::Enqueue(remote_file.uri.c_str(),
                                Path(remote_file.GetName()));
#endif
}

#ifdef HAVE_DOWNLOAD_MANAGER

class AddFileListItemRenderer final : public ListItemRenderer {
  const std::vector<AvailableFile> &list;

  TwoTextRowsRenderer row_renderer;

public:
  explicit AddFileListItemRenderer(const std::vector<AvailableFile> &_list)
    :list(_list) {}

  unsigned CalculateLayout(const DialogLook &look) {
    return row_renderer.CalculateLayout(*look.list.font_bold,
                                          look.small_font);
  }

  void OnPaintItem(Canvas &canvas, const PixelRect rc, unsigned i) noexcept override;
};

void
AddFileListItemRenderer::OnPaintItem(Canvas &canvas, const PixelRect rc,
                                     unsigned i) noexcept
{
  assert(i < list.size());

  const AvailableFile &file = list[i];

  if (file.GetName())
    row_renderer.DrawFirstRow(canvas, rc, file.GetName());

  if (file.GetDescription())
    row_renderer.DrawSecondRow(canvas, rc, file.GetDescription());

  if (file.update_date.IsPlausible()) {
    char string_buffer[21];
    FormatISO8601(string_buffer, file.update_date);
    row_renderer.DrawRightSecondRow(canvas, rc, string_buffer);
  }
}

#endif

void
ManagedFileListWidget::Add()
{
#ifdef HAVE_DOWNLOAD_MANAGER
  assert(Net::DownloadManager::IsAvailable());

  std::vector<AvailableFile> list;
  for (const auto &remote_file : repository) {
    if (IsDownloading(remote_file.GetName()))
      /* already downloading this file */
      continue;

    if (!remote_file.GetName())
      continue;

    if (FindItem(remote_file.GetName()) < 0)
      list.push_back(remote_file);
  }

  if (list.empty())
    return;

  AddFileListItemRenderer item_renderer(list);
  int i = ListPicker(_("Select a file"),
                     list.size(), 0,
                     item_renderer.CalculateLayout(UIGlobals::GetDialogLook()),
                     item_renderer);
  if (i < 0)
    return;

  assert((unsigned)i < list.size());

  const AvailableFile &remote_file = list[i];
  if (!remote_file.GetName())
    return;

  Net::DownloadManager::Enqueue(remote_file.GetURI(),
                                Path(remote_file.GetName()));
#endif
}

void
ManagedFileListWidget::UpdateFiles() {
#ifdef HAVE_DOWNLOAD_MANAGER
  assert(Net::DownloadManager::IsAvailable());

  for (const auto &file : items) {
    if (UpdateAvailable(repository, file.name)) {
      const AvailableFile *remote_file = FindRemoteFile(repository, file.name);

      if (remote_file != nullptr) {
        if (!remote_file->GetName())
          return;

        Net::DownloadManager::Enqueue(remote_file->GetURI(),
                                      Path(remote_file->GetName()));
      }
    }
  }
#endif
}

void
ManagedFileListWidget::Cancel()
{
#ifdef HAVE_DOWNLOAD_MANAGER
  assert(Net::DownloadManager::IsAvailable());

  if (items.empty())
    return;

  const unsigned current = GetList().GetCursorIndex();
  assert(current < items.size());

  const FileItem &item = items[current];
  Net::DownloadManager::Cancel(item.name);
#endif
}

#ifdef HAVE_DOWNLOAD_MANAGER

void
ManagedFileListWidget::OnTimer()
{
  bool download_active;

  {
    const std::lock_guard lock{mutex};
    download_active = !downloads.empty();
  }

  if (download_active) {
    Net::DownloadManager::Enumerate(*this);
    RefreshList();
    UpdateButtons();
  } else
    refresh_download_timer.Cancel();
}

void
ManagedFileListWidget::OnDownloadAdded(const std::string_view name,
                                       size_t size, size_t position) noexcept
{
//  const auto name = path_relative.GetBase();
  if (name.empty())
    return;

  {
    const std::lock_guard lock{mutex};
    downloads[name.data()] = DownloadStatus{size, position};
    failures.erase(name.data());
  }

  download_notify.SendNotification();
}

void
ManagedFileListWidget::OnDownloadComplete(const std::string_view name) noexcept
{
  // const auto name = path_relative.GetBase();
  if (name.empty())
    return;

  {
    const std::lock_guard lock{mutex};

    downloads.erase(name.data());

    if (name == "repository") {
      repository_failed = false;
      repository_modified = true;
    }
  }

  download_notify.SendNotification();
}

void
ManagedFileListWidget::OnDownloadError(const std::string_view name,
                                       [[maybe_unused]] std::exception_ptr error) noexcept
{
  // const auto name = path_relative.GetBase();
  if (name == nullptr)
    return;


  {
    const std::lock_guard lock{mutex};

    downloads.erase(name.data());

    // TODO: store the error
    if (name == "repository") {
      repository_failed = true;
    } else
      failures.insert(name.data());
  }

  download_notify.SendNotification();
}

void
ManagedFileListWidget::OnDownloadNotification() noexcept
{
  bool repository_modified2, repository_failed2;

  {
    const std::lock_guard lock{mutex};
    repository_modified2 = std::exchange(repository_modified, false);
    repository_failed2 = std::exchange(repository_failed, false);
  }

  if (repository_modified2)
    LoadRepositoryFile();

  RefreshList();
  UpdateButtons();

  if (repository_failed2)
    ShowMessageBox(_("Failed to download the repository index."),
                   _("Error"), MB_OK);
}

static void
ShowFileManager2()
{
  TWidgetDialog<ManagedFileListWidget>
    dialog(WidgetDialog::Full{}, UIGlobals::GetMainWindow(),
           UIGlobals::GetDialogLook(),
           _("File Manager"));
  dialog.SetWidget();
  dialog.GetWidget().CreateButtons(dialog);
  dialog.AddButton(_("Close"), mrOK);

  dialog.EnableCursorSelection();

  dialog.ShowModal();
}

#endif

void
ShowFileManager()
{
#ifdef HAVE_DOWNLOAD_MANAGER
  if (Net::DownloadManager::IsAvailable()) {
    ShowFileManager2();
    return;
  }
#endif

  const char *message =
    _("The file manager is not available on this device.");

  ShowMessageBox(message, _("File Manager"), MB_OK);
}
