/* The one pointer this ICD's entry points reach the root through
 * (constitution III).
 *
 * The Khronos loader resolves one symbol by name and then dispatches through
 * a table of bare function pointers, every one of them called with OpenCL's
 * own arguments and nowhere to put a `this`. ClIcdRoot binds itself when
 * constructed and unbinds when destroyed, so a test binds the root under test
 * and the DLL binds the one in its storage. */
#ifndef AV_CLICD_CL_ICD_BINDING_HPP
#define AV_CLICD_CL_ICD_BINDING_HPP

namespace av::clicd {

class ClIcdRoot;

class ClIcdBinding {
public:
    void       bind(ClIcdRoot &root);
    void       unbind();
    bool       bound() const;
    ClIcdRoot &root() const;

private:
    ClIcdRoot *root_ = nullptr;
};

extern ClIcdBinding cl_icd_binding;

}  // namespace av::clicd

#endif  // AV_CLICD_CL_ICD_BINDING_HPP
