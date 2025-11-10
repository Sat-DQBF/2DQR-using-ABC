#ifndef TWODQR_EXT_HPP
#define TWODQR_EXT_HPP

#include "algorithm_base/algorithm_base.hpp"
#include "abc_wrapper/abc_wrapper.hpp"
#include "common/dqcir_ext.hpp"

class twoDQR_ext : public Algorithm 
{
public:
    dqcir_ext& p;

    twoDQR_ext(dqcir_ext& instance, ABC_wrapper& abc);
    std::string to_string();
    bool refine();
    void patch(std::unordered_map<std::string, bool>& counterexample);
};

#endif