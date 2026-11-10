#ifndef AV_CLICD_CL_OPTION_JUDGE_HPP
#define AV_CLICD_CL_OPTION_JUDGE_HPP

namespace av::clicd {

class ClOptionJudge {
public:
    explicit ClOptionJudge(const char *options);

    bool valid() const;

private:
    bool token_valid(const char *start, unsigned length, bool &takes_next) const;

    const char *options_;
};

}

#endif
