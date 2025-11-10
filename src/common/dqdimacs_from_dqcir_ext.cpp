#include "common/dqcir_ext.hpp"
#include "common/dqdimacs.hpp"
#include "common/utils.hpp"

#include <cstdint>
#include <string>
#include <unordered_map>

// Tseitin Transformation
void dqdimacs::from_file(dqcir_ext& p) {
    std::vector<uint64_t> last_wire;
    int original_var_cnt;
    var_cnt = 0;
    std::unordered_map<std::string, uint64_t> var_map;
    for (auto& u : p.u_vars) {
        var_map[u] = ++var_cnt;
        u_vars.push_back(var_map[u]);
    }

    for (auto& [y, v] : p.e_vars) {
        var_map[y] = ++var_cnt;
        e_vars.push_back({var_map[y], {}});
        for (auto& u : v) {
            e_vars.back().second.push_back(var_map[u]);
        }
    }

    original_var_cnt = var_cnt;

    for (auto& g : p.common_phi) {
        var_map[g.name] = ++var_cnt;
        if (g.operation == "&") {
            // w = and(c1, c2, c3, ...)
            // ~c1 | ~c2 | ~c3 | ... | w
            {
                Clause c;
                c.push_back({var_map[g.name], true});
                for (auto& u : g.inputs) {
                    c.push_back({var_map[u.name], !u.sign});
                }
                phi.push_back(c);
            }
            // ~w | c1
            for (auto& u : g.inputs) {
                Clause c;
                c.push_back({var_map[g.name], false});
                c.push_back({var_map[u.name], u.sign});
                phi.push_back(c);
            }
        } else if (g.operation == "|") {
            // w = or(c1, c2, c3, ..)
            // ~w | c1 | c2 | c3 | ...
            {
                Clause c;
                c.push_back({var_map[g.name], false});
                for (auto& u : g.inputs) {
                    c.push_back({var_map[u.name], u.sign});
                }
                phi.push_back(c);
            }

            // ~c1 | w
            for (auto& u : g.inputs) {
                Clause c;
                c.push_back({var_map[g.name], true});
                c.push_back({var_map[u.name], !u.sign});
                phi.push_back(c);
            }
        } else {
            print_error(("Unsupported operation: " + g.operation).c_str());
        }
    }

    for (auto& [y0, v] : p.phi) {
        for (auto& [y1, gates] : v) {
            if (gates.size() == 0) {
                continue;
            }

            for (auto& g : gates) {
                var_map[g.name] = ++var_cnt;
                if (g.operation == "&") {
                    // w = and(c1, c2, c3, ...)
                    // ~c1 | ~c2 | ~c3 | ... | w
                    {
                        Clause c;
                        c.push_back({var_map[g.name], true});
                        for (auto& u : g.inputs) {
                            c.push_back({var_map[u.name], !u.sign});
                        }
                        phi.push_back(c);
                    }
                    // ~w | c1
                    for (auto& u : g.inputs) {
                        Clause c;
                        c.push_back({var_map[g.name], false});
                        c.push_back({var_map[u.name], u.sign});
                        phi.push_back(c);
                    }
                } else if (g.operation == "|") {
                    // w = or(c1, c2, c3, ..)
                    // ~w | c1 | c2 | c3 | ...
                    {
                        Clause c;
                        c.push_back({var_map[g.name], false});
                        for (auto& u : g.inputs) {
                            c.push_back({var_map[u.name], u.sign});
                        }
                        phi.push_back(c);
                    }

                    // ~c1 | w
                    for (auto& u : g.inputs) {
                        Clause c;
                        c.push_back({var_map[g.name], true});
                        c.push_back({var_map[u.name], !u.sign});
                        phi.push_back(c);
                    }
                } else {
                    print_error(("Unsupported operation: " + g.operation).c_str());
                }
            }
            last_wire.push_back(var_map[gates.back().name]);
        }
    }

    uint64_t out_var = ++var_cnt;
    // out_var = and(last_wire)
    {
        Clause c;
        c.push_back({out_var, true});
        for (auto& w : last_wire) {
            c.push_back({w, false});
        }
        phi.push_back(c);
    }
    for (auto& w : last_wire) {
        Clause c;
        c.push_back({w, true});
        c.push_back({out_var, false});
        phi.push_back(c);
    }
    phi.push_back({{out_var, true}});

    for (int i = original_var_cnt + 1; i <= var_cnt; i++) {
        e_vars.push_back({i, std::vector<uint64_t>(u_vars)});
    }
}