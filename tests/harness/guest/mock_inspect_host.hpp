#ifndef AV_MOCK_INSPECT_HOST_HPP
#define AV_MOCK_INSPECT_HOST_HPP

#include "guest/inspect/inspect_host.hpp"

#include <cstddef>
#include <cstdint>

namespace av {

class MockInspectHost final : public InspectHost {
public:
    static constexpr std::uint32_t max_entries = 16u;
    static constexpr std::size_t   max_name = 64u;
    static constexpr std::size_t   journal_bytes = 4096u;

    MockInspectHost();
    ~MockInspectHost();

    std::uint32_t processes(std::uint32_t *pids, std::uint32_t capacity) override;
    Status        executable(std::uint32_t pid, char *path, std::size_t capacity) override;
    Status        copy_section(const char *name, std::uint8_t *destination, std::size_t capacity,
                               std::size_t &copied) override;
    Status        request_state(const char *dump, const char *done, std::uint32_t milliseconds) override;
    Status        write_file(const char *path, const std::uint8_t *head, std::size_t head_bytes,
                             const std::uint8_t *body, std::size_t body_bytes) override;
    std::uint64_t clock() const override;

    void add_process(std::uint32_t pid, const char *path);
    void claim_processes(std::uint32_t count);
    void add_section(const char *name, const std::uint8_t *bytes, std::size_t length);
    void add_service(const char *dump, Status answer);
    void set_clock(std::uint64_t now);
    void fail_next_write(Status status);

    const char         *written_path() const;
    const std::uint8_t *written() const;
    std::size_t         written_size() const;
    std::uint32_t       writes() const;
    const char         *journal() const;
    std::uint32_t       waited() const;

private:
    struct Process {
        std::uint32_t pid;
        const char   *path;
    };
    struct Section {
        char                name[max_name];
        const std::uint8_t *bytes;
        std::size_t         length;
    };
    struct Service {
        char   dump[max_name];
        Status answer;
    };

    void note(const char *name);

    Process        processes_[max_entries];
    std::uint32_t  process_count_;
    std::uint32_t  claimed_;
    Section        sections_[max_entries];
    std::uint32_t  section_count_;
    Service        services_[max_entries];
    std::uint32_t  service_count_;
    std::uint64_t  now_;
    Status         write_failure_;
    char           path_[max_name * 8u];
    std::uint8_t  *written_;
    std::size_t    written_size_;
    std::uint32_t  writes_;
    char           journal_[journal_bytes];
    std::size_t    journal_length_;
    std::uint32_t  waited_;
};

}

#endif
