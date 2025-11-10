#ifndef DQCIR_EXT_HPP
#define DQCIR_EXT_HPP

#include <string>
#include <unordered_map>
#include <vector>


#include "abc_wrapper/abc_wrapper.hpp"
#include "common/dqdimacs.fwd.hpp"
#include "common/verilog.hpp"

class dqcir_ext {
   public:
    dqcir_ext();

    // Initialize from dqcir_ext file
    void from_file(std::string path, bool rename = false);
    std::unordered_map<std::string, std::string> rename_map;

    // Initialize from dqdimacs file
    void from_file(dqdimacs& p);

    // Print problem info
    void print_stat(bool detailed = false);

    // Universal variables, (name)
    std::vector<std::string> u_vars;

    // Existential variables, (name, dependency set)
    std::vector<std::pair<std::string, std::vector<std::string>>> e_vars;

    std::vector<Gate> common_phi;
    std::unordered_map<std::string, std::unordered_map<std::string, std::vector<Gate>>> phi;

    // Print to file
    void to_file(std::string path);
};

void optimize(dqcir_ext& p, ABC_wrapper& abc);

#endif