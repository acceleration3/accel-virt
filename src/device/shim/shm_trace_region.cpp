#include "device/shim/shm_trace_region.hpp"

#include "core/support/text_buffer.hpp"
#include "core/trace/function_scope.hpp"

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/mman.h>
#include <unistd.h>

namespace av {

namespace {

constexpr const char directory[] = "/dev/shm";
constexpr const char *const swept_prefixes[] = {"rvk-trace-", "rvk-state-", "rvk-events-"};

/* The pid a swept file names: the digits after its last '-', before any '.'. */
uint32_t pid_of(const char *name)
{
    AV_SCOPE("pid_of");
    const char *digits = nullptr;
    for (const char *cursor = name; *cursor != '\0'; ++cursor)
        if (*cursor == '-') digits = cursor + 1;
    uint32_t pid = 0u;
    for (const char *cursor = digits; cursor != nullptr && *cursor >= '0' && *cursor <= '9'; ++cursor)
        pid = pid * 10u + static_cast<uint32_t>(*cursor - '0');
    return pid;
}

bool starts_with(const char *name, const char *prefix)
{
    AV_SCOPE("starts_with");
    for (; *prefix != '\0'; ++prefix, ++name)
        if (*name != *prefix) return false;
    return true;
}

}  // namespace

ShmTraceRegion::~ShmTraceRegion()
{
    AV_SCOPE("ShmTraceRegion::~ShmTraceRegion");
    if (region_ != nullptr) static_cast<void>(munmap(region_, bytes_));
}

Status ShmTraceRegion::open(const char *module, size_t bytes)
{
    AV_SCOPE("ShmTraceRegion::open");
    return open("/rvk-trace-", module, bytes);
}

Status ShmTraceRegion::open(const char *prefix, const char *module, size_t bytes)
{
    AV_SCOPE("ShmTraceRegion::open");
    if (region_ != nullptr) return Status::invalid_state;
    sweep();
    const uint32_t process = static_cast<uint32_t>(getpid());
    char           name[name_bytes];
    TextBuffer     text(name, sizeof name);
    text.append(prefix);
    text.append(module);
    text.append("-");
    text.append_decimal(process);
    /* A pid is reused: what a dead process of this number left is not ours. */
    static_cast<void>(shm_unlink(name));
    const int file = shm_open(name, O_CREAT | O_EXCL | O_RDWR, 0600);
    if (file < 0) return Status::no_memory;
    void *mapped = MAP_FAILED;
    if (ftruncate(file, static_cast<off_t>(bytes)) == 0)
        mapped = mmap(nullptr, bytes, PROT_READ | PROT_WRITE, MAP_SHARED, file, 0);
    static_cast<void>(close(file));
    if (mapped == MAP_FAILED) {
        static_cast<void>(shm_unlink(name));
        return Status::no_memory;
    }
    region_ = static_cast<uint8_t *>(mapped);
    bytes_ = bytes;
    process_ = process;
    return Status::ok;
}

uint8_t *ShmTraceRegion::region() const
{
    AV_SCOPE("ShmTraceRegion::region");
    return region_;
}

size_t ShmTraceRegion::bytes() const
{
    AV_SCOPE("ShmTraceRegion::bytes");
    return bytes_;
}

uint32_t ShmTraceRegion::process() const
{
    AV_SCOPE("ShmTraceRegion::process");
    return process_;
}

void ShmTraceRegion::sweep() const
{
    AV_SCOPE("ShmTraceRegion::sweep");
    DIR *const listing = opendir(directory);
    if (listing == nullptr) return;
    for (const dirent *entry = readdir(listing); entry != nullptr; entry = readdir(listing)) {
        bool ours = false;
        for (const char *const prefix : swept_prefixes) ours = ours || starts_with(entry->d_name, prefix);
        const uint32_t pid = pid_of(entry->d_name);
        /* Only a process that does not exist: another user's live one refuses
         * the probe with EPERM. */
        if (!ours || pid == 0u || kill(static_cast<pid_t>(pid), 0) == 0 || errno != ESRCH) continue;
        static_cast<void>(unlinkat(dirfd(listing), entry->d_name, 0));
    }
    static_cast<void>(closedir(listing));
}

}  // namespace av
