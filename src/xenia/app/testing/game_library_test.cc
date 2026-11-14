/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * Copyright 2026 Ben Vanik. All rights reserved.                             *
 * Released under the BSD license - see LICENSE in the root for more details. *
 ******************************************************************************
 */

#include <unistd.h>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <thread>
#include <vector>

#include "third_party/catch/include/catch.hpp"
#include "third_party/zarchive/include/zarchive/zarchivewriter.h"
#include "xenia/app/game_library.h"
#include "xenia/base/memory.h"
#include "xenia/kernel/util/xex2_info.h"

namespace xe {
namespace app {
namespace test {

namespace fs = std::filesystem;

namespace {

// A fresh folder per test case, removed afterwards.
struct Scratch {
  fs::path root;
  Scratch() {
    static int counter = 0;
    root = fs::temp_directory_path() /
           ("xe_game_library_test_" + std::to_string(getpid()) + "_" +
            std::to_string(counter++));
    fs::remove_all(root);
    fs::create_directories(root / "storage");
    fs::create_directories(root / "games");
  }
  ~Scratch() {
    std::error_code ec;
    fs::remove_all(root, ec);
  }
};

// The start of an XEX2 file, with only what the library reads from it: the
// optional-header table and the execution info that holds the title ID.
std::vector<uint8_t> XexHeader(uint32_t title_id) {
  const size_t info_offset = 0x20;
  std::vector<uint8_t> data(
      info_offset + sizeof(xe::xex2_opt_execution_info), 0);
  data[0] = 'X';
  data[1] = 'E';
  data[2] = 'X';
  data[3] = '2';
  xe::store_and_swap<uint32_t>(data.data() + 0x14, 1);  // optional headers
  xe::store_and_swap<uint32_t>(data.data() + 0x18,
                               xe::XEX_HEADER_EXECUTION_INFO);
  xe::store_and_swap<uint32_t>(data.data() + 0x1C, uint32_t(info_offset));
  xe::store_and_swap<uint32_t>(
      data.data() + info_offset + offsetof(xe::xex2_opt_execution_info,
                                           title_id),
      title_id);
  return data;
}

// A .zar holding `files` (path in the archive, contents).
void WriteZar(
    const fs::path& path,
    const std::vector<std::pair<std::string, std::vector<uint8_t>>>& files) {
  std::ofstream out(path, std::ios::binary);
  REQUIRE(out.is_open());
  ZArchiveWriter writer(
      [](int32_t, void*) {},
      [](const void* data, size_t length, void* ctx) {
        static_cast<std::ofstream*>(ctx)->write(
            static_cast<const char*>(data), std::streamsize(length));
      },
      &out);
  for (const auto& [name, contents] : files) {
    const size_t slash = name.rfind('/');
    if (slash != std::string::npos) {
      REQUIRE(writer.MakeDir(name.substr(0, slash).c_str(), true));
    }
    REQUIRE(writer.StartNewFile(name.c_str()));
    writer.AppendData(contents.data(), contents.size());
  }
  writer.Finalize();
}

// Scans `scratch`'s games folder and returns what the library lists.
std::vector<GameLibrary::Game> ScanGames(const Scratch& scratch) {
  GameLibrary library(scratch.root / "storage", nullptr);
  library.AddFolder(scratch.root / "games");
  const auto deadline =
      std::chrono::steady_clock::now() + std::chrono::seconds(30);
  while (library.scanning() && std::chrono::steady_clock::now() < deadline) {
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
  }
  REQUIRE_FALSE(library.scanning());
  return library.games();
}

}  // namespace

TEST_CASE("a .zar disc is listed, with its title ID", "[app][game_library]") {
  Scratch scratch;
  WriteZar(scratch.root / "games" / "Some Game (USA) (En,Fr).zar",
           {{"default.xex", XexHeader(0x4D5307E6)},
            {"media/readme.txt", {'h', 'i'}}});

  const auto games = ScanGames(scratch);
  REQUIRE(games.size() == 1);
  REQUIRE(games[0].path ==
          scratch.root / "games" / "Some Game (USA) (En,Fr).zar");
  REQUIRE(games[0].title_id == 0x4D5307E6);
  REQUIRE(games[0].name == "Some Game");
  REQUIRE(games[0].disc == 0);
}

TEST_CASE("a .zar in a subfolder and with an upper-case extension is listed",
          "[app][game_library]") {
  Scratch scratch;
  fs::create_directories(scratch.root / "games" / "Racing");
  WriteZar(scratch.root / "games" / "Racing" / "Other Game (Disc 2).ZAR",
           {{"default.xex", XexHeader(0x545407F2)}});

  const auto games = ScanGames(scratch);
  REQUIRE(games.size() == 1);
  REQUIRE(games[0].title_id == 0x545407F2);
  REQUIRE(games[0].name == "Other Game");
  REQUIRE(games[0].disc == 2);
}

TEST_CASE("a .zar whose title ID cannot be read is still listed by name",
          "[app][game_library]") {
  Scratch scratch;
  // No default.xex in it.
  WriteZar(scratch.root / "games" / "Odd Disc.zar",
           {{"other.bin", {1, 2, 3, 4}}});
  // Not an archive at all.
  {
    std::ofstream out(scratch.root / "games" / "Broken.zar", std::ios::binary);
    out << "not a zarchive";
  }

  auto games = ScanGames(scratch);
  REQUIRE(games.size() == 2);
  for (const auto& game : games) {
    REQUIRE(game.title_id == 0);
    REQUIRE((game.name == "Odd Disc" || game.name == "Broken"));
  }
}

TEST_CASE("a second scan lists a .zar from the index", "[app][game_library]") {
  Scratch scratch;
  WriteZar(scratch.root / "games" / "Some Game.zar",
           {{"default.xex", XexHeader(0x415607D1)}});

  REQUIRE(ScanGames(scratch).size() == 1);
  // A new library over the same storage: the folder list and the index are
  // read back, and the unchanged file is not opened again.
  GameLibrary library(scratch.root / "storage", nullptr);
  REQUIRE(library.folders().size() == 1);
  library.Rescan();
  const auto deadline =
      std::chrono::steady_clock::now() + std::chrono::seconds(30);
  while (library.scanning() && std::chrono::steady_clock::now() < deadline) {
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
  }
  const auto games = library.games();
  REQUIRE(games.size() == 1);
  REQUIRE(games[0].title_id == 0x415607D1);
  REQUIRE(games[0].name == "Some Game");
}

}  // namespace test
}  // namespace app
}  // namespace xe
