/* ADLVersionsInfoX2 (adl_structures.h, AMD Display Library SDK, MIT): what
 * ADL2_Graphics_VersionsX2_Get writes. A DDI record. */
#ifndef AV_VENDOR_ADL_VERSIONS_INFO_X2_RECORD_HPP
#define AV_VENDOR_ADL_VERSIONS_INFO_X2_RECORD_HPP

namespace av::vendor {

struct AdlVersionsInfoX2Record {
    char strDriverVer[256];
    char strCatalystVersion[256];
    char strCrimsonVersion[256];
    char strCatalystWebLink[256];
};

}  // namespace av::vendor

#endif  // AV_VENDOR_ADL_VERSIONS_INFO_X2_RECORD_HPP
