#include "ui/Inspector.h"

#include "core/Pair.h"
#include "data/Clock.h"
#include "threading/LockGuard.h"
#include "text/Format.h"

namespace inspector {

Inspector::Inspector(Model& model) : m_model(model) {}

Inspector::~Inspector() = default;

const ProcessSample* Inspector::Selected() {
    const Optional<Sample>& current = m_model.GetCurrent();
    if (!current) {
        return nullptr;
    }
    if (m_selected.IsEmpty() || current->Find(m_selected) == nullptr) {
        for (const ProcessSample& process : current->processes) {
            if (process.source.alive) {
                m_selected = process.source.Key();
                break;
            }
        }
    }
    return current->Find(m_selected);
}

bool Inspector::IsPinned(StringView series) const noexcept {
    for (const PinnedPlot& plot : m_pinned) {
        if (plot.series.GetView() == series) {
            return true;
        }
    }
    return false;
}

void Inspector::TogglePin(String series, String label, bool rate) {
    for (usize i = 0; i < m_pinned.GetSize(); ++i) {
        if (m_pinned[i].series == series) {
            m_pinned.RemoveAt(i);
            return;
        }
    }
    m_pinned.PushBack(PinnedPlot{RValueCast(series), RValueCast(label), rate});
    m_show_plots = true;
}

void Inspector::Build(ui::Context& ctx) {
    LockGuard<Mutex> hold(m_model.GetMutex());
    (void)Selected();
    MenuBar(ctx);
    const RectF status = ctx.ReserveWorkRectBottom(ctx.GetVar(ui::StyleVar::MenuBarHeight));
    ui::DockSpace(ctx, "workspace", ctx.GetWorkRect());
    if (!m_dock_built) {
        ui::DockBuilder dock(ctx, "workspace");
        dock.Dock("Threads", "", ui::DockSide::Center);
        dock.Dock("Processes", "Threads", ui::DockSide::Left, 0.24f);
        dock.Dock("State", "Threads", ui::DockSide::Right, 0.38f);
        dock.Dock("Plots", "Threads", ui::DockSide::Bottom, 0.5f);
        dock.Dock("Surfaces", "Plots", ui::DockSide::Center);
        dock.Dock("Connections", "Plots", ui::DockSide::Center);
        m_dock_built = true;
    }
    if (m_show_processes) ProcessesPanel(ctx);
    if (m_show_threads) ThreadsPanel(ctx);
    if (m_show_state) StatePanel(ctx);
    if (m_show_connections) ConnectionsPanel(ctx);
    if (m_show_surfaces) SurfacesPanel(ctx);
    if (m_show_plots) PlotsPanel(ctx);
    if (ui::BeginModal(ctx, "About avinspector", &m_show_about)) {
        ui::Text(ctx, "avinspector - a live view of an accel-virt stack");
        ui::TextDisabled(ctx, "Trace tables and state dumps.");
        String& line = ctx.GetScratchString();
        FormatTo(line, "{} scope names from {}", m_model.GetNames().GetSize(), m_model.GetSourceRoot());
        ui::TextDisabled(ctx, line);
        if (ui::Button(ctx, "Close")) {
            ui::CloseCurrentPopup(ctx);
        }
        ui::EndModal(ctx);
    }
    StatusBar(ctx, status);
}

void Inspector::MenuBar(ui::Context& ctx) {
    if (!ui::BeginMainMenuBar(ctx)) {
        return;
    }
    if (ui::BeginMenu(ctx, "File")) {
        if (ui::MenuItem(ctx, "Quit", "Ctrl+Q")) {
            m_quit = true;
        }
        ui::EndMenu(ctx);
    }
    if (ui::BeginMenu(ctx, "View")) {
        ui::MenuItemOptions item;
        const Pair<StringView, bool*> panels[] = {
            {"Processes", &m_show_processes}, {"Threads", &m_show_threads},   {"State", &m_show_state},
            {"Connections", &m_show_connections}, {"Surfaces", &m_show_surfaces}, {"Plots", &m_show_plots},
        };
        for (const auto& panel : panels) {
            item.checked = panel.second;
            ui::MenuItem(ctx, panel.first, item);
        }
        ui::EndMenu(ctx);
    }
    if (ui::BeginMenu(ctx, "Sampling")) {
        if (ui::MenuItem(ctx, m_model.GetSampler().IsPaused() ? "Resume" : "Pause", "Space")) {
            m_model.GetSampler().SetPaused(!m_model.GetSampler().IsPaused());
        }
        if (ui::MenuItem(ctx, "Sample now", "F5")) {
            m_model.GetSampler().RefreshNow();
        }
        ui::MenuSeparator(ctx);
        const int intervals[] = {100, 250, 500, 1000, 2000};
        for (const int interval : intervals) {
            String& label = ctx.GetScratchString();
            FormatTo(label, "Every {} ms", interval);
            bool chosen = m_model.GetSampler().GetIntervalMs() == interval;
            ui::MenuItemOptions item;
            item.checked = &chosen;
            if (ui::MenuItem(ctx, label, item)) {
                m_model.GetSampler().SetIntervalMs(interval);
            }
        }
        ui::MenuSeparator(ctx);
        if (ui::MenuItem(ctx, "Clear history")) {
            m_model.GetHistory().Clear();
        }
        ui::EndMenu(ctx);
    }
    if (ui::BeginMenu(ctx, "Help")) {
        if (ui::MenuItem(ctx, "About")) {
            m_show_about = true;
        }
        ui::EndMenu(ctx);
    }
    ui::EndMainMenuBar(ctx);
}

void Inspector::StatusBar(ui::Context& ctx, RectF rect) {
    String& line = ctx.GetScratchString();
    const Optional<Sample>& current = m_model.GetCurrent();
    if (!current) {
        FormatTo(line, "waiting for the first round...");
    } else {
        usize alive = 0;
        for (const ProcessSample& process : current->processes) {
            alive += process.source.alive ? 1 : 0;
        }
        FormatTo(line, "round {}  |  {} sources ({} alive)  |  sampled {} ago, every {} ms{}  |  {} series  |  {} scope names{}",
                 current->round, current->processes.GetSize(), alive,
                 FormatSeconds(m_model.SecondsSince(current->taken)), m_model.GetSampler().GetIntervalMs(),
                 m_model.GetSampler().IsPaused() ? " (paused)" : "", m_model.GetHistory().GetSize(),
                 m_model.GetNames().GetSize(), m_mcp_status.IsEmpty() ? String() : Format("  |  {}", m_mcp_status));
    }
    ctx.DrawRect(rect, ctx.GetColor(ui::ColorRole::MenuBarBg));
    ctx.DrawTextInRect(line, rect.Inflate(-6.f, 0.f), ctx.GetColor(ui::ColorRole::TextDisabled));
}

String FormatBytes(u64 bytes) {
    const char* units[] = {"B", "KiB", "MiB", "GiB", "TiB"};
    f64 value = static_cast<f64>(bytes);
    usize unit = 0;
    while (value >= 1024.0 && unit + 1 < sizeof units / sizeof units[0]) {
        value /= 1024.0;
        ++unit;
    }
    return unit == 0 ? Format("{} B", bytes) : Format("{:.1f} {}", value, units[unit]);
}

String FormatSeconds(f64 seconds) {
    if (seconds < 1.0) {
        return Format("{} ms", static_cast<int>(seconds * 1000.0));
    }
    if (seconds < 60.0) {
        return Format("{:.1f} s", seconds);
    }
    const int whole = static_cast<int>(seconds);
    return Format("{} m {} s", whole / 60, whole % 60);
}

String FormatRate(f64 per_second) {
    if (per_second >= 1e6) {
        return Format("{:.1f} M/s", per_second / 1e6);
    }
    if (per_second >= 1e3) {
        return Format("{:.1f} k/s", per_second / 1e3);
    }
    return Format("{:.1f}/s", per_second);
}

}
