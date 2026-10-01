// dxgi_modes.exe: what DXGI answers a game asking for display modes - every
// adapter's outputs, and for each the count of modes GetDisplayModeList gives
// in the formats games ask for, with the largest. Dark Souls Remastered takes
// the last mode of its format's list without looking at the count.
//
//   dxgi_modes.exe
#include <cstdio>
#include <dxgi.h>

int wmain()
{
    /* A probe that crashes must have printed what it had. */
    setvbuf(stdout, nullptr, _IONBF, 0);
    IDXGIFactory1 *factory = nullptr;
    if (FAILED(CreateDXGIFactory1(__uuidof(IDXGIFactory1), reinterpret_cast<void **>(&factory)))) {
        wprintf(L"CreateDXGIFactory1 failed\n");
        return 1;
    }
    const DXGI_FORMAT formats[] = {DXGI_FORMAT_R8G8B8A8_UNORM, DXGI_FORMAT_R8G8B8A8_UNORM_SRGB,
                                   DXGI_FORMAT_B8G8R8A8_UNORM, DXGI_FORMAT_B8G8R8A8_UNORM_SRGB,
                                   DXGI_FORMAT_R10G10B10A2_UNORM, DXGI_FORMAT_R16G16B16A16_FLOAT};
    IDXGIAdapter1 *adapter = nullptr;
    for (UINT a = 0; factory->EnumAdapters1(a, &adapter) != DXGI_ERROR_NOT_FOUND; a++) {
        DXGI_ADAPTER_DESC1 desc = {};
        adapter->GetDesc1(&desc);
        wprintf(L"adapter %u: %ls %04x:%04x\n", a, desc.Description, desc.VendorId, desc.DeviceId);
        IDXGIOutput *output = nullptr;
        for (UINT o = 0; adapter->EnumOutputs(o, &output) != DXGI_ERROR_NOT_FOUND; o++) {
            DXGI_OUTPUT_DESC out = {};
            output->GetDesc(&out);
            wprintf(L"  output %u: %ls attached %d\n", o, out.DeviceName, out.AttachedToDesktop);
            for (const DXGI_FORMAT format : formats) {
                UINT          count = 0;
                const HRESULT asked = output->GetDisplayModeList(format, 0, &count, nullptr);
                DXGI_MODE_DESC modes[512] = {};
                UINT           got = count < 512 ? count : 512;
                if (SUCCEEDED(asked) && got != 0) output->GetDisplayModeList(format, 0, &got, modes);
                if (got != 0)
                    wprintf(L"    format %3u: 0x%08lx %u modes, last %ux%u@%u/%u\n", static_cast<unsigned>(format),
                            static_cast<unsigned long>(asked), count, modes[got - 1].Width, modes[got - 1].Height,
                            modes[got - 1].RefreshRate.Numerator, modes[got - 1].RefreshRate.Denominator);
                else
                    wprintf(L"    format %3u: 0x%08lx %u modes\n", static_cast<unsigned>(format),
                            static_cast<unsigned long>(asked), count);
            }
            output->Release();
        }
        adapter->Release();
    }
    factory->Release();
    return 0;
}
