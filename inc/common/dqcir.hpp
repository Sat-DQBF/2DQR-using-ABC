#ifndef DQCIR_HPP
#define DQCIR_HPP

#include <string>
#include <unordered_map>
#include <vector>

#include "common/dqdimacs.fwd.hpp"
#include "common/dqcir_ext.fwd.hpp"
#include "common/verilog.hpp"

// Data structure for DQBF in circuit form
class dqcir {
   public:
    // Number of variables
    uint64_t var_cnt;

    dqcir();

    // Initialize from dqcir file
    void from_file(std::string path, bool rename = false);
    std::unordered_map<std::string, std::string> rename_map;

    // Initialize from dqdimacs file
    void from_file(dqdimacs& p);

    // Initialize from dqcir_ext file
    void from_file(dqcir_ext& p);

    // Print problem info
    void print_stat(bool detailed = false);

    // Universal variables, (name)
    std::vector<std::string> u_vars;

    // Existential variables, (name, dependency set)
    std::vector<std::pair<std::string, std::vector<std::string>>> e_vars;

    // Output var
    std::string output_var;

    std::vector<Gate> phi;

    // Print to file
    void to_file(std::string path);
};
#endif