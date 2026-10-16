/* One present's frame, stageable through its API's pool (StagedFrame): what
 * SharedSurfaces asks to stage a frame only when one is wanted - every present
 * for Direct3D 9 and 11, and for Direct3D 12 once a blt has read the surface,
 * since its copy is a GPU copy on the presenting queue. Virtual because each
 * Direct3D runtime's is a real implementation. */
#ifndef AV_FRAME_STAGER_HPP
#define AV_FRAME_STAGER_HPP

namespace av {

class StagedFrame;

class FrameStager {
public:
    FrameStager(const FrameStager &) = delete;
    FrameStager &operator=(const FrameStager &) = delete;

    /* The frame staged, out until the present engine gives it back; null
     * when the API cannot stage it - a source not a 2D image of four-byte
     * pixels, no frame free - which is a frame not shown. */
    virtual StagedFrame *stage() = 0;

protected:
    FrameStager() = default;
    ~FrameStager() = default;
};

}  // namespace av

#endif  // AV_FRAME_STAGER_HPP
