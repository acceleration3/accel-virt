#include "icd/common/split_report.hpp"

#include "core/trace/function_scope.hpp"
#include "proto/client/client_transport.hpp"

namespace av::icd {

SplitReport::SplitReport() : wire_(nullptr), pieces_(0u)
{
    AV_SCOPE("SplitReport::SplitReport");
}

SplitReport::SplitReport(ClientTransport &wire) : wire_(&wire), pieces_(0u)
{
    AV_SCOPE("SplitReport::SplitReport");
}

void SplitReport::piece()
{
    AV_SCOPE("SplitReport::piece");
    if (wire_ == nullptr) return;
    pieces_++;
    if (pieces_ == 2u) wire_->note(ChannelCounter::split_transfers, 1u);
    if (pieces_ >= 2u) wire_->note(ChannelCounter::split_pieces, 1u);
}

}
