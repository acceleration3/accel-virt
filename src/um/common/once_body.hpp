#ifndef AV_UM_ONCE_BODY_HPP
#define AV_UM_ONCE_BODY_HPP

namespace av {

class OnceBody {
public:
    virtual void run_once() = 0;

protected:
    constexpr OnceBody() = default;
    ~OnceBody() = default;
};

}

#endif
