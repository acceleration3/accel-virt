// d3d11_cube.exe: a cube map made with initial data for each face, then read
// back face by face, which is what a game's environment map does at load.
// Prints each face's first texel; a driver that counts the faces wrong reads
// past the initial data it was given.
//
//   d3d11_cube.exe
#include <cstdio>
#include <d3d11.h>

int wmain()
{
    setvbuf(stdout, nullptr, _IONBF, 0);
    ID3D11Device        *device = nullptr;
    ID3D11DeviceContext *context = nullptr;
    const HRESULT made = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, nullptr, 0,
                                           D3D11_SDK_VERSION, &device, nullptr, &context);
    wprintf(L"D3D11CreateDevice 0x%08lx\n", static_cast<unsigned long>(made));
    if (FAILED(made)) return 1;
    constexpr UINT edge = 4, faces = 6;
    UINT32         texels[faces][edge * edge] = {};
    D3D11_SUBRESOURCE_DATA initial[faces] = {};
    for (UINT face = 0; face < faces; face++) {
        for (UINT32 &texel : texels[face]) texel = 0xff000000u | (face + 1u) * 0x101010u;
        initial[face] = {texels[face], edge * 4u, 0u};
    }
    D3D11_TEXTURE2D_DESC desc = {};
    desc.Width = edge;
    desc.Height = edge;
    desc.MipLevels = 1;
    desc.ArraySize = faces;
    desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    desc.MiscFlags = D3D11_RESOURCE_MISC_TEXTURECUBE;
    ID3D11Texture2D *cube = nullptr;
    const HRESULT created = device->CreateTexture2D(&desc, initial, &cube);
    wprintf(L"CreateTexture2D cube 0x%08lx\n", static_cast<unsigned long>(created));
    if (FAILED(created)) return 2;
    desc.Usage = D3D11_USAGE_STAGING;
    desc.BindFlags = 0;
    desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    ID3D11Texture2D *staging = nullptr;
    if (FAILED(device->CreateTexture2D(&desc, nullptr, &staging))) return 3;
    context->CopyResource(staging, cube);
    for (UINT face = 0; face < faces; face++) {
        D3D11_MAPPED_SUBRESOURCE mapped = {};
        const HRESULT read = context->Map(staging, face, D3D11_MAP_READ, 0, &mapped);
        wprintf(L"face %u: 0x%08lx first 0x%08x\n", face, static_cast<unsigned long>(read),
                SUCCEEDED(read) ? *static_cast<const UINT32 *>(mapped.pData) : 0u);
        if (SUCCEEDED(read)) context->Unmap(staging, face);
    }
    staging->Release();
    cube->Release();
    context->Release();
    device->Release();
    return 0;
}
