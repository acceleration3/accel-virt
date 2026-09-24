/* What a queue's or command list's block names: the device that made the
 * object and the recorder its commands go through, from the header every
 * record starts with (d3d12_records.hpp). The runtime calls those tables'
 * slots with the object's handle alone, so this is how a thunk reaches them -
 * one read, no search. */
#ifndef AV_UM_D3D12_RECORD_HANDLE_HPP
#define AV_UM_D3D12_RECORD_HANDLE_HPP

namespace av::d3d12 {

class D3d12Device;
class D3d12Recorder;

class D3d12RecordHandle {
public:
    explicit D3d12RecordHandle(void *block);
    D3d12Device   &device() const;
    D3d12Recorder &recorder() const;

private:
    void *block_;
};

}  // namespace av::d3d12

#endif  // AV_UM_D3D12_RECORD_HANDLE_HPP
