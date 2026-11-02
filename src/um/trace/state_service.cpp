#include "um/trace/state_service.hpp"

#include "core/platform/lock_marks.hpp"
#include "core/trace/function_scope.hpp"
#include "core/trace/state_reporter.hpp"
#include "core/trace/state_writer.hpp"
#include "um/trace/wide_name.hpp"
#include "um/trace/object_security.hpp"

#include <windows.h>

namespace {

constexpr const char *scopes[] = {"Global", "Local"};
constexpr size_t      length_bytes = 4u;

DWORD WINAPI state_service_thread(LPVOID service)
{
    AV_SCOPE("state_service_thread");
    static_cast<av::StateService *>(service)->serve();
    return 0u;
}

}

namespace av {

StateService::StateService()
    : reporter_(nullptr), dump_(nullptr), done_(nullptr), section_(nullptr), view_(nullptr), thread_(nullptr)
{
    AV_SCOPE("StateService::StateService");
}

Status StateService::start(const char *module, uint32_t pid, StateReporter &reporter, ObjectSecurity &security)
{
    AV_SCOPE("StateService::start");
    if (thread_ != nullptr) return Status::invalid_state;
    reporter_ = &reporter;
    bool made = false;
    for (const char *const scope : scopes) {
        made = make(scope, module, pid, security);
        if (made) break;
    }
    if (!made) return Status::no_memory;
    thread_ = CreateThread(nullptr, 0u, &state_service_thread, this, 0u, nullptr);
    if (thread_ != nullptr) return Status::ok;
    close();
    return Status::no_memory;
}

void StateService::serve()
{
    AV_SCOPE("StateService::serve");
    const LockMarks doorbell(dump_);
    for (;;) {
        doorbell.waiting();
        const DWORD woke = WaitForSingleObject(dump_, INFINITE);
        doorbell.woken();
        if (woke != WAIT_OBJECT_0) return;
        StateWriter writer(static_cast<char *>(static_cast<void *>(view_ + length_bytes)),
                           section_bytes - length_bytes);
        reporter_->report(writer);
        const uint32_t length = static_cast<uint32_t>(writer.length());
        for (size_t byte = 0u; byte < length_bytes; byte++) view_[byte] = static_cast<uint8_t>(length >> (8u * byte));
        static_cast<void>(SetEvent(done_));
    }
}

bool StateService::make(const char *scope, const char *module, uint32_t pid, ObjectSecurity &security)
{
    AV_SCOPE("StateService::make");
    const auto       attributes = static_cast<LPSECURITY_ATTRIBUTES>(security.attributes());
    const WideName dump(scope, "dump", module, pid);
    const WideName done(scope, "done", module, pid);
    const WideName state(scope, "state", module, pid);
    dump_ = CreateEventW(attributes, FALSE, FALSE, dump.text());
    done_ = CreateEventW(attributes, FALSE, FALSE, done.text());
    section_ = CreateFileMappingW(INVALID_HANDLE_VALUE, attributes, PAGE_READWRITE, 0u,
                                  static_cast<DWORD>(section_bytes), state.text());
    if (section_ != nullptr)
        view_ = static_cast<uint8_t *>(MapViewOfFile(section_, FILE_MAP_WRITE, 0u, 0u, section_bytes));
    if (dump_ != nullptr && done_ != nullptr && view_ != nullptr) return true;
    close();
    return false;
}

void StateService::close()
{
    AV_SCOPE("StateService::close");
    if (view_ != nullptr) static_cast<void>(UnmapViewOfFile(view_));
    void *const handles[] = {section_, done_, dump_};
    for (void *const handle : handles)
        if (handle != nullptr) static_cast<void>(CloseHandle(handle));
    view_ = nullptr;
    section_ = nullptr;
    done_ = nullptr;
    dump_ = nullptr;
}

}
