/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * Copyright 2022 Ben Vanik. All rights reserved.                             *
 * Released under the BSD license - see LICENSE in the root for more details. *
 ******************************************************************************
 */

#include "xenia/kernel/xmutant.h"

#include "xenia/base/byte_stream.h"
#include "xenia/base/logging.h"
#include "xenia/kernel/kernel_state.h"
#include "xenia/kernel/xthread.h"

namespace xe {
namespace kernel {

XMutant::XMutant(KernelState* kernel_state)
    : XObject(kernel_state, kObjectType) {}

XMutant::XMutant() : XObject(kObjectType) {}

XMutant::~XMutant() = default;

void XMutant::Initialize(bool initial_owner) {
  assert_false(mutant_);

  mutant_ = xe::threading::Mutant::Create(initial_owner);
  assert_not_null(mutant_);
  ownership_depth_ = initial_owner ? 1 : 0;
  owning_thread_ = initial_owner ? XThread::GetCurrentThread() : nullptr;
}

void XMutant::InitializeNative(void* native_ptr, X_DISPATCH_HEADER* header) {
  assert_false(mutant_);

  guest_header_ptr_ = memory()->HostToGuestVirtual(native_ptr);
  // KeInitializeMutant materializes an owned mutant on its owning thread.
  Initialize(header->signal_state == 0);
  SetNativePointer(guest_header_ptr_, true);
  WriteGuestState();
}

X_STATUS XMutant::ReleaseMutant(uint32_t priority_increment, bool abandon,
                               bool wait, int32_t* previous_state) {
  std::lock_guard<std::mutex> lock(state_mutex_);
  set_priority_increment(priority_increment);

  // TODO(benvanik): abandoning.
  assert_false(abandon);
  if (mutant_->Release()) {
    if (previous_state) {
      *previous_state = static_cast<int32_t>(1u - ownership_depth_);
    }
    if (ownership_depth_) {
      --ownership_depth_;
    }
    if (!ownership_depth_) {
      owning_thread_ = nullptr;
    }
    WriteGuestState();
    return X_STATUS_SUCCESS;
  } else {
    return X_STATUS_MUTANT_NOT_OWNED;
  }
}

bool XMutant::Save(ByteStream* stream) {
  if (!SaveObject(stream)) {
    return false;
  }

  XThread* owner = owning_thread_.load();
  uint32_t owning_thread_handle = owner ? owner->handle() : 0;
  stream->Write<uint32_t>(owning_thread_handle);
  XELOGD("XMutant {:08X} (owner: {:08X})", handle(), owning_thread_handle);

  return true;
}

object_ref<XMutant> XMutant::Restore(KernelState* kernel_state,
                                     ByteStream* stream) {
  auto mutant = new XMutant();
  mutant->kernel_state_ = kernel_state;

  if (!mutant->RestoreObject(stream)) {
    delete mutant;
    return nullptr;
  }

  mutant->Initialize(false);

  auto owning_thread_handle = stream->Read<uint32_t>();
  if (owning_thread_handle) {
    mutant->owning_thread_ = kernel_state->object_table()
                                 ->LookupObject<XThread>(owning_thread_handle)
                                 .get();
    mutant->owning_thread_.load()->AcquireMutantOnStartup(
        retain_object(mutant));
  }

  return object_ref<XMutant>(mutant);
}

void XMutant::WaitCallback() {
  std::lock_guard<std::mutex> lock(state_mutex_);
  owning_thread_ = XThread::GetCurrentThread();
  ++ownership_depth_;
  WriteGuestState();
}

void XMutant::WriteGuestState() {
  if (!guest_header_ptr_) {
    return;
  }
  auto mutant = memory()->TranslateVirtual<X_KMUTANT*>(guest_header_ptr_);
  mutant->header.signal_state = 1u - ownership_depth_;
  auto owner = owning_thread_.load();
  mutant->owner = owner ? owner->guest_object() : 0;
}

}  // namespace kernel
}  // namespace xe
