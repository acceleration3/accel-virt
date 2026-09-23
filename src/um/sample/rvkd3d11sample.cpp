/* A D3D11 sample application: a coloured triangle spinning in a 640x480
 * window, its shaders compiled at run time by d3dcompiler_47 as samples do,
 * its rotation in a constant buffer, presented through a flip-model swap
 * chain at vsync. Runs RVK_SAMPLE_FRAMES frames (600 by default) and writes
 * what it drew - the frame count, the present results and the centre and a
 * corner pixel read back - to standard output. No C runtime. */
#include <d3d11.h>
#include <d3dcommon.h>
#include <windows.h>

#include <stdint.h>

extern "C" int  _purecall();
extern "C" int  _purecall() { ExitProcess(STATUS_ASSERTION_FAILURE); }
extern "C" int  _fltused = 0;

namespace {

const char shader_source[] =
    "cbuffer Frame : register(b0) { float angle; float3 unused; };\n"
    "struct In { float2 position : POSITION; float3 colour : COLOR; };\n"
    "struct Out { float4 position : SV_Position; float3 colour : COLOR; };\n"
    "Out vs(In v) { float s = sin(angle), c = cos(angle); Out o;\n"
    "  o.position = float4(v.position.x * c - v.position.y * s, v.position.x * s + v.position.y * c, 0, 1);\n"
    "  o.colour = v.colour; return o; }\n"
    "float4 ps(Out p) : SV_Target { return float4(p.colour, 1); }\n";

using D3DCompileFn = HRESULT(WINAPI *)(LPCVOID, SIZE_T, LPCSTR, const D3D_SHADER_MACRO *, ID3DInclude *, LPCSTR,
                                        LPCSTR, UINT, UINT, ID3DBlob **, ID3DBlob **);

LRESULT CALLBACK window_procedure(HWND window, UINT message, WPARAM w, LPARAM l)
{
    return DefWindowProcW(window, message, w, l);
}

class Sample {
public:
    int run()
    {
        if (!make_window()) return say("FAIL window\n"), 1;
        if (!make_device()) return say("FAIL device\n"), 2;
        if (!make_pipeline()) return say("FAIL pipeline\n"), 3;
        const uint32_t frames = frames_wanted();
        uint32_t presented = 0u, failures = 0u;
        HRESULT last = S_OK;
        for (uint32_t frame = 0u; frame < frames; frame++) {
            pump();
            draw(static_cast<float>(frame) * 0.05F);
            if (frame == 30u) read_back();
            last = swapchain_->Present(1u, 0u);
            if (last == S_OK) presented++; else failures++;
        }
        say("frames "); number(frames); say(" presented "); number(presented); say(" failed "); number(failures);
        say(" last 0x"); hex(static_cast<uint32_t>(last)); say(" centre 0x"); hex(centre_); say(" corner 0x");
        hex(corner_); say("\n");
        const bool drew = centre_ != corner_ && (corner_ & 0x00FFFFFFu) == 0x00102040u;
        say(drew && failures == 0u ? "ok d3d11sample\n" : "FAIL d3d11sample\n");
        return drew && failures == 0u ? 0 : 4;
    }

private:
    static constexpr UINT width = 640u, height = 480u;

    bool make_window()
    {
        WNDCLASSEXW type = {};
        type.cbSize = sizeof(type);
        type.lpfnWndProc = window_procedure;
        type.hInstance = GetModuleHandleW(nullptr);
        type.lpszClassName = L"rvkd3d11sample";
        RegisterClassExW(&type);
        window_ = CreateWindowExW(0u, L"rvkd3d11sample", L"rvk D3D11 sample", WS_OVERLAPPEDWINDOW | WS_VISIBLE, 100,
                                  100, static_cast<int>(width), static_cast<int>(height), nullptr, nullptr,
                                  type.hInstance, nullptr);
        if (window_ != nullptr) ShowWindow(window_, SW_SHOW);
        return window_ != nullptr;
    }

