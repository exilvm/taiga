/*
** Taiga, a lightweight client for MyAnimeList
** Copyright (C) 2010-2012, Eren Okka
** 
** This program is free software: you can redistribute it and/or modify
** it under the terms of the GNU General Public License as published by
** the Free Software Foundation, either version 3 of the License, or
** (at your option) any later version.
** 
** This program is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
** GNU General Public License for more details.
** 
** You should have received a copy of the GNU General Public License
** along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "../std.h"

#include "dlg_settings.h"
#include "dlg_torrent.h"

#include "../anime_db.h"
#include "../common.h"
#include "../feed.h"
#include "../gfx.h"
#include "../http.h"
#include "../resource.h"
#include "../settings.h"
#include "../string.h"
#include "../taiga.h"
#include "../theme.h"

#include "../win32/win_gdi.h"

class TorrentDialog TorrentDialog;

// =============================================================================

BOOL TorrentDialog::OnInitDialog() {
  // Set properties
  SetSizeMin(470, 260);

  // Create list
  list_.Attach(GetDlgItem(IDC_LIST_TORRENT));
  list_.EnableGroupView(true);
  list_.SetExtendedStyle(LVS_EX_CHECKBOXES | LVS_EX_DOUBLEBUFFER | 
    LVS_EX_FULLROWSELECT | LVS_EX_INFOTIP | LVS_EX_LABELTIP);
  list_.SetImageList(UI.ImgList16.GetHandle());
  list_.SetTheme();
  
  // Insert list columns
  list_.InsertColumn(0, 240, 240, LVCFMT_LEFT,  L"Anime title");
  list_.InsertColumn(1,  60,  60, LVCFMT_RIGHT, L"Episode");
  list_.InsertColumn(2, 120, 120, LVCFMT_LEFT,  L"Group");
  list_.InsertColumn(3,  70,  70, LVCFMT_RIGHT, L"Size");
  list_.InsertColumn(4, 100, 100, LVCFMT_LEFT,  L"Video");
  list_.InsertColumn(5, 250, 250, LVCFMT_LEFT,  L"Description");
  list_.InsertColumn(6, 250, 250, LVCFMT_LEFT,  L"File name");
  // Insert list groups
  list_.InsertGroup(0, L"Anime");
  list_.InsertGroup(1, L"Batch");
  list_.InsertGroup(2, L"Other");

  // Create main toolbar
  toolbar_.Attach(GetDlgItem(IDC_TOOLBAR_TORRENT));
  toolbar_.SetImageList(UI.ImgList16.GetHandle(), 16, 16);
  toolbar_.SendMessage(TB_SETEXTENDEDSTYLE, 0, TBSTYLE_EX_DRAWDDARROWS | TBSTYLE_EX_MIXEDBUTTONS);
  // Insert toolbar buttons
  BYTE fsStyle = BTNS_AUTOSIZE | BTNS_SHOWTEXT;
  toolbar_.InsertButton(0, ICON16_REFRESH,  100, 1, fsStyle, 0, L"Check new torrents", NULL);
  toolbar_.InsertButton(1, 0, 0, 0, BTNS_SEP, NULL, NULL, NULL);
  toolbar_.InsertButton(2, ICON16_DOWNLOAD, 101, 1, fsStyle, 0, L"Download marked torrents", NULL);
  toolbar_.InsertButton(3, ICON16_CROSS,    102, 1, fsStyle, 0, L"Discard all", NULL);
  toolbar_.InsertButton(4, 0, 0, 0, BTNS_SEP, NULL, NULL, NULL);
  toolbar_.InsertButton(5, ICON16_SETTINGS, 103, 1, fsStyle, 0, L"Settings", NULL);

  // Create rebar
  rebar_.Attach(GetDlgItem(IDC_REBAR_TORRENT));
  // Insert rebar bands
  rebar_.InsertBand(NULL, 0, 0, 0, 0, 0, 0, 0, 0, 
    RBBIM_CHILD | RBBIM_CHILDSIZE | RBBIM_SIZE | RBBIM_STYLE, RBBS_NOGRIPPER);
  rebar_.InsertBand(toolbar_.GetWindowHandle(), GetSystemMetrics(SM_CXSCREEN), 0, 0, 0, 0, 0, 0, 
    HIWORD(toolbar_.GetButtonSize()) + (HIWORD(toolbar_.GetPadding()) / 2), 
    RBBIM_CHILD | RBBIM_CHILDSIZE | RBBIM_SIZE | RBBIM_STYLE, RBBS_NOGRIPPER);

  // Refresh list
  RefreshList();

  return TRUE;
}

// =============================================================================

BOOL TorrentDialog::OnCommand(WPARAM wParam, LPARAM lParam) {
  Feed* feed = Aggregator.Get(FEED_CATEGORY_LINK);
  if (!feed) return 0;
  
  // Toolbar
  switch (LOWORD(wParam)) {
    // Check new torrents
    case 100: {
      feed->Check(Settings.RSS.Torrent.source);
      /**
      #ifdef _DEBUG
      feed->Load();
      feed->ExamineData();
      RefreshList();
      #endif
      /**/
      return TRUE;
    }
    // Download marked torrents
    case 101: {
      for (int i = 0; i < list_.GetItemCount(); i++) {
        FeedItem* feed_item = reinterpret_cast<FeedItem*>(list_.GetItemParam(i));
        if (feed_item) {
          bool check_state = list_.GetCheckState(i) == TRUE;
          if (feed_item->download && !check_state) {
            // Discard items that have passed all filters but are unchecked by the user
            Aggregator.file_archive.push_back(feed_item->title);
          }
          feed_item->download = check_state;
        }
      }
      feed->Download(-1);
      return TRUE;
    }
    // Discard marked torrents
    case 102: {
      for (int i = 0; i < list_.GetItemCount(); i++) {
        if (list_.GetCheckState(i) == TRUE) {
          FeedItem* feed_item = reinterpret_cast<FeedItem*>(list_.GetItemParam(i));
          if (feed_item) {
            feed_item->download = false;
            list_.SetCheckState(i, FALSE);
            Aggregator.file_archive.push_back(feed_item->title);
          }
        }
      }
      return TRUE;
    }
    // Settings
    case 103: {
      ExecuteAction(L"Settings", 0, PAGE_TORRENT1);
      return TRUE;
    }
  }

  return FALSE;
}

