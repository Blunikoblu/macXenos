/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * Copyright 2026 Xenia Canary. All rights reserved.                          *
 * Released under the BSD license - see LICENSE in the root for more details. *
 ******************************************************************************
 */

#include "xenia/kernel/util/object_table.h"

#include "third_party/catch/include/catch.hpp"
#include "xenia/base/cvar.h"

DECLARE_bool(strict_handle_lookup);

namespace xe {
namespace kernel {
namespace util {
namespace test {

namespace {

// Restores the cvar when a test case ends.
struct StrictLookup {
  explicit StrictLookup(bool value) : saved(cvars::strict_handle_lookup) {
    cvars::strict_handle_lookup = value;
  }
  ~StrictLookup() { cvars::strict_handle_lookup = saved; }
  bool saved;
};

constexpr X_STATUS kInvalidHandle = X_STATUS_INVALID_HANDLE;

}  // namespace

TEST_CASE("Handles resolve only by the value the kernel handed out",
          "[kernel][object_table]") {
  StrictLookup strict(true);
  ObjectTable table;
  // Fill slots 0..2 so the event below sits at slot 2 (handle F8000008),
  // like MvC2's notify listener.
  std::vector<object_ref<XObject>> filler;
  X_HANDLE handle = 0;
  for (int i = 0; i < 2; ++i) {
    filler.push_back(make_object<XObject>(XObject::Type::Semaphore));
    REQUIRE(table.AddHandle(filler.back().get(), &handle) == X_STATUS_SUCCESS);
  }
  auto event = make_object<XObject>(XObject::Type::Event);
  REQUIRE(table.AddHandle(event.get(), &handle) == X_STATUS_SUCCESS);
  REQUIRE(handle == 0xF8000008);

  // The handed-out value, and the same value with tag bits (the console
  // ignores the low two bits).
  CHECK(table.LookupObject<XObject>(0xF8000008).get() == event.get());
  CHECK(table.LookupObject<XObject>(0xF800000B).get() == event.get());

  // Values whose low bits match but whose high bits do not: an NTSTATUS
  // stored as a handle, a bare index, another table's high byte.
  for (X_HANDLE garbage :
       {0xC0000008u, 0x00000008u, 0x80000008u, 0xF9000008u, 0xFC000008u,
        0x01000008u, 0x40000008u, 0xF0000008u}) {
    INFO("value " << std::hex << garbage);
    CHECK_FALSE(table.LookupObject<XObject>(garbage));
    CHECK(table.RetainHandle(garbage) == kInvalidHandle);
    CHECK(table.ReleaseHandle(garbage) == kInvalidHandle);
    CHECK(table.RemoveHandle(garbage) == kInvalidHandle);
    X_HANDLE dup = 0;
    CHECK(table.DuplicateHandle(garbage, &dup) == kInvalidHandle);
  }
  // None of that touched the event's handle.
  REQUIRE(event->handles().size() == 1);
  CHECK(table.LookupObject<XObject>(0xF8000008).get() == event.get());

  // Close it: the closed value is invalid too (console: NtClose, wait and
  // ObReferenceObjectByHandle after close return STATUS_INVALID_HANDLE).
  REQUIRE(table.ReleaseHandle(0xF8000008) == X_STATUS_SUCCESS);
  CHECK(event->handles().empty());
  CHECK_FALSE(table.LookupObject<XObject>(0xF8000008));
  CHECK(table.ReleaseHandle(0xF8000008) == kInvalidHandle);
  CHECK(table.RetainHandle(0xF8000008) == kInvalidHandle);

  for (auto& object : filler) {
    REQUIRE(table.ReleaseHandle(object->handles()[0]) == X_STATUS_SUCCESS);
  }
}

TEST_CASE("Socket and host handles keep resolving", "[kernel][object_table]") {
  StrictLookup strict(true);
  ObjectTable table;
  X_HANDLE handle = 0;

  // Guest slot 0 holds an event (F8000000), slot 1 a socket: its handle is
  // the bare index (00000004), which titles use as a descriptor.
  auto event = make_object<XObject>(XObject::Type::Event);
  REQUIRE(table.AddHandle(event.get(), &handle) == X_STATUS_SUCCESS);
  REQUIRE(handle == 0xF8000000);
  auto socket = make_object<XObject>(XObject::Type::Socket);
  X_HANDLE socket_handle = 0;
  REQUIRE(table.AddHandle(socket.get(), &socket_handle) == X_STATUS_SUCCESS);
  REQUIRE(socket_handle == 0x00000004);
  CHECK(table.LookupObject<XObject>(socket_handle).get() == socket.get());
  // The socket's slot under the guest high byte is not its handle.
  CHECK_FALSE(table.LookupObject<XObject>(0xF8000004));

  // Host objects live in the host table at 0x01000000 + index (never 0).
  auto host = make_object<XObject>(XObject::Type::Thread);
  host->set_host_object(true);
  X_HANDLE host_handle = 0;
  REQUIRE(table.AddHandle(host.get(), &host_handle) == X_STATUS_SUCCESS);
  REQUIRE(host_handle > 0x01000000);
  REQUIRE((host_handle & 0xFF000000) == 0x01000000);
  CHECK(table.LookupObject<XObject>(host_handle).get() == host.get());
  CHECK_FALSE(table.LookupObject<XObject>(host_handle | 0x40000000));
  X_HANDLE dup = 0;
  REQUIRE(table.DuplicateHandle(host_handle, &dup) == X_STATUS_SUCCESS);
  CHECK(table.LookupObject<XObject>(dup).get() == host.get());

  // Pseudo-handles: -1 (current process) resolves to nothing here.
  CHECK_FALSE(table.LookupObject<XObject>(0xFFFFFFFF));

  REQUIRE(table.ReleaseHandle(dup) == X_STATUS_SUCCESS);
  REQUIRE(table.ReleaseHandle(host_handle) == X_STATUS_SUCCESS);
  REQUIRE(table.ReleaseHandle(socket_handle) == X_STATUS_SUCCESS);
  REQUIRE(table.ReleaseHandle(0xF8000000) == X_STATUS_SUCCESS);
}

TEST_CASE("strict_handle_lookup=false keeps the masking lookup",
          "[kernel][object_table]") {
  StrictLookup legacy(false);
  ObjectTable table;
  std::vector<object_ref<XObject>> objects;
  X_HANDLE handle = 0;
  for (int i = 0; i < 3; ++i) {
    objects.push_back(make_object<XObject>(XObject::Type::Event));
    REQUIRE(table.AddHandle(objects.back().get(), &handle) == X_STATUS_SUCCESS);
  }
  REQUIRE(handle == 0xF8000008);
  // The old behaviour: a bare index aliases the guest slot.
  CHECK(table.LookupObject<XObject>(0x00000008).get() == objects[2].get());
  // C0000008 was never resolved (it falls in the host range, past the table).
  CHECK_FALSE(table.LookupObject<XObject>(0xC0000008));
  for (auto& object : objects) {
    REQUIRE(table.ReleaseHandle(object->handles()[0]) == X_STATUS_SUCCESS);
  }
}

TEST_CASE("Canonical handle values", "[kernel][object_table]") {
  using T = XObject::Type;
  STATIC_REQUIRE(ObjectTable::CanonicalHandle(2, false, T::Event) ==
                 0xF8000008);
  STATIC_REQUIRE(ObjectTable::CanonicalHandle(2, false, T::Socket) ==
                 0x00000008);
  STATIC_REQUIRE(ObjectTable::CanonicalHandle(2, true, T::Thread) ==
                 0x01000008);
  STATIC_REQUIRE(ObjectTable::IsCanonicalHandle(0xF800000A, 2, false,
                                                T::Event));
  STATIC_REQUIRE_FALSE(ObjectTable::IsCanonicalHandle(0xC0000008, 2, false,
                                                      T::Event));
}

}  // namespace test
}  // namespace util
}  // namespace kernel
}  // namespace xe
