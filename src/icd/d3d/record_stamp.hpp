/* What heads every block a Direct3D runtime allocates for an object this
 * driver makes (pfnCalcPrivate*Size): the tag saying what kind of record the
 * block holds and the 32-bit id the host knows the object by. The block is
 * the runtime's memory, written by assignment and never constructed there, and
 * another component can scribble on it - so a slot that must know what it was
 * handed asks the stamp before it reads a field of the record. */
#ifndef AV_D3D_RECORD_STAMP_HPP
#define AV_D3D_RECORD_STAMP_HPP

#include <stdint.h>

namespace av::d3d {

class RecordStamp {
public:
    /* Unstamped: holds no kind of record, names no object. */
    RecordStamp();
    RecordStamp(uint32_t tag, uint32_t id);

    /* Whether the block holds a record of the kind `tag` names. */
    bool     holds(uint32_t tag) const;
    /* The host's name for the object; 0 for a record the host never sees. */
    uint32_t id() const;

private:
    uint32_t tag_;
    uint32_t id_;
};

}  // namespace av::d3d

#endif  // AV_D3D_RECORD_STAMP_HPP
