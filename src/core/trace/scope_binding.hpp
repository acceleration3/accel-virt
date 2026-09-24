/* The trace table AV_SCOPE pushes onto. A function cannot be handed its
 * binary's table - every function in src/ opens a scope - so each binary binds
 * its table here once its root exists (constitution III lists this pointer).
 * A table is unbound only when no thread can still be inside the binary:
 * DriverUnload, DLL detach at process exit, the device module's end. Unbound,
 * AV_SCOPE does nothing, which is what tests and fuzz targets see. */
#ifndef AV_SCOPE_BINDING_HPP
#define AV_SCOPE_BINDING_HPP

namespace av {

class TraceTable;

class ScopeBinding {
public:
    constexpr ScopeBinding() = default;
    ScopeBinding(const ScopeBinding &) = delete;
    ScopeBinding &operator=(const ScopeBinding &) = delete;

    void        bind(TraceTable &table);
    void        unbind();
    TraceTable *table() const;

private:
    TraceTable *table_ = nullptr;
};

/* Hidden, so a shared object keeps its own: librvk_executor.so is loaded into
 * a QEMU that exports its symbols. */
#if defined(__ELF__)
[[gnu::visibility("hidden")]]
#endif
extern constinit ScopeBinding scope_binding;

/* Inline: every AV_SCOPE reads it, and an out-of-line call per scope was a
 * third of a busy host worker's time with tracing off. */
inline TraceTable *ScopeBinding::table() const { return table_; }

}  // namespace av

#endif  // AV_SCOPE_BINDING_HPP
