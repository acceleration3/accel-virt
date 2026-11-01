#include "um/d3d/d3d_call_trace.hpp"

#include "core/trace/function_scope.hpp"
#include "icd/d3d/record_stamp.hpp"
#include "um/common/guest_setting.hpp"

#include <windows.h>

namespace av::d3d {

namespace {

/* What the variable holds when it names no count: enough for a game's first
 * frames after it loads. */
constexpr uint64_t default_count = 200000u;

}  // namespace

D3dCallTrace::D3dCallTrace()
    : file_(L"avumd-calls"), clock_(), caller_(), on_(false), skip_(0u), count_(0u), calls_(0u), contexts_(0u), started_(0u)
{
    AV_SCOPE("D3dCallTrace::D3dCallTrace");
}

void D3dCallTrace::open()
{
    AV_SCOPE("D3dCallTrace::open");
    const GuestSetting setting("AV_D3D_TRACE");
    const SettingText  value = setting.value();
    if (value.text() == nullptr) return;
    uint64_t first = 0u;
    if (!value.number(first)) first = 0u;
    const SettingText counted = value.after('+');
    if (counted.text() != nullptr) {
        skip_ = first;
        if (!counted.number(count_)) count_ = 0u;
    } else {
        count_ = first;
    }
    if (count_ == 0u) count_ = default_count;
    started_ = clock_.microseconds();
    file_.open();
    on_ = true;
}

bool D3dCallTrace::on() const
{
    AV_SCOPE("D3dCallTrace::on");
    return on_;
}

uint64_t D3dCallTrace::microseconds() const
{
    AV_SCOPE("D3dCallTrace::microseconds");
    return clock_.microseconds() - started_;
}

uint32_t D3dCallTrace::number_context()
{
    AV_SCOPE("D3dCallTrace::number_context");
    return __atomic_fetch_add(&contexts_, 1u, __ATOMIC_RELAXED);
}

bool D3dCallTrace::take()
{
    AV_SCOPE("D3dCallTrace::take");
    const uint64_t call = __atomic_fetch_add(&calls_, 1u, __ATOMIC_RELAXED);
    return call >= skip_ && call < skip_ + count_;
}

uint32_t D3dCallTrace::thread()
{
    AV_SCOPE("D3dCallTrace::thread");
    return caller_.thread_id();
}

D3dCallTrace::Line::Line(uint64_t microseconds, uint32_t thread, uint32_t context, const char *slot)
    : bytes_{}, body_(bytes_, sizeof bytes_)
{
    AV_SCOPE("D3dCallTrace::Line::Line");
    body_.append_decimal(microseconds);
    body_.append(" t");
    body_.append_decimal(thread);
    body_.append(" c");
    body_.append_decimal(context);
    body_.append(" ");
    body_.append(slot);
}

const char *D3dCallTrace::Line::text() const
{
    AV_SCOPE("D3dCallTrace::Line::text");
    return body_.text();
}

void D3dCallTrace::Line::number(uint64_t value)
{
    AV_SCOPE("D3dCallTrace::Line::number");
    body_.append(" ");
    body_.append_decimal(value);
}

void D3dCallTrace::Line::signed_number(int64_t value)
{
    AV_SCOPE("D3dCallTrace::Line::signed_number");
    body_.append(value < 0 ? " -" : " ");
    body_.append_decimal(value < 0 ? 0u - static_cast<uint64_t>(value) : static_cast<uint64_t>(value));
}

/* Every record starts with its stamp (d3d11_records.hpp). */
void D3dCallTrace::Line::object(const void *block)
{
    AV_SCOPE("D3dCallTrace::Line::object");
    /* An array the runtime passes with a count of 0 may point anywhere (SetDescriptorHeaps). */
    MEMORY_BASIC_INFORMATION memory = {};
    if (block != nullptr && (VirtualQuery(block, &memory, sizeof memory) == 0u || memory.State != MEM_COMMIT ||
                             (memory.Protect & (PAGE_NOACCESS | PAGE_GUARD)) != 0u)) {
        body_.append(" unreadable");
        return;
    }
    body_.append(" #");
    body_.append_decimal(block == nullptr ? 0u : static_cast<const d3d::RecordStamp *>(block)->id());
}

void D3dCallTrace::Line::pointer(const void *address)
{
    AV_SCOPE("D3dCallTrace::Line::pointer");
    body_.append(address == nullptr ? " null" : " ptr");
}

void D3dCallTrace::Line::resource(const D3D11DDIARG_CREATERESOURCE &create)
{
    AV_SCOPE("D3dCallTrace::Line::resource");
    body_.append(" dimension=");
    body_.append_decimal(static_cast<uint64_t>(create.ResourceDimension));
    if (create.pMipInfoList != nullptr) {
        body_.append(" size=");
        body_.append_decimal(create.pMipInfoList[0].TexelWidth);
        body_.append("x");
        body_.append_decimal(create.pMipInfoList[0].TexelHeight);
        body_.append("x");
        body_.append_decimal(create.pMipInfoList[0].TexelDepth);
    }
    body_.append(" mips=");
    body_.append_decimal(create.MipLevels);
    body_.append(" layers=");
    body_.append_decimal(create.ArraySize);
    body_.append(" format=");
    body_.append_decimal(static_cast<uint64_t>(create.Format));
    body_.append(" usage=");
    body_.append_decimal(create.Usage);
    body_.append(" bind=0x");
    body_.append_hex(create.BindFlags, 4u);
    body_.append(" cpu=0x");
    body_.append_hex(create.MapFlags, 4u);
    body_.append(" misc=0x");
    body_.append_hex(create.MiscFlags, 4u);
    body_.append(" stride=");
    body_.append_decimal(create.ByteStride);
    body_.append(create.pInitialDataUP != nullptr ? " initial" : "");
    body_.append(create.pPrimaryDesc != nullptr ? " primary" : "");
}

void D3dCallTrace::Line::box(const D3D10_DDI_BOX &box)
{
    AV_SCOPE("D3dCallTrace::Line::box");
    body_.append(" box=");
    body_.append_decimal(static_cast<uint32_t>(box.left));
    body_.append(",");
    body_.append_decimal(static_cast<uint32_t>(box.top));
    body_.append(",");
    body_.append_decimal(static_cast<uint32_t>(box.front));
    body_.append("-");
    body_.append_decimal(static_cast<uint32_t>(box.right));
    body_.append(",");
    body_.append_decimal(static_cast<uint32_t>(box.bottom));
    body_.append(",");
    body_.append_decimal(static_cast<uint32_t>(box.back));
}

}  // namespace av::d3d
