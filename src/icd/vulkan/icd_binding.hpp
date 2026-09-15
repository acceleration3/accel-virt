/* The one pointer the generated wrappers' hooks and the ICD's own entry points
 * reach the root through. Neither can be handed a context: a hook is a free
 * function the 063 emitters call by name, and an entry point is a bare function
 * pointer the loader calls with Vulkan's arguments only. So, like
 * MiniportBinding for the context-less DDIs, this is the Vulkan ICD's process
 * state (constitution III), defined once beside the hooks in icd_hooks.cpp.
 *
 * IcdRoot binds itself here when it is constructed and unbinds when it is
 * destroyed: in the DLL that is the root in the exports' ObjectStorage, and in a
 * test it is the root under test. Every function reaching root() was handed out
 * by a bound root, so none asks with nothing bound. */
#ifndef AV_ICD_BINDING_HPP
#define AV_ICD_BINDING_HPP

namespace av::icd {

class IcdRoot;

class IcdBinding {
public:
    void     bind(IcdRoot &root);
    void     unbind();
    bool     bound() const;
    IcdRoot &root() const;

private:
    IcdRoot *root_ = nullptr;
};

extern IcdBinding icd_binding;

}  // namespace av::icd

#endif  // AV_ICD_BINDING_HPP