LRESULT TorrentDialog::OnNotify(int idCtrl, LPNMHDR pnmh) {
  Feed* feed = Aggregator.Get(FEED_CATEGORY_LINK);
  if (!feed) return 0;

  // ListView control
  if (idCtrl == IDC_LIST_TORRENT) {
    switch (pnmh->code) {
      // Column click
      case LVN_COLUMNCLICK: {
        LPNMLISTVIEW lplv = (LPNMLISTVIEW)pnmh;
        int order = 1;
        if (lplv->iSubItem == list_.GetSortColumn()) order = list_.GetSortOrder() * -1;
        switch (lplv->iSubItem) {
          // Episode
          case 1:
            list_.Sort(lplv->iSubItem, order, LIST_SORTTYPE_NUMBER, ListViewCompareProc);
            break;
          // File size
          case 3:
            list_.Sort(lplv->iSubItem, order, LIST_SORTTYPE_FILESIZE, ListViewCompareProc);
            break;
          // Other columns
          default:
            list_.Sort(lplv->iSubItem, order, LIST_SORTTYPE_DEFAULT, ListViewCompareProc);
            break;
        }
        break;
      }

      // Double click
      case NM_DBLCLK: {
        if (list_.GetSelectedCount() > 0) {
          LPNMITEMACTIVATE lpnmitem = reinterpret_cast<LPNMITEMACTIVATE>(pnmh);
          if (lpnmitem->iItem == -1) break;
          FeedItem* feed_item = reinterpret_cast<FeedItem*>(list_.GetItemParam(lpnmitem->iItem));
          if (feed_item) {
            feed->Download(feed_item->index);
          }
        }
        break;
      }

      // Right click
      case NM_RCLICK: {
        LPNMITEMACTIVATE lpnmitem = reinterpret_cast<LPNMITEMACTIVATE>(pnmh);
        if (lpnmitem->iItem == -1) break;
        FeedItem* feed_item = reinterpret_cast<FeedItem*>(list_.GetItemParam(lpnmitem->iItem));
        if (feed_item) {
          wstring answer = UI.Menus.Show(m_hWindow, 0, 0, L"TorrentListRightClick");
          if (answer == L"DownloadTorrent") {
            feed->Download(feed_item->index);
          } else if (answer == L"DiscardTorrent") {
            feed_item->download = false;
            list_.SetCheckState(lpnmitem->iItem, FALSE);
            Aggregator.file_archive.push_back(feed_item->title);
          } else if (answer == L"DiscardTorrents") {
            auto anime_item = AnimeDatabase.FindItem(feed_item->episode_data.anime_id);
            if (anime_item) {
              for (int i = 0; i < list_.GetItemCount(); i++) {
                feed_item = reinterpret_cast<FeedItem*>(list_.GetItemParam(i));
                if (feed_item && feed_item->episode_data.anime_id == anime_item->GetId()) {
                  feed_item->download = false;
                  list_.SetCheckState(i, FALSE);
                }
              }
              Aggregator.filter_manager.AddFilter(
                FEED_FILTER_ACTION_DISCARD, FEED_FILTER_MATCH_ALL, true, 
                L"Discard \"" + anime_item->GetTitle() + L"\"");
              Aggregator.filter_manager.filters.back().AddCondition(
                FEED_FILTER_ELEMENT_ANIME_ID, FEED_FILTER_OPERATOR_IS, 
                ToWstr(anime_item->GetId()));
            }
          } else if (answer == L"MoreTorrents") {
            feed->Check(ReplaceVariables(
              L"http://www.nyaa.eu/?page=rss&cats=1_37&filter=2&term=%title%", // TEMP
              feed_item->episode_data));
          } else if (answer == L"SearchMAL") {
            ExecuteAction(L"SearchAnime(" + feed_item->episode_data.title + L")");
          }
        }
        break;
      }

      // Custom draw
      case NM_CUSTOMDRAW: {
        LPNMLVCUSTOMDRAW pCD = reinterpret_cast<LPNMLVCUSTOMDRAW>(pnmh);
        switch (pCD->nmcd.dwDrawStage) {
          case CDDS_PREPAINT:
            return CDRF_NOTIFYITEMDRAW;
          case CDDS_ITEMPREPAINT:
            return CDRF_NOTIFYSUBITEMDRAW;
          case CDDS_PREERASE:
          case CDDS_ITEMPREERASE:
            return CDRF_NOTIFYPOSTERASE;

          case CDDS_ITEMPREPAINT | CDDS_SUBITEM: {
            // Alternate background color
            if ((pCD->nmcd.dwItemSpec % 2) && !list_.IsGroupViewEnabled()) {
              pCD->clrTextBk = RGB(248, 248, 248);
            }
            // Change text color
            FeedItem* feed_item = reinterpret_cast<FeedItem*>(pCD->nmcd.lItemlParam);
            if (feed_item) {
              if (feed_item->episode_data.anime_id < 1) {
                pCD->clrText = RGB(180, 180, 180);
              } else if (feed_item->episode_data.new_episode) {
                pCD->clrText = GetSysColor(pCD->iSubItem == 1 ? COLOR_HIGHLIGHT : COLOR_WINDOWTEXT);
              }
            }
            return CDRF_NOTIFYPOSTPAINT;
          }
        }
      }
    }
  }

  return 0;
}

