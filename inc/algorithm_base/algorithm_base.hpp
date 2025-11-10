#ifndef ALGORITHM_BASE_HPP
#define ALGORITHM_BASE_HPP

#include <unordered_map>
#include <unordered_set>

#include "abc_wrapper/abc_wrapper.hpp"
#include "common/verilog.hpp"

class Algorithm
{
public:
    struct options {
        bool gen_skolem = false;
        bool fraig = true;
        bool keep_aux = false;
        bool transition_relation = true;
    };

    options opt;

    VerilogModule verilog;
    ABC_wrapper& abc;

    size_t max_dep_size;
    size_t min_dep_size;
    std::unordered_map<std::string, int> idx_lookup;

    Wire transition;
    Wire neg_property;

    int patch_cnt;
    std::unordered_set<std::string> patch_set;
    
    Algorithm(ABC_wrapper& abc) : abc(abc) {};
    virtual std::string to_string() = 0;
    virtual bool refine() = 0;
    virtual void patch(std::unordered_map<std::string, bool>& counterexample) = 0;
    void run();
};

#endif
