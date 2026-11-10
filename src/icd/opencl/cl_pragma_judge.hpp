#ifndef AV_CLICD_CL_PRAGMA_JUDGE_HPP
#define AV_CLICD_CL_PRAGMA_JUDGE_HPP

#include <stddef.h>

namespace av::clicd {

class ClPragmaJudge {
public:
    ClPragmaJudge(const char *source, size_t bytes);

    bool enables(const char *extension) const;

private:
    const char *source_;
    size_t      bytes_;
};

}

#endif
