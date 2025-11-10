#include "common/dqcir_ext.hpp"
#include "common/dqdimacs.hpp"
#include "common/utils.hpp"

#include <algorithm>
#include <set>
#include <string>

void dqcir_ext::from_file(dqdimacs& p) {
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
    // Sort by e_vars
    auto get_e_vars = [&](const Clause& c) {
        std::vector<int> e_vars;
        for (auto& v : c) {
            if (e_vars_set.find(v.var) != e_vars_set.end()) {
                e_vars.push_back(v.var);
            }
        }
        std::sort(e_vars.begin(), e_vars.end());
        return e_vars;
    };
    auto cmp = [&](const Clause a, const Clause b) {
        std::vector<int> a_e = get_e_vars(a);
        std::vector<int> b_e = get_e_vars(b);
        return a_e < b_e;
    };
    std::sort(p.phi.begin(), p.phi.end(), cmp);

    for (auto& clause : p.phi) {
        std::vector<int> es = get_e_vars(clause);
        if (es.size() > 2) {
            print_error("The number of existential variables in a clause is more than 2");
        }
        if (es.size() == 1) {
            for (auto& v : p.e_vars) {
                if (v.first != es[0]) {
                    es.push_back(v.first);
                }
            }
        }
        std::string y0 = "e" + std::to_string(es[0]);
        std::string y1 = "e" + std::to_string(es[1]);

        std::vector<Gate>* curr_phi = &(phi[y0][y1]);

        curr_phi->push_back({"w" + uitoh(curr_phi->size() + 1), "|", {}});
        for (auto& v : clause) {
            if (e_vars_set.find(v.var) != e_vars_set.end()) {
                curr_phi->back().inputs.push_back({"e" + std::to_string(v.var), v.sign});
            } else {
                curr_phi->back().inputs.push_back({"u" + std::to_string(v.var), v.sign});
            }
        }
    }


    for (auto& [y0, m] : phi) {
        for (auto& [y1, gates] : m) {
            if (gates.size() != 0) {
                Gate out = {y0 + "_" + y1 + "_out", "&", {}};
                for (auto& g: gates) {
                    g.inputs.push_back({g.name, true});
                }
                gates.push_back(out);
            }
        }
    }
}