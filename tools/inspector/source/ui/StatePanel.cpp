#include "ui/Inspector.h"

#include "text/Format.h"
#include "text/NumberConversion.h"

namespace inspector {
namespace {

bool Matches(const StateNode& node, StringView filter) {
    if (filter.IsEmpty() || node.path.GetView().FindNoCase(filter) != StringView::Npos) {
        return true;
    }
    for (const StateField& field : node.fields) {
        if (field.key.GetView().FindNoCase(filter) != StringView::Npos ||
            field.value.GetView().FindNoCase(filter) != StringView::Npos) {
            return true;
        }
    }
    return false;
}

DynamicArray<bool> VisibleNodes(const StateDump& dump, StringView filter) {
    const DynamicArray<StateNode>& nodes = dump.GetNodes();
    DynamicArray<bool> visible;
    visible.Resize(nodes.GetSize());
    for (usize i = 0; i < nodes.GetSize(); ++i) {
        visible[i] = false;
    }
    for (usize i = 0; i < nodes.GetSize(); ++i) {
        if (!Matches(nodes[i], filter)) {
            continue;
        }
        for (usize at = i; at != StringView::Npos && !visible[at]; at = nodes[at].parent) {
            visible[at] = true;
        }
    }
    return visible;
}

}

void Inspector::StateNodeTree(ui::Context& ctx, const ProcessSample& process, const StateDump& dump, usize index,
                              const DynamicArray<bool>& visible) {
    const StateNode& node = dump.GetNodes()[index];
    if (!visible[index]) {
        return;
    }
    const String process_key = process.source.Key();
    String& label = ctx.GetScratchString();
    FormatTo(label, "{}{}{}##{}", node.kind, node.index.IsEmpty() ? "" : "/", node.index, node.path);
    String& preview = ctx.GetScratchString();
    for (usize f = 0; f < node.fields.GetSize() && f < 4; ++f) {
        FormatTo(preview, "{}{}={}", f == 0 ? "   " : "  ", node.fields[f].key, node.fields[f].value);
    }
    ui::TreeFlags flags = ui::TreeFlags(ui::TreeFlag::SpanFullWidth);
    if (node.children.IsEmpty() && node.fields.IsEmpty()) {
        flags |= ui::TreeFlag::Leaf;
    }
    if (!m_state_filter.IsEmpty() || node.parent == StringView::Npos) {
        ui::SetNextTreeNodeOpen(ctx, true);
    }
    const bool is_new = m_model.IsNewNode(process_key, node.path);
    const bool open = ui::TreeNode(ctx, label, flags);
    {
        const RectF row = ui::GetLastTreeRowRect(ctx);
        const float label_width = ctx.MeasureTextWidth(StringView(label).Substr(0, StringView(label).Find("##")));
        ctx.DrawTextInRect(preview, RectF{row.Left() + label_width + 34.f, row.Top(), row.Width(), row.Height()},
                           is_new ? palette::kChanged : palette::kMuted);
    }
    if (!open) {
        return;
    }
    for (const StateField& field : node.fields) {
        const String change = m_model.FieldChange(process_key, node.path, field.key, field.value);
        const bool numeric = node.Integer(field.key).HasValue();
        const String series = History::SeriesKey(process_key, node.path, field.key);
        String& line = ctx.GetScratchString();
        FormatTo(line, "{} = {}", field.key, field.value);
        if (numeric) {
            if (const Optional<f64> rate = m_model.RateOf(process_key, node.path, field.key); rate && *rate != 0.0) {
                FormatTo(line, "    ({})", FormatRate(*rate));
            }
        }
        FormatTo(line, "##{}#{}", node.path, field.key);
        const bool pinned = IsPinned(series);
        if (ui::TreeLeaf(ctx, line, pinned) && numeric) {
            TogglePin(series, Format("{} {} {}", process_key, node.path, field.key), false);
        }
        if (!change.IsEmpty()) {
            const RectF row = ui::GetLastTreeRowRect(ctx);
            ctx.DrawRect(RectF{row.Left(), row.Top(), 3.f, row.Height()}, palette::kChanged);
            String& tip = ctx.GetScratchString();
            if (change == StringView("new")) {
                FormatTo(tip, "new this round");
            } else {
                FormatTo(tip, "was {}", change);
            }
            ctx.SetItemTooltip(tip);
        }
    }
    for (const usize child : node.children) {
        StateNodeTree(ctx, process, dump, child, visible);
    }
    ui::TreePop(ctx);
}

void Inspector::StatePanel(ui::Context& ctx) {
    if (!ui::BeginWindow(ctx, "State", &m_show_state)) {
        ui::EndWindow(ctx);
        return;
    }
    const ProcessSample* process = Selected();
    if (process == nullptr) {
        ui::TextDisabled(ctx, "Choose a process.");
        ui::EndWindow(ctx);
        return;
    }
    ui::InputTextOptions filter;
    filter.hint = "filter: path, key or value";
    ui::InputText(ctx, "##filter", m_state_filter, filter);
    if (!process->state) {
        ui::TextColored(ctx, palette::kBad, process->state_error.IsEmpty() ? String("no dump") : process->state_error);
        ui::EndWindow(ctx);
        return;
    }
    const StateDump& dump = *process->state;
    if (!dump.GetError().IsEmpty()) {
        String& error = ctx.GetScratchString();
        FormatTo(error, "The binary answered: {}", dump.GetError());
        ui::TextColored(ctx, palette::kWarn, error);
    }
    ui::TextDisabled(ctx, "Yellow: changed since the last round. Click a number to plot it.");
    const DynamicArray<bool> visible = VisibleNodes(dump, m_state_filter);
    if (ui::BeginPanel(ctx, "dump")) {
        for (const usize root : dump.GetRoots()) {
            StateNodeTree(ctx, *process, dump, root, visible);
        }
        ui::EndPanel(ctx);
    }
    ui::EndWindow(ctx);
}

}
