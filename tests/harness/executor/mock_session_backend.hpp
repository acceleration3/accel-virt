#ifndef AV_MOCK_SESSION_BACKEND_HPP
#define AV_MOCK_SESSION_BACKEND_HPP

#include "executor/session_backend.hpp"
#include "executor/sharing/shared_image_shape.hpp"

#include <cstddef>
#include <cstdint>

namespace av {

class MockSessionBackend final : public SessionBackend {
public:
    static constexpr std::uint32_t echo_id = 0x10001u;
    static constexpr std::uint32_t long_answer_id = 0x10002u;
    static constexpr std::uint32_t malformed_id = 0x10003u;
    static constexpr std::uint32_t greedy_id = 0x10004u;
    static constexpr std::uint32_t lose_id = 0x10005u;
    static constexpr std::uint32_t roomy_id = 0x10006u;
    static constexpr std::uint64_t wire_offset = 0x2000u;
    static constexpr std::uint64_t object_handle = 0x9000u;
    static constexpr std::uint64_t detail_mask = 0xD000000000000000u;

    MockSessionBackend();

    void          set_answer_bytes(std::size_t bytes);
    std::uint32_t ran() const;
    void          set_adopts(bool adopts);
    std::uint32_t adopted() const;
    std::uint32_t abandoned() const;

    bool knows(std::uint32_t command_id) const override;
    [[nodiscard]] bool run(std::uint32_t command_id, WireReader &payload, WireWriter &reply,
                           WireArena &arena) override;
    bool lost() const override;
    [[nodiscard]] bool lose_device() override;
    void set_losable(bool losable);
    void lose_elsewhere();
    void end_batch() override;
    bool poll() override;
    void gpu_progress(uint64_t &marked, uint64_t &reached) const override;
    void bind_signal(void (*signal)(void *context), void *context) override;
    bool signal();
    [[nodiscard]] bool adopt(ImportedBlob &blob) override;
    void abandon(ImportedBlob &blob) override;
    [[nodiscard]] bool export_object(std::uint32_t kind, std::uint64_t wire_handle,
                                     std::uint32_t &handle_kind, int &fd) override;
    [[nodiscard]] bool import_object(std::uint32_t kind, std::uint32_t handle_kind, int fd,
                                     std::uint64_t size, const av::SharedImageShape &shape,
                                     std::uint64_t &wire_handle, std::uint64_t &detail) override;
    const av::SharedImageShape &imported_shape() const { return imported_shape_; }
    void               release_object(std::uint32_t kind, std::uint64_t wire_handle, std::uint64_t detail) override;

    void          shut_down() override;
    std::uint32_t shut_downs() const;
    std::uint32_t abandoned_before_shut_down() const;
    std::uint32_t released_before_shut_down() const;

    void          set_shares(bool shares);
    std::uint32_t exported_objects() const;
    std::uint32_t imported_objects() const;
    std::uint32_t released_objects() const;
    std::uint64_t last_released_detail() const;
    void          before_next_export(void (*during)(void *context), void *context);

private:
    av::SharedImageShape imported_shape_;
    std::size_t   answer_bytes_;
    std::uint32_t ran_;
    std::uint32_t adopted_;
    std::uint32_t abandoned_;
    bool          lost_;
    bool          losable_;
    bool          adopts_;
    bool          shares_;
    std::uint32_t exported_objects_;
    std::uint32_t imported_objects_;
    std::uint32_t released_objects_;
    std::uint64_t last_released_detail_ = 0u;
    std::uint32_t shut_downs_ = 0u;
    std::uint32_t abandoned_before_shut_down_ = 0u;
    std::uint32_t released_before_shut_down_ = 0u;
    int           descriptor_;
    void        (*signal_)(void *context);
    void         *signal_context_;
    void        (*during_export_)(void *context);
    void         *during_export_context_;
};

}

#endif
