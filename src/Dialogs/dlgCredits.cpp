// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#include "Dialogs/Dialogs.h"
#include "Dialogs/WidgetDialog.hpp"
#include "Widget/ArrowPagerWidget.hpp"
#include "Widget/RichTextWidget.hpp"
#include "Widget/VScrollWidget.hpp"
#include "Look/DialogLook.hpp"
#include "Version.hpp"
#include "ProductName.hpp"
#include "util/StringCompare.hxx"
#include "Inflate.hpp"
#include "util/AllocatedString.hxx"
#include "util/StaticString.hxx"
#include "UIGlobals.hpp"
#include "Language/Language.hpp"

#include <vector>

extern "C"
{
  extern const uint8_t COPYING_gz[];
  extern const size_t COPYING_gz_size;

  extern const uint8_t NEWS_txt_gz[];
  extern const size_t NEWS_txt_gz_size;

  extern const uint8_t AUTHORS_gz[];
  extern const size_t AUTHORS_gz_size;

  extern const uint8_t THIRD_PARTY_NOTICES_txt_gz[];
  extern const size_t THIRD_PARTY_NOTICES_txt_gz_size;

#ifdef HAVE_BRAND_NEWS
  /* OpenSoar-News.md, embedded like NEWS.txt (build/libdata.mk and
     Data/CMakeLists.txt; the symbol follows the file name) */
  extern const uint8_t OpenSoar_News_md_gz[];
  extern const size_t OpenSoar_News_md_gz_size;
#endif
}

/**
 * Build the logo/about page as markdown text.  Everything that names
 * the product comes from ProductName.hpp, so a rebranded build
 * (OpenSoar) gets its own name and web site here without touching
 * this file; a sponsor (SPONSOR_NAME/SPONSOR_URL from the brand
 * configuration, logo_sponsor.svg in the build) puts its logo next
 * to the product logo and its link under "Visit us at".  The page
 * is meant to fit a screen without scrolling.
 * @param dark_title use light/white title art on dark dialog backgrounds
 */
static const char *
GetLogoText(bool dark_title) noexcept
{
  static StaticString<1024> text;

  const char *const title_res =
    dark_title ? "IDB_TITLE_HD_WHITE" : "IDB_TITLE_HD";

  /* the logo - or, with a sponsor, both logos side by side */
  text.Format("![%s Logo](resource:IDB_LOGO_HD)", PRODUCT_NAME);
#if defined(SPONSOR_NAME) && defined(HAVE_SPONSOR_LOGO)
  text.AppendFormat(" ![%s](resource:IDB_LOGO_SPONSOR_HD)", SPONSOR_NAME);
#endif

  /* kept tight - version and git hash on one line, no empty lines
     between the short ones - so that the page fits a small screen */
  text.AppendFormat("\n\n![%s](resource:%s)\n\n**Version %s**",
                    PRODUCT_NAME, title_res, XCSoar_VersionString);
#ifdef GIT_COMMIT_ID
  text.AppendFormat(" (git %s)", GIT_COMMIT_ID);
#endif

  text.AppendFormat("\n\n%s\n", _("Visit us at:"));

  /* XCSoar first: that is where the manuals and the project live;
     a fork's own site follows */
  static constexpr const char *xcsoar_site = "https://xcsoar.org";
  const bool own_site =
    !StringStartsWith(PRODUCT_WEB_SITE_URL, xcsoar_site);
  text.AppendFormat("[%s](%s)\n",
                    own_site ? xcsoar_site : PRODUCT_WEB_SITE_URL,
                    own_site ? xcsoar_site : PRODUCT_WEB_SITE_URL);
  if (own_site)
    text.AppendFormat("[%s](%s)\n",
                      PRODUCT_WEB_SITE_URL, PRODUCT_WEB_SITE_URL);

#ifdef SPONSOR_URL
  text.AppendFormat("[%s](%s)\n", SPONSOR_URL, SPONSOR_URL);
#endif

  return text.c_str();
}

void
dlgCreditsShowModal([[maybe_unused]] UI::SingleWindow &parent)
{
  const DialogLook &look = UIGlobals::GetDialogLook();

  const auto authors = InflateToString(AUTHORS_gz, AUTHORS_gz_size);
  const auto news = InflateToString(NEWS_txt_gz, NEWS_txt_gz_size);
  const auto third_party = InflateToString(THIRD_PARTY_NOTICES_txt_gz,
                                           THIRD_PARTY_NOTICES_txt_gz_size);
  const auto license = InflateToString(COPYING_gz, COPYING_gz_size);
#ifdef HAVE_BRAND_NEWS
  const auto brand_news = InflateToString(OpenSoar_News_md_gz,
                                          OpenSoar_News_md_gz_size);
#endif

  WidgetDialog dialog(WidgetDialog::Full{}, UIGlobals::GetMainWindow(),
                      look, _("Credits"));

  auto pager = std::make_unique<ArrowPagerWidget>(look.button,
                                                  dialog.MakeModalResultCallback(mrOK));
  ArrowPagerWidget *const pager_ptr = pager.get();

  auto add_scroll_page = [&](std::unique_ptr<RichTextWidget> &&content) {
    pager->Add(std::make_unique<VScrollWidget>(std::move(content), look, true));
  };

  /* the pages, with their captions: About, [the brand's own news,]
     Authors, News, Third-party, License */
  std::vector<const char *> titles;

  add_scroll_page(std::make_unique<RichTextWidget>(
    look, GetLogoText(look.dark_mode)));
  titles.push_back(_("About"));

#ifdef HAVE_BRAND_NEWS
  /* what this brand adds on top of XCSoar, release by release -
     the XCSoar news follow on their own page */
  static StaticString<64> brand_news_title;
  brand_news_title.Format("%s %s", PRODUCT_NAME, _("News"));
  add_scroll_page(std::make_unique<RichTextWidget>(look, brand_news.c_str()));
  titles.push_back(brand_news_title.c_str());
#endif

  add_scroll_page(std::make_unique<RichTextWidget>(look, authors.c_str()));
  titles.push_back(_("Authors"));
  add_scroll_page(std::make_unique<RichTextWidget>(look, news.c_str(), false));
  titles.push_back(_("News"));
  add_scroll_page(std::make_unique<RichTextWidget>(look, third_party.c_str()));
  titles.push_back("Third-party");
  add_scroll_page(std::make_unique<RichTextWidget>(look, license.c_str(), false));
  titles.push_back(_("License"));

  const unsigned total_pages = pager->GetSize();

  auto update_caption = [&dialog, &titles, pager_ptr, total_pages]() {
    const unsigned current = pager_ptr->GetCurrentIndex();
    StaticString<128> caption;
    if (current < titles.size())
      caption.Format("%s (%u/%u)", titles[current], current + 1, total_pages);
    else
      caption = _("Credits");
    dialog.SetCaption(caption);
  };

  pager->SetPageFlippedCallback(update_caption);
  update_caption();

  dialog.FinishPreliminary(std::move(pager));
  dialog.ShowModal();
}
