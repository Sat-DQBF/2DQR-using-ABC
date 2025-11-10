#include "common/dqcir.hpp"
#include "common/dqdimacs.hpp"
#include "common/utils.hpp"

#include <set>
#include <string>

void dqcir::from_file(dqdimacs& p) {
    for (auto u : p.u_vars) {
        u_vars.push_back("u" + std::to_string(u));
    }
    std::set<int> e_vars_set;
    for (auto v : p.e_vars) {
        e_vars.emplace_back("e" + std::to_string(v.first), std::vector<std::string>());
        e_vars_set.insert(v.first);
        for (auto u : v.second) {
            e_vars.back().second.push_back("u" + std::to_string(u));
        }
    }
    output_var = "out";
    for (auto& clause : p.phi) {
        Gate g({"w" + uitoh(phi.size()), "|", {}});
        for (auto& lit : clause) {
            if (e_vars_set.find(lit.var) != e_vars_set.end()) {
                g.inputs.push_back({"e" + std::to_string(lit.var), lit.sign});
            } else {
                g.inputs.push_back({"u" + std::to_string(lit.var), lit.sign});
            }
        }
        phi.push_back(g);
    }

    Gate out({"out", "&", {}});
    for (auto& g : phi) {
        out.inputs.push_back({g.name, true});
    }
    phi.push_back(out);
}