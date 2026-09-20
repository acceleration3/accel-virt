/* The cl_mem objects one OpenCL context imported from a Win32 handle
 * (feature 053, US2, contracts/wire_additions.md).
 *
 * `cl_khr_external_memory_win32` is how an OpenCL application names memory
 * another API made: the handle travels in `clCreateBufferWithProperties`'
 * property list and what comes back is an ordinary `cl_mem`. A HANDLE is a
 * value only the guest kernel resolves, so nothing of it crosses the wire -
 * this turns it into the object id behind it and opens that on the device,
 * which answers the `cl_mem` it made.
 *
 * Only the opaque Win32 handle is taken. The KMT spelling names a D3D9-era
 * object and the NAME spelling is a named section, which is exactly what
 * another process opens *by name* - the thing this path refuses on purpose -
 * while the fd and dma-buf spellings name descriptors no guest has.
 *
 * One per guest context, because the device counts this context as a holder of
 * every object it opened. */
#ifndef AV_CLICD_CL_EXTERNAL_MEMORY_HPP
#define AV_CLICD_CL_EXTERNAL_MEMORY_HPP

#include <CL/cl.h>

#include <stdint.h>

namespace av {
class ClientTransport;
class StateWriter;
}  // namespace av

namespace av::icd {
class LogSink;
class SharedHandles;
}  // namespace av::icd

namespace av::clicd {

class ClPropertyList;
struct ClExternalMemoryProbe;

/* One object this context opened, and what the device made of it. A type of
 * its own so the table's rows are complete (tools/header_scan.py reads a
 * class's members only to its first nested type). */
struct ClImportedMemoryRow {
    /* What the object is called on the wire. Never 0 in a live row. */
    uint64_t object_id_;
    /* The cl_mem the device answered, which the guest names it by. */
    uint64_t made_;
    /* The bytes it was opened as, which a release names again. */
    uint64_t size_;
};

class ClExternalMemory {
public:
    /* As many as a context can share, matching icd::SharedHandles: an object
     * whose handle the process could not hold is one it could not name. */
    static constexpr uint32_t max_objects = 64u;
    /* The wire body wants whole pages of a memory object, and so does the
     * record the publisher wrote. */
    static constexpr uint64_t page_bytes = 4096u;
    /* Every external handle property `cl_khr_external_memory` defines. The
     * caller that judges a property list needs them to let one through to
     * `open`, which is where the four this path cannot take are refused with
     * the reason - a list judged against the one taken would refuse the other
     * four as unknown properties, which says less. */
    static constexpr uint32_t handle_property_count = 5u;
    static constexpr uint64_t handle_properties[handle_property_count] = {
        0x2061u,  /* CL_EXTERNAL_MEMORY_HANDLE_OPAQUE_WIN32_KHR      */
        0x2062u,  /* CL_EXTERNAL_MEMORY_HANDLE_OPAQUE_WIN32_KMT_KHR  */
        0x2069u,  /* CL_EXTERNAL_MEMORY_HANDLE_OPAQUE_WIN32_NAME_KHR */
        0x2060u,  /* CL_EXTERNAL_MEMORY_HANDLE_OPAQUE_FD_KHR         */
        0x2067u}; /* CL_EXTERNAL_MEMORY_HANDLE_DMA_BUF_KHR           */

    ClExternalMemory(icd::SharedHandles &handles, icd::LogSink &log);
    ClExternalMemory(const ClExternalMemory &) = delete;
    ClExternalMemory &operator=(const ClExternalMemory &) = delete;

    /* Whether `list` asks for memory some other API made. True for every
     * external handle property, including the ones this path does not take: a
     * list naming one of those is a create to refuse, never one to answer with
     * ordinary memory, because an application handed a blank buffer where it
     * asked for somebody else's image would read zeroes and never know why. */
    bool asked_for(const ClPropertyList &list) const;

    /* The object `list` names, opened on the device for this context, with the
     * `cl_mem` it answered in `made`. `size` is the buffer the application
     * asked for, rounded up to whole pages before it is named, because that is
     * what an open is judged against.
     *
     * CL_INVALID_PROPERTY for a handle type this path does not take;
     * CL_INVALID_VALUE for a handle of 0, a handle this process may not open,
     * a size of 0 or one no whole number of pages holds, and an object this
     * context already holds - which the device refuses too, because a context
     * with two cl_mem over one object could release it twice;
     * CL_OUT_OF_RESOURCES when the table is full or the device would not open
     * it. `made` is 0 unless CL_SUCCESS. */
    cl_int open(ClientTransport &wire, uint32_t device, const ClPropertyList &list, uint64_t size,
                uint64_t &made);

    /* The object behind `made` released on the device and the row dropped.
     * False for a `cl_mem` this table never answered - an ordinary buffer,
     * which its caller then gives back the ordinary way. */
    bool   give_back(ClientTransport &wire, uint64_t made);
    /* Every object still held, released: what a context torn down gives back,
     * since the device counts this context as a holder of each. */
    void   release_all(ClientTransport &wire);

    uint32_t live() const;
    void     report(StateWriter &writer) const;

    /* A live row names an object, a cl_mem and a size, and neither the object
     * nor the cl_mem appears twice. */
    bool invariants_hold() const;

private:
    friend struct ClExternalMemoryProbe;

    /* The handle `list` names and whether this path takes its type; false when
     * the list names no external handle at all. */
    bool                names_handle(const ClPropertyList &list, uint64_t &handle, bool &taken) const;
    ClImportedMemoryRow *row_of(uint64_t made);
    /* Whether this context already holds the object `object_id` names. */
    bool                 holds(uint64_t object_id) const;

    icd::SharedHandles &handles_;
    icd::LogSink       &log_;
    ClImportedMemoryRow rows_[max_objects];
    uint32_t            live_;
};

}  // namespace av::clicd

#endif  // AV_CLICD_CL_EXTERNAL_MEMORY_HPP
