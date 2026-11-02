#include "harness/guest/mock_inspect_host.hpp"

#include <cstdlib>
#include <cstring>

namespace av {

MockInspectHost::MockInspectHost()
    : processes_{}, process_count_(0u), claimed_(0u), sections_{}, section_count_(0u), services_{},
      service_count_(0u), now_(0u), write_failure_(Status::ok), path_{}, written_(nullptr), written_size_(0u),
      writes_(0u), journal_{}, journal_length_(0u), waited_(0u)
{}

MockInspectHost::~MockInspectHost() { std::free(written_); }

std::uint32_t MockInspectHost::processes(std::uint32_t *pids, std::uint32_t capacity)
{
    std::uint32_t written = 0u;
    for (; written < process_count_ && written < capacity; written++) pids[written] = processes_[written].pid;
    const std::uint32_t claimed = claimed_ != 0u ? claimed_ : written;
    claimed_ = 0u;
    return claimed;
}

Status MockInspectHost::executable(std::uint32_t pid, char *path, std::size_t capacity)
{
    for (std::uint32_t index = 0u; index < process_count_; index++) {
        if (processes_[index].pid != pid || processes_[index].path == nullptr) continue;
        std::strncpy(path, processes_[index].path, capacity - 1u);
        path[capacity - 1u] = '\0';
        return Status::ok;
    }
    return Status::not_found;
}

Status MockInspectHost::copy_section(const char *name, std::uint8_t *destination, std::size_t capacity,
                                     std::size_t &copied)
{
    note(name);
    for (std::uint32_t index = 0u; index < section_count_; index++) {
        if (std::strcmp(sections_[index].name, name) != 0) continue;
        copied = sections_[index].length < capacity ? sections_[index].length : capacity;
        std::memcpy(destination, sections_[index].bytes, copied);
        return Status::ok;
    }
    return Status::not_found;
}

Status MockInspectHost::request_state(const char *dump, const char *done, std::uint32_t milliseconds)
{
    note(dump);
    note(done);
    waited_ = milliseconds;
    for (std::uint32_t index = 0u; index < service_count_; index++)
        if (std::strcmp(services_[index].dump, dump) == 0) return services_[index].answer;
    return Status::not_found;
}

Status MockInspectHost::write_file(const char *path, const std::uint8_t *head, std::size_t head_bytes,
                                   const std::uint8_t *body, std::size_t body_bytes)
{
    writes_++;
    if (write_failure_ != Status::ok) {
        const Status failure = write_failure_;
        write_failure_ = Status::ok;
        return failure;
    }
    std::strncpy(path_, path, sizeof path_ - 1u);
    std::free(written_);
    written_size_ = head_bytes + body_bytes;
    written_ = static_cast<std::uint8_t *>(std::malloc(written_size_ + 1u));
    if (head_bytes != 0u) std::memcpy(written_, head, head_bytes);
    if (body_bytes != 0u) std::memcpy(written_ + head_bytes, body, body_bytes);
    return Status::ok;
}

std::uint64_t MockInspectHost::clock() const { return now_; }

void MockInspectHost::add_process(std::uint32_t pid, const char *path)
{
    processes_[process_count_++] = Process{pid, path};
}

void MockInspectHost::claim_processes(std::uint32_t count) { claimed_ = count; }

void MockInspectHost::add_section(const char *name, const std::uint8_t *bytes, std::size_t length)
{
    Section &section = sections_[section_count_++];
    std::strncpy(section.name, name, max_name - 1u);
    section.bytes = bytes;
    section.length = length;
}

void MockInspectHost::add_service(const char *dump, Status answer)
{
    Service &service = services_[service_count_++];
    std::strncpy(service.dump, dump, max_name - 1u);
    service.answer = answer;
}

void MockInspectHost::set_clock(std::uint64_t now) { now_ = now; }
void MockInspectHost::fail_next_write(Status status) { write_failure_ = status; }

const char         *MockInspectHost::written_path() const { return path_; }
const std::uint8_t *MockInspectHost::written() const { return written_; }
std::size_t         MockInspectHost::written_size() const { return written_size_; }
std::uint32_t       MockInspectHost::writes() const { return writes_; }
const char         *MockInspectHost::journal() const { return journal_; }
std::uint32_t       MockInspectHost::waited() const { return waited_; }

void MockInspectHost::note(const char *name)
{
    const std::size_t length = std::strlen(name);
    if (journal_length_ + length + 2u > sizeof journal_) return;
    std::memcpy(journal_ + journal_length_, name, length);
    journal_length_ += length;
    journal_[journal_length_++] = '\n';
    journal_[journal_length_] = '\0';
}

}
