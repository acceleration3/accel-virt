#ifndef AV_VENDOR_ADL_VERSIONS_INFO_RECORD_HPP
#define AV_VENDOR_ADL_VERSIONS_INFO_RECORD_HPP

namespace av::vendor {

struct AdlVersionsInfoRecord {
    char strDriverVer[256];
    char strCatalystVersion[256];
    char strCatalystWebLink[256];
};

}

#endif
