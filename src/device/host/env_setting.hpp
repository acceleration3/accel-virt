/* One of the host process's environment variables: the debug switches the
 * device and the executor read (AV_PERFETTO, AV_TRACE_SCOPES, AV_PTRACE,
 * AV_VK_SUBMIT_TIMING) and the paths and lists they are given (AV_D3D_PATH,
 * AV_TRACE_DEPTH, AV_TRACE_ONLY). A switch is on when its variable is set and
 * starts with '1', as every one of them is documented - read the same way
 * everywhere, so AV_PERFETTO=0 turns nothing on rather than some readers'
 * half of it. Read once, when it is made. */
#ifndef AV_ENV_SETTING_HPP
#define AV_ENV_SETTING_HPP

namespace av {

class EnvSetting {
public:
    explicit EnvSetting(const char *name);

    /* Set, and starting with '1'. */
    bool        on() const;
    /* What it was set to; null when it was not. */
    const char *text() const;

private:
    const char *value_;
};

}  // namespace av

#endif  // AV_ENV_SETTING_HPP
