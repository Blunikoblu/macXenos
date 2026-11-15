/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * Copyright 2026 Ben Vanik. All rights reserved.                             *
 * Released under the BSD license - see LICENSE in the root for more details. *
 ******************************************************************************
 */

#ifndef XENIA_APP_RECENT_TITLES_H_
#define XENIA_APP_RECENT_TITLES_H_

#include <algorithm>
#include <cstdint>
#include <ctime>
#include <filesystem>
#include <string>
#include <utility>
#include <vector>

namespace xe {
namespace app {

struct RecentTitleEntry {
  std::string title_name;
  std::filesystem::path path_to_file;
  std::time_t last_run_time;
  // 0 for entries recorded before title IDs were kept.
  uint32_t title_id = 0;
};

// Puts a game just launched at the front of the play history, in place of
// what the history held for it: every older entry with the same name (the
// game from another folder) and every older entry for the same file.
//
// The second matters for histories written by 1.0, where a game started after
// another was stopped was recorded under the stopped game's name and title
// ID. Matching on the name alone left such an entry in place for ever, one
// name on several files; matching on the file too replaces it the next time
// that file is played.
inline void RecordRecentTitle(std::vector<RecentTitleEntry>* titles,
                              RecentTitleEntry launched) {
  titles->erase(
      std::remove_if(titles->begin(), titles->end(),
                     [&launched](const RecentTitleEntry& entry) {
                       // Games with no name are told apart by file only.
                       return (!launched.title_name.empty() &&
                               entry.title_name == launched.title_name) ||
                              entry.path_to_file == launched.path_to_file;
                     }),
      titles->end());
  titles->insert(titles->begin(), std::move(launched));
}

}  // namespace app
}  // namespace xe

#endif  // XENIA_APP_RECENT_TITLES_H_