    bool make_device()
    {
        RECT client = {};
        GetClientRect(window_, &client);
        DXGI_SWAP_CHAIN_DESC described = {};
        described.BufferDesc.Width = static_cast<UINT>(client.right);
        described.BufferDesc.Height = static_cast<UINT>(client.bottom);
        described.BufferDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
        described.SampleDesc.Count = 1u;
        described.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        described.BufferCount = 2u;
        described.OutputWindow = window_;
        described.Windowed = TRUE;
        described.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
        D3D_FEATURE_LEVEL made = D3D_FEATURE_LEVEL_11_0;
        if (FAILED(D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0u, nullptr, 0u,
                                                 D3D11_SDK_VERSION, &described, &swapchain_, &device_, &made,
                                                 &context_)))
            return false;
        width_ = described.BufferDesc.Width;
        height_ = described.BufferDesc.Height;
        ID3D11Texture2D *back = nullptr;
        if (FAILED(swapchain_->GetBuffer(0u, __uuidof(ID3D11Texture2D), reinterpret_cast<void **>(&back)))) return false;
        const HRESULT viewed = device_->CreateRenderTargetView(back, nullptr, &target_);
        back->Release();
        return SUCCEEDED(viewed);
    }

    bool compile(D3DCompileFn compile_fn, const char *entry, const char *profile, ID3DBlob **code)
    {
        ID3DBlob *errors = nullptr;
        const HRESULT compiled = compile_fn(shader_source, sizeof(shader_source) - 1u, "sample", nullptr, nullptr, entry,
                                            profile, 0u, 0u, code, &errors);
        if (errors != nullptr) {
            say(static_cast<const char *>(errors->GetBufferPointer()));
            errors->Release();
        }
        return SUCCEEDED(compiled);
    }

    bool make_pipeline()
    {
        HMODULE compiler = LoadLibraryA("d3dcompiler_47.dll");
        auto compile_fn = compiler != nullptr ? reinterpret_cast<D3DCompileFn>(GetProcAddress(compiler, "D3DCompile"))
                                              : nullptr;
        if (compile_fn == nullptr) return say("FAIL no d3dcompiler_47\n"), false;
        ID3DBlob *vs = nullptr, *ps = nullptr;
        if (!compile(compile_fn, "vs", "vs_4_0", &vs) || !compile(compile_fn, "ps", "ps_4_0", &ps)) return false;
        if (FAILED(device_->CreateVertexShader(vs->GetBufferPointer(), vs->GetBufferSize(), nullptr, &vertex_)) ||
            FAILED(device_->CreatePixelShader(ps->GetBufferPointer(), ps->GetBufferSize(), nullptr, &pixel_)))
            return false;
        const D3D11_INPUT_ELEMENT_DESC elements[2] = {
            {"POSITION", 0u, DXGI_FORMAT_R32G32_FLOAT, 0u, 0u, D3D11_INPUT_PER_VERTEX_DATA, 0u},
            {"COLOR", 0u, DXGI_FORMAT_R32G32B32_FLOAT, 0u, 8u, D3D11_INPUT_PER_VERTEX_DATA, 0u}};
        if (FAILED(device_->CreateInputLayout(elements, 2u, vs->GetBufferPointer(), vs->GetBufferSize(), &layout_)))
            return false;
        vs->Release();
        ps->Release();
        const float vertices[15] = {0.0F, 0.6F, 1.0F, 0.0F, 0.0F, 0.52F, -0.3F, 0.0F, 1.0F, 0.0F,
                                    -0.52F, -0.3F, 0.0F, 0.0F, 1.0F};
        D3D11_BUFFER_DESC buffer = {sizeof(vertices), D3D11_USAGE_IMMUTABLE, D3D11_BIND_VERTEX_BUFFER, 0u, 0u, 0u};
        D3D11_SUBRESOURCE_DATA initial = {vertices, 0u, 0u};
        if (FAILED(device_->CreateBuffer(&buffer, &initial, &vertices_))) return false;
        D3D11_BUFFER_DESC constants = {16u, D3D11_USAGE_DEFAULT, D3D11_BIND_CONSTANT_BUFFER, 0u, 0u, 0u};
        return SUCCEEDED(device_->CreateBuffer(&constants, nullptr, &constants_));
    }

