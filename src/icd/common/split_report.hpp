#ifndef AV_ICD_SPLIT_REPORT_HPP
#define AV_ICD_SPLIT_REPORT_HPP

#include <stdint.h>

namespace av {
class ClientTransport;
}

namespace av::icd {

class SplitReport {
public:
    SplitReport();
    explicit SplitReport(ClientTransport &wire);

    void piece();

private:
    ClientTransport *wire_;
    uint64_t         pieces_;
};

}

#endif
