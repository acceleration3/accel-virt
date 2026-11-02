#ifndef UI_INSPECTOR_H
#define UI_INSPECTOR_H

#include "containers/DynamicArray.h"
#include "containers/HashMap.h"
#include "containers/String.h"
#include "core/Optional.h"
#include "core/Types.h"
#include "data/Model.h"
#include "graphics/Color.h"
#include "ui/UI.h"

namespace inspector {

class Inspector {
public:
    explicit Inspector(Model& model);
    ~Inspector();
    Inspector(const Inspector&) = delete;
    Inspector& operator=(const Inspector&) = delete;
    Inspector(Inspector&&) = delete;
    Inspector& operator=(Inspector&&) = delete;

    void Build(ui::Context& ctx);
    bool WantsQuit() const noexcept { return m_quit; }
    void SetMcpStatus(String status) { m_mcp_status = RValueCast(status); }

private:
    struct PinnedPlot {
        String series;
        String label;
        bool rate = false;
    };
    void MenuBar(ui::Context& ctx);
    void StatusBar(ui::Context& ctx, RectF rect);
    void ProcessesPanel(ui::Context& ctx);
    void ThreadsPanel(ui::Context& ctx);
    void StatePanel(ui::Context& ctx);
    void ConnectionsPanel(ui::Context& ctx);
    void SurfacesPanel(ui::Context& ctx);
    void PlotsPanel(ui::Context& ctx);

    const ProcessSample* Selected();
    bool IsPinned(StringView series) const noexcept;
    void TogglePin(String series, String label, bool rate);
    void StateNodeTree(ui::Context& ctx, const ProcessSample& process, const StateDump& dump, usize node,
                       const DynamicArray<bool>& visible);

    Model& m_model;
    DynamicArray<PinnedPlot> m_pinned;
    String m_mcp_status;

    String m_selected;
    u32 m_selected_thread = 0;
    String m_state_filter;
    String m_selected_context;
    bool m_only_active = false;
    float m_stall_seconds = 2.f;
    bool m_show_processes = true, m_show_threads = true, m_show_state = true, m_show_connections = true,
         m_show_surfaces = true, m_show_plots = true, m_show_about = false;
    bool m_dock_built = false;
    bool m_quit = false;
};

namespace palette {
inline constexpr graphics::Color kGood{110, 200, 120, 255};
inline constexpr graphics::Color kWarn{235, 170, 70, 255};
inline constexpr graphics::Color kBad{235, 90, 80, 255};
inline constexpr graphics::Color kInfo{110, 170, 240, 255};
inline constexpr graphics::Color kChanged{240, 220, 110, 255};
inline constexpr graphics::Color kMuted{140, 140, 150, 255};
}

String FormatBytes(u64 bytes);
String FormatSeconds(f64 seconds);
String FormatRate(f64 per_second);

}

#endif
