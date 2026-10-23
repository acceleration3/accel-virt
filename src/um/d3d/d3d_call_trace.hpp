/* The Direct3D runtimes' calls into this driver - 9's, 11's and 12's - one
 * line each, written to C:\ProgramData\av\avumd-calls-<pid>.log while
 * AV_D3D_TRACE is set in the process's environment: when, on which thread,
 * into which context (each device, deferred context and Direct3D 9 device
 * numbered as it is made), the slot, and its arguments - a number as it is, an
 * object by the id its record's stamp holds, the first of an array of them,
 * and a resource or a view by what it is made of. What an application does at
 * the DDI, for a probe to be written from (docs/testing.md, "Probes from an
 * application's calls"). A Direct3D 9 device writes its frame, wait and state
 * lines among them.
 *
 * The variable's value is how many calls to write, after how many to pass
 * over: "20000", or "300000+20000" for the twenty thousand after the first
 * three hundred thousand. One per process, which UmdDrivers owns and hands
 * every runtime's half; while it is off a call costs a load and a branch. */
#ifndef AV_UM_D3D_CALL_TRACE_HPP
#define AV_UM_D3D_CALL_TRACE_HPP

#include "core/support/text_buffer.hpp"
#include "core/trace/function_scope.hpp"
#include "um/common/file_log.hpp"
#include "um/common/performance_clock.hpp"
#include "um/common/windows_calling_thread.hpp"
#include "um/d3d/d3d11_ddi.hpp"

#include <stdint.h>

namespace av::d3d {

class D3dCallTrace {
public:
    D3dCallTrace();
    D3dCallTrace(const D3dCallTrace &) = delete;
    D3dCallTrace &operator=(const D3dCallTrace &) = delete;

    /* Reads the environment once, and opens the file if it asks for calls. */
    void     open();
    bool     on() const;
    /* The number the next device or deferred context is called by. */
    uint32_t number_context();
    template <typename... Arguments>
    void     call(uint32_t context, const char *slot, const Arguments &...arguments);

private:
    /* One call's line, cut at what one log line holds. */
    class Line {
    public:
        Line(uint64_t microseconds, uint32_t thread, uint32_t context, const char *slot);
        Line(const Line &) = delete;
        Line &operator=(const Line &) = delete;

        template <typename Argument>
        void        put(const Argument &argument);
        const char *text() const;

    private:
        void number(uint64_t value);
        void signed_number(int64_t value);
        void object(const void *block);
        void pointer(const void *address);
        void resource(const D3D11DDIARG_CREATERESOURCE &create);
        void box(const D3D10_DDI_BOX &box);
        template <typename View>
        void view(const View &view);

        static constexpr uint32_t line_bytes = 250u;
        char       bytes_[line_bytes];
        TextBuffer body_;
    };

    /* Whether this call is one to write, counting it either way. */
    bool     take();
    uint64_t microseconds() const;
    uint32_t thread();

    FileLog          file_;
    PerformanceClock clock_;
    WindowsCallingThread caller_;
    bool     on_;
    uint64_t skip_;
    uint64_t count_;
    uint64_t calls_;
    uint32_t contexts_;
    /* The clock's microseconds when the trace opened. */
    uint64_t started_;
};

template <typename... Arguments>
void D3dCallTrace::call(uint32_t context, const char *slot, const Arguments &...arguments)
{
    AV_SCOPE("D3dCallTrace::call");
    if (!on_ || !take()) return;
    Line line(microseconds(), thread(), context, slot);
    (line.put(arguments), ...);
    file_.write(line.text());
}

template <typename Argument>
void D3dCallTrace::Line::put(const Argument &argument)
{
    AV_SCOPE("D3dCallTrace::Line::put");
    if constexpr (requires { argument.pDrvPrivate; }) {
        object(argument.pDrvPrivate);
    } else if constexpr (requires { argument.handle; }) {
        return;
    } else if constexpr (__is_enum(Argument) || __is_integral(Argument)) {
        if constexpr (__is_signed(Argument)) signed_number(static_cast<int64_t>(argument));
        else number(static_cast<uint64_t>(argument));
    } else if constexpr (__is_floating_point(Argument)) {
        signed_number(static_cast<int64_t>(argument * 1000.0F));
    } else if constexpr (requires { argument->pMipInfoList; }) {
        if (argument == nullptr) pointer(nullptr);
        else resource(*argument);
    } else if constexpr (requires { argument->hDrvResource; argument->Format; }) {
        if (argument == nullptr) pointer(nullptr);
        else view(*argument);
    } else if constexpr (requires { argument->left; argument->back; }) {
        if (argument == nullptr) pointer(nullptr);
        else box(*argument);
    } else if constexpr (requires { argument->pDrvPrivate; }) {
        /* An array of objects: its first, which is what a one-slot bind is. */
        if (argument == nullptr) pointer(nullptr);
        else object(argument->pDrvPrivate);
    } else if constexpr (__is_pointer(Argument)) {
        pointer(argument);
    } else {
        body_.append(" ?");
    }
}

template <typename View>
void D3dCallTrace::Line::view(const View &view)
{
    AV_SCOPE("D3dCallTrace::Line::view");
    body_.append(" of");
    object(view.hDrvResource.pDrvPrivate);
    body_.append(" format=");
    body_.append_decimal(static_cast<uint64_t>(view.Format));
}

}  // namespace av::d3d

#endif  // AV_UM_D3D_CALL_TRACE_HPP
