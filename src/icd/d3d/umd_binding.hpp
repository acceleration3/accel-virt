/* The one pointer a DDI table thunk reaches the root through. A thunk is a
 * bare function pointer the Direct3D runtime calls with its own argument
 * structure and nothing else, so - like MiniportBinding for the context-less
 * DDIs, and IcdBinding for the Vulkan loader's entry points - this is the D3D
 * user-mode driver's process state (constitution III), defined once beside the
 * thunks.
 *
 * UmdRoot binds itself here when it is constructed and unbinds when it is
 * destroyed: in the DLL that is the root in the exports' ObjectStorage, and in
 * a test it is the root under test. Every thunk reached through a table this
 * root handed out, so none asks with nothing bound. */
#ifndef AV_D3D_UMD_BINDING_HPP
#define AV_D3D_UMD_BINDING_HPP

namespace av::d3d {

class UmdRoot;

class UmdBinding {
public:
    void     bind(UmdRoot &root);
    void     unbind();
    bool     bound() const;
    UmdRoot &root() const;

private:
    UmdRoot *root_ = nullptr;
};

extern UmdBinding umd_binding;

}  // namespace av::d3d

#endif  // AV_D3D_UMD_BINDING_HPP
