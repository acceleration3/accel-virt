#ifndef AV_VENDOR_ADL_ADAPTER_INFO_RECORD_HPP
#define AV_VENDOR_ADL_ADAPTER_INFO_RECORD_HPP

namespace av::vendor {

struct AdlAdapterInfoRecord {
    int  iSize;
    int  iAdapterIndex;
    char strUDID[256];
    int  iBusNumber;
    int  iDeviceNumber;
    int  iFunctionNumber;
    int  iVendorID;
    char strAdapterName[256];
    char strDisplayName[256];
    int  iPresent;
    int  iExist;
    char strDriverPath[256];
    char strDriverPathExt[256];
    char strPNPString[256];
    int  iOSDisplayIndex;
};

}

#endif
