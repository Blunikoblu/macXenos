/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * Copyright 2026 Ben Vanik. All rights reserved.                             *
 * Released under the BSD license - see LICENSE in the root for more details. *
 ******************************************************************************
 */

#include <string>
#include <vector>

#include "third_party/catch/include/catch.hpp"
#include "xenia/app/recent_titles.h"

namespace xe {
namespace app {
namespace test {

namespace {

RecentTitleEntry Entry(const std::string& name, const std::string& path,
                       uint32_t title_id) {
  return {name, path, 0, title_id};
}

std::vector<std::string> Names(const std::vector<RecentTitleEntry>& titles) {
  std::vector<std::string> names;
  for (const auto& title : titles) {
    names.push_back(title.title_name + " @ " + title.path_to_file.string());
  }
  return names;
}

}  // namespace

TEST_CASE("a launched game goes to the front of the history",
          "[app][recent_titles]") {
  std::vector<RecentTitleEntry> titles = {Entry("Iron Man", "/g/iron.iso", 1),
                                          Entry("TMNT", "/g/tmnt.iso", 2)};
  RecordRecentTitle(&titles, Entry("Mortal Kombat", "/g/mk.iso", 3));
  REQUIRE(Names(titles) ==
          std::vector<std::string>{"Mortal Kombat @ /g/mk.iso",
                                   "Iron Man @ /g/iron.iso",
                                   "TMNT @ /g/tmnt.iso"});
}

TEST_CASE("playing a game again moves its entry, without a second one",
          "[app][recent_titles]") {
  std::vector<RecentTitleEntry> titles = {Entry("Iron Man", "/g/iron.iso", 1),
                                          Entry("TMNT", "/g/tmnt.iso", 2)};
  RecordRecentTitle(&titles, Entry("TMNT", "/g/tmnt.iso", 2));
  REQUIRE(Names(titles) == std::vector<std::string>{"TMNT @ /g/tmnt.iso",
                                                    "Iron Man @ /g/iron.iso"});
  // The same game from another folder replaces the old place.
  RecordRecentTitle(&titles, Entry("TMNT", "/other/tmnt.iso", 2));
  REQUIRE(Names(titles) ==
          std::vector<std::string>{"TMNT @ /other/tmnt.iso",
                                   "Iron Man @ /g/iron.iso"});
}

TEST_CASE("an entry recorded under another game's name is replaced when its "
          "file is played",
          "[app][recent_titles]") {
  // What 1.0 wrote after games were started one after another: Iron Man's
  // name and title ID on three files.
  std::vector<RecentTitleEntry> titles = {
      Entry("Iron Man", "/g/mk.iso", 1), Entry("Iron Man", "/g/tmnt.iso", 1),
      Entry("Iron Man", "/g/iron.iso", 1)};
  // TMNT played properly: its file's wrong entry goes, and the others stay
  // until their own files are played.
  RecordRecentTitle(&titles, Entry("TMNT", "/g/tmnt.iso", 2));
  REQUIRE(Names(titles) ==
          std::vector<std::string>{"TMNT @ /g/tmnt.iso",
                                   "Iron Man @ /g/mk.iso",
                                   "Iron Man @ /g/iron.iso"});
  RecordRecentTitle(&titles, Entry("Mortal Kombat", "/g/mk.iso", 3));
  REQUIRE(Names(titles) ==
          std::vector<std::string>{"Mortal Kombat @ /g/mk.iso",
                                   "TMNT @ /g/tmnt.iso",
                                   "Iron Man @ /g/iron.iso"});
  REQUIRE(titles[0].title_id == 3);
}

TEST_CASE("playing the game whose name was misused clears every entry under "
          "it",
          "[app][recent_titles]") {
  std::vector<RecentTitleEntry> titles = {
      Entry("Iron Man", "/g/mk.iso", 1), Entry("Iron Man", "/g/tmnt.iso", 1),
      Entry("Iron Man", "/g/iron.iso", 1)};
  RecordRecentTitle(&titles, Entry("Iron Man", "/g/iron.iso", 1));
  REQUIRE(Names(titles) ==
          std::vector<std::string>{"Iron Man @ /g/iron.iso"});
}

TEST_CASE("games with no name are kept apart by their files",
          "[app][recent_titles]") {
  std::vector<RecentTitleEntry> titles = {Entry("", "/g/a.xex", 0),
                                          Entry("", "/g/b.xex", 0)};
  RecordRecentTitle(&titles, Entry("", "/g/c.xex", 0));
  REQUIRE(titles.size() == 3);
  RecordRecentTitle(&titles, Entry("", "/g/a.xex", 0));
  REQUIRE(Names(titles) == std::vector<std::string>{" @ /g/a.xex",
                                                    " @ /g/c.xex",
                                                    " @ /g/b.xex"});
}

}  // namespace test
}  // namespace app
}  // namespace xe
