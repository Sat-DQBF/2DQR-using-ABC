#ifndef DQDIMACS_HPP
#define DQDIMACS_HPP

#include "common/dqcir_ext.fwd.hpp"
#include "common/dqcir.fwd.hpp"

#include <string>
#include <vector>

struct Literal {
    uint64_t var;
    bool sign;
};

typedef std::vector<Literal> Clause;

// Data structure for DQBF in circuit form
class dqdimacs {
   public:
    // Number of variables
    uint64_t var_cnt;

    dqdimacs();

    // Initialize from dqdimacs file
    void from_file(std::string path);

    // Initialize from dqcir_ext file
    void from_file(dqcir_ext& p);

    // Initialize from dqcir file
    void from_file(dqcir& p);

    // Print problem info
    void print_stat(bool detailed = false);

    // Universal variables, (name)
    std::vector<uint64_t> u_vars;

    // Existential variables, (name, dependency set)
    std::vector<std::pair<uint64_t, std::vector<uint64_t>>> e_vars;

    std::vector<Clause> phi;

    // Print to file
    void to_file(std::string path);
};
#endif