void TorrentDialog::OnSize(UINT uMsg, UINT nType, SIZE size) {
  switch (uMsg) {
    case WM_SIZE: {
      win32::Rect rcWindow;
      rcWindow.Set(0, 0, size.cx, size.cy);
      // Resize rebar
      rebar_.SendMessage(WM_SIZE, 0, 0);
      rcWindow.top += rebar_.GetBarHeight() + ScaleY(WIN_CONTROL_MARGIN / 2);
      // Resize list
      list_.SetPosition(NULL, rcWindow);
    }
  }
}

// =============================================================================

void TorrentDialog::EnableInput(bool enable) {
  // Enable/disable toolbar buttons
  toolbar_.EnableButton(0, enable);
  toolbar_.EnableButton(2, enable);
  // Enable/disable list
  list_.Enable(enable);
}

void TorrentDialog::RefreshList() {
  if (!IsWindow()) return;
  Feed* feed = Aggregator.Get(FEED_CATEGORY_LINK);
  if (!feed) return;
  
  // Hide list to avoid visual defects and gain performance
  list_.Hide();
  list_.DeleteAllItems();

  // Add items
  for (auto it = feed->items.begin(); it != feed->items.end(); ++it) {
    if (Settings.RSS.Torrent.hide_unidentified && it->episode_data.anime_id == anime::ID_NOTINLIST) {
      continue;
    }
    wstring title, number, video;
    int group = TORRENT_ANIME, icon = StatusToIcon(0);
    if (it->category == L"Batch" || 
      !IsNumeric(it->episode_data.number)) {
        group = TORRENT_BATCH;
    }
    auto anime_item = AnimeDatabase.FindItem(it->episode_data.anime_id);
    if (anime_item) {
      icon = StatusToIcon(anime_item->GetAiringStatus());
      title = anime_item->GetTitle();
    } else if (!it->episode_data.title.empty()) {
      title = it->episode_data.title;
    } else {
      group = TORRENT_OTHER;
      title = it->title;
    }
    vector<int> numbers;
    SplitEpisodeNumbers(it->episode_data.number, numbers);
    number = JoinEpisodeNumbers(numbers);
    if (!it->episode_data.version.empty()) {
      number += L"v" + it->episode_data.version;
    }
    video = it->episode_data.video_type;
    if (!it->episode_data.resolution.empty()) {
      if (!video.empty()) video += L" ";
      video += it->episode_data.resolution;
    }
    int index = list_.InsertItem(it - feed->items.begin(), 
      group, icon, 0, NULL, title.c_str(), reinterpret_cast<LPARAM>(&(*it)));
    list_.SetItem(index, 1, number.c_str());
    list_.SetItem(index, 2, it->episode_data.group.c_str());
    list_.SetItem(index, 3, it->episode_data.file_size.c_str());
    list_.SetItem(index, 4, video.c_str());
    list_.SetItem(index, 5, it->description.c_str());
    list_.SetItem(index, 6, it->episode_data.file.c_str());
    list_.SetCheckState(index, it->download);
  }

  // Show again
  list_.Show();

  // Set icon
  HICON hIcon = feed->GetIcon();
  if (hIcon) hIcon = CopyIcon(hIcon);
  SetIconSmall(hIcon);

  // Set title
  wstring title = L"Torrents";
  if (!feed->title.empty()) {
    title = feed->title;
  } else if (!feed->link.empty()) {
    win32::Url url(feed->link);
    title += L" (" + url.Host + L")";
  }
  if (!feed->description.empty()) {
    title += L" - " + feed->description;
  }
  SetText(title.c_str());
}

void TorrentDialog::SetTimerText(const wstring& text) {
  toolbar_.SetButtonText(0, text.c_str());
}