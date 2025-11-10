#ifndef TWODQR_ONE_SIDED_HPP
#define TWODQR_ONE_SIDED_HPP

#include "algorithm_base/algorithm_base.hpp"
#include "common/dqcir.hpp"

class twoDQR : public Algorithm 
{
public:
    dqcir& p;

    twoDQR(dqcir& instance, ABC_wrapper& abc);
    std::string to_string();
    bool refine();
    void patch(std::unordered_map<std::string, bool>& counterexample);

    bool swapped = false;
};

#endif