// avinspector: a live view of an accel-virt stack on this host.
//
//   avinspector [--source DIR] [--shm DIR] [--interval MS] [--mcp-port PORT] [--seconds N]
//   avinspector --mcp [--source DIR] [--shm DIR] [--interval MS]
//
// The window by default; --mcp-port also serves the MCP tools (data/McpTools.h) on 127.0.0.1:PORT,
// one newline-delimited JSON-RPC session per connection. --mcp runs no window at all: the tools on
// stdin/stdout, for an MCP client that launches the inspector itself (`claude mcp add`).
// --source names the tree whose AV_SCOPE names the trace tables are read with (default: the tree
// the inspector was built beside); --shm where the tables and state sockets are (/dev/shm).

#include "application/App.h"
#include "application/modules/RendererModule.h"
#include "application/modules/UIModule.h"
#include "application/modules/WindowModule.h"
#include "data/McpTools.h"
#include "data/Model.h"
#include "data/StdioTransport.h"
#include "mcp/TcpTransport.h"
#include "text/Format.h"
#include "text/NumberConversion.h"
#include "threading/Thread.h"
#include "threading/ThisThread.h"
#include "ui/Inspector.h"

#include <stdio.h>
#include <string.h>

namespace {

int Usage() {
    fprintf(stderr, "usage: avinspector [--source DIR] [--shm DIR] [--interval MS] [--mcp-port PORT] [--seconds N]\n"
                    "       avinspector --mcp [--source DIR] [--shm DIR] [--interval MS]\n");
    return 2;
}

// No window: the model folds rounds in on a thread of its own while the tools serve stdio.
int ServeStdio(inspector::Model& model) {
    model.Start();
    Atomic<bool> stop{false};
    Thread pump([&] {
        while (!stop.Load()) {
            model.Update();
            ThisThread::SleepFor(Milliseconds<>(20));
        }
    });
    inspector::McpTools tools(model);
    mcp::Server server(inspector::McpTools::Info());
    tools.Register(server);
    inspector::StdioTransport transport;
    server.Serve(transport);   // until the client closes stdin
    stop.Store(true);
    pump.Join();
    model.Stop();
    return 0;
}

} // namespace

int main(int argc, char** argv) {
    inspector::ModelOptions options;
    double seconds = 0.0;
    bool stdio_mcp = false;
    int mcp_port = 0;
    const char* font_path = "/usr/share/fonts/noto/NotoSans-Regular.ttf";
    const char* mono_path = "/usr/share/fonts/noto/NotoSansMono-Regular.ttf";
    for (int i = 1; i < argc; ++i) {
        const bool has_value = i + 1 < argc;
        if (strcmp(argv[i], "--source") == 0 && has_value) {
            options.source_root = argv[++i];
        } else if (strcmp(argv[i], "--shm") == 0 && has_value) {
            options.shm_directory = argv[++i];
        } else if (strcmp(argv[i], "--interval") == 0 && has_value) {
            options.interval_ms = ParseInt<int>(argv[++i]).ValueOr(250);
        } else if (strcmp(argv[i], "--seconds") == 0 && has_value) {
            seconds = ParseDouble(argv[++i]).ValueOr(0.0);
        } else if (strcmp(argv[i], "--font") == 0 && has_value) {
            font_path = argv[++i];
        } else if (strcmp(argv[i], "--mcp") == 0) {
            stdio_mcp = true;
        } else if (strcmp(argv[i], "--mcp-port") == 0 && has_value) {
            mcp_port = ParseInt<int>(argv[++i]).ValueOr(0);
        } else {
            return Usage();
        }
    }

    inspector::Model model(options);
    if (stdio_mcp) {
        return ServeStdio(model);
    }

    AppConfig config;
    config.name = "avinspector";
    config.loop_mode = LoopMode::OnDemand;
    // Wake often enough to take each round the sampler publishes, input or not.
    config.min_update_hz = 20.0;
    App app(config);
    WindowConfig window;
    window.title = "avinspector";
    window.size = Extent2I{1600, 1000};
    window.flags |= WindowFlagBit::Resizable;
    app.AddModule<WindowModule>(window);
    app.AddModule<RendererModule>();
    UIModule& ui_module = app.AddModule<UIModule>();

    inspector::Inspector inspector(model);
    inspector::McpTools tools(model);
    mcp::Server server(inspector::McpTools::Info());
    tools.Register(server);
    if (mcp_port > 0) {
        mcp::TcpTransportOptions tcp;
        tcp.endpoint = Endpoint(IpAddress::LoopbackV4(), static_cast<u16>(mcp_port));
        tcp.close_after_reply = false;
        auto transport = mcp::TcpTransport::Listen(tcp);
        if (transport.IsError()) {
            fprintf(stderr, "avinspector: MCP cannot listen on port %d: %s\n", mcp_port,
                    transport.Error().Message().CStr());
            return 1;
        }
        server.Start(RValueCast(transport.Value()));
        inspector.SetMcpStatus(Format("MCP on 127.0.0.1:{}", mcp_port));
    }

    app.AddSystem(Phase::Startup, "inspector.start", [&](App& a, const FrameTime&) {
        if (ui_module.AddFontFromFile(font_path, 15.f, true).IsError()) {
            a.Log(LogLevel::Warning, "avinspector: could not load the UI font; text will be invisible");
        }
        (void)ui_module.AddFontFromFile(mono_path, 14.f, false, true);
        model.Start();
        ui_module.AddBuilder("inspector.ui", [&](ui::Context& ctx, const FrameTime&) {
            inspector.Build(ctx);
            if (inspector.WantsQuit()) {
                a.RequestStop();
            }
        });
    });
    app.AddSystem(Phase::Update, "inspector.sample", [&](App& a, const FrameTime& time) {
        if (model.Update()) {
            a.RequestRedraw();
        }
        if (seconds > 0.0 && time.real_time >= seconds) {
            a.RequestStop();
        }
    });
    const Result<void, Error> result = app.Run();
    server.Stop();
    model.Stop();
    if (result.IsError()) {
        fprintf(stderr, "avinspector: %s\n", result.Error().Message().CStr());
        return 1;
    }
    return 0;
}