    void draw(float angle)
    {
        const float frame[4] = {angle, 0.0F, 0.0F, 0.0F};
        context_->UpdateSubresource(constants_, 0u, nullptr, frame, 0u, 0u);
        const float clear[4] = {0.0625F, 0.125F, 0.25F, 1.0F};
        context_->OMSetRenderTargets(1u, &target_, nullptr);
        context_->ClearRenderTargetView(target_, clear);
        const D3D11_VIEWPORT viewport = {0.0F, 0.0F, static_cast<float>(width_), static_cast<float>(height_), 0.0F, 1.0F};
        context_->RSSetViewports(1u, &viewport);
        const UINT stride = 20u, offset = 0u;
        context_->IASetVertexBuffers(0u, 1u, &vertices_, &stride, &offset);
        context_->IASetInputLayout(layout_);
        context_->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        context_->VSSetShader(vertex_, nullptr, 0u);
        context_->VSSetConstantBuffers(0u, 1u, &constants_);
        context_->PSSetShader(pixel_, nullptr, 0u);
        context_->Draw(3u, 0u);
    }

    void read_back()
    {
        ID3D11Texture2D *back = nullptr;
        if (FAILED(swapchain_->GetBuffer(0u, __uuidof(ID3D11Texture2D), reinterpret_cast<void **>(&back)))) return;
        D3D11_TEXTURE2D_DESC described = {};
        back->GetDesc(&described);
        described.Usage = D3D11_USAGE_STAGING;
        described.BindFlags = 0u;
        described.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
        described.MiscFlags = 0u;
        ID3D11Texture2D *staging = nullptr;
        if (SUCCEEDED(device_->CreateTexture2D(&described, nullptr, &staging))) {
            context_->CopyResource(staging, back);
            D3D11_MAPPED_SUBRESOURCE mapped = {};
            if (SUCCEEDED(context_->Map(staging, 0u, D3D11_MAP_READ, 0u, &mapped))) {
                const auto *rows = static_cast<const uint8_t *>(mapped.pData);
                centre_ = *reinterpret_cast<const uint32_t *>(rows + (height_ / 2u) * mapped.RowPitch + (width_ / 2u) * 4u);
                corner_ = *reinterpret_cast<const uint32_t *>(rows + 4u * mapped.RowPitch + 4u * 4u);
                context_->Unmap(staging, 0u);
            }
            staging->Release();
        }
        back->Release();
    }

    void pump()
    {
        MSG message = {};
        while (PeekMessageW(&message, nullptr, 0u, 0u, PM_REMOVE)) {
            TranslateMessage(&message);
            DispatchMessageW(&message);
        }
    }

    uint32_t frames_wanted()
    {
        wchar_t value[16] = {};
        const DWORD length = GetEnvironmentVariableW(L"RVK_SAMPLE_FRAMES", value, 16u);
        uint32_t frames = 0u;
        for (DWORD index = 0u; index < length && value[index] >= L'0' && value[index] <= L'9'; index++)
            frames = frames * 10u + static_cast<uint32_t>(value[index] - L'0');
        return frames != 0u ? frames : 600u;
    }

    void say(const char *text)
    {
        DWORD length = 0u, wrote = 0u;
        while (text[length] != '\0') length++;
        WriteFile(GetStdHandle(STD_OUTPUT_HANDLE), text, length, &wrote, nullptr);
    }
    void number(uint32_t value)
    {
        char digits[12] = {};
        int at = 10;
        do { digits[at--] = static_cast<char>('0' + value % 10u); value /= 10u; } while (value != 0u && at >= 0);
        say(digits + at + 1);
    }
    void hex(uint32_t value)
    {
        char digits[9] = {};
        for (int at = 7; at >= 0; at--, value >>= 4u) digits[at] = "0123456789ABCDEF"[value & 0xFu];
        say(digits);
    }

    HWND                    window_ = nullptr;
    IDXGISwapChain         *swapchain_ = nullptr;
    ID3D11Device           *device_ = nullptr;
    ID3D11DeviceContext    *context_ = nullptr;
    ID3D11RenderTargetView *target_ = nullptr;
    ID3D11VertexShader     *vertex_ = nullptr;
    ID3D11PixelShader      *pixel_ = nullptr;
    ID3D11InputLayout      *layout_ = nullptr;
    ID3D11Buffer           *vertices_ = nullptr;
    ID3D11Buffer           *constants_ = nullptr;
    UINT                    width_ = 0u, height_ = 0u;
    uint32_t                centre_ = 0u, corner_ = 0u;
};

}  // namespace

extern "C" void rvkd3d11sample_entry() { ExitProcess(static_cast<UINT>(Sample().run())); }
