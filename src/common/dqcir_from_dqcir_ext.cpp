#include "common/circuit.hpp"
#include "common/dqcir.hpp"
#include "common/dqcir_ext.hpp"
#include "common/utils.hpp"

#include <string>
#include <unordered_map>

void dqcir::from_file(dqcir_ext& p) {
    e_vars.push_back({"y", {}});
    e_vars.push_back({"yp", {}});

    for (auto& x : p.u_vars) {
        rename_map[x] = "x" + std::to_string(u_vars.size() + 1);
        u_vars.push_back(rename_map[x]);
        e_vars[0].second.push_back(rename_map[x]);
    }
    for (auto& x : p.u_vars) {
        u_vars.push_back(rename_map[x] + "p");
        e_vars[1].second.push_back(rename_map[x] + "p");
    }

    output_var = "RES";
    Gate output_gate({"RES", "&", {}});

    uint32_t log_m = log_2(p.e_vars.size() - 1) + 1;
    for (int i = 0; i < log_m; i++) {
        u_vars.push_back("i" + std::to_string(i));
        e_vars[0].second.push_back("i" + std::to_string(i));
    }
    for (int i = 0; i < log_m; i++) {
        u_vars.push_back("ip" + std::to_string(i));
        e_vars[1].second.push_back("ip" + std::to_string(i));
    }

    // i_eq_{i}
    for (int i = 0; i < p.e_vars.size(); i++) {
        Gate tmp({"i_eq_" + std::to_string(i), "&", {}});
        for (int j = 0; j < log_m; j++) {
            tmp.inputs.push_back({"i" + std::to_string(j), ((1 << (log_m - j - 1)) & i) != 0});
        }
        phi.push_back(tmp);
    }
    // ip_eq_{i}
    for (int i = 0; i < p.e_vars.size(); i++) {
        Gate tmp({"ip_eq_" + std::to_string(i), "&", {}});
        for (int j = 0; j < log_m; j++) {
            tmp.inputs.push_back({"ip" + std::to_string(j), ((1 << (log_m - j - 1)) & i) != 0});
        }
        phi.push_back(tmp);
    }
    auto gen_eq_gates = [&] (std::string i1, std::string i2) {
        phi.push_back({i1 + "_eq_" + i2 + "_0", "|", {{i1, true}, {i2, false}}});
        phi.push_back({i1 + "_eq_" + i2 + "_1", "|", {{i1, false}, {i2, true}}});
        phi.push_back({i1 + "_eq_" + i2, "&", {{i1 + "_eq_" + i2 + "_0", true}, {i1 + "_eq_" + i2 + "_1", true}}});
    };

    // x{i}_eq_x{i}p
    for (auto& v : p.u_vars) {
        gen_eq_gates(rename_map[v], rename_map[v] + "p");
    }
    // y_eq_yp
    gen_eq_gates("y", "yp");

    // i == i' == k & z_k == z_k' -> y == y'
    for (int k = 0; k < p.e_vars.size(); k++) {
        Gate tmp({"dep_y" + std::to_string(k), "|", {}});
        tmp.inputs.push_back({"i_eq_" + std::to_string(k), false});
        tmp.inputs.push_back({"ip_eq_" + std::to_string(k), false});
        for (auto& v : p.e_vars[k].second) {
            tmp.inputs.push_back({rename_map[v] + "_eq_" + rename_map[v] + "p", false});
        }
        tmp.inputs.push_back({"y_eq_yp", true});
        phi.push_back(tmp);
        output_gate.inputs.push_back({"dep_y" + std::to_string(k), true});
    }

    // phi_i_j
    Gate tmp({"phi", "&", {}});
    std::unordered_map<std::string, int> yi_map;
    for (int i = 0; i < p.e_vars.size(); i++) {
        yi_map[p.e_vars[i].first] = i;
    }
    int wire_cnt = 0;
    for (auto& g : p.common_phi) {
        rename_map[g.name] = "w" + std::to_string(++wire_cnt);
        phi.push_back({rename_map[g.name], g.operation, {}});
        for (auto& in : g.inputs) {
            if (rename_map.find(in.name) != rename_map.end()) {
                phi.back().inputs.push_back({rename_map[in.name], in.sign});
            } else {
                print_error(("Unknown input: " + in.name).c_str());
            }
        }
    }

    for (auto& [i, m]: p.phi) {
        for (auto& [j, gates] : m) {
            if (gates.size() == 0) {
                continue;
            }
            rename_map[i] = "y";
            rename_map[j] = "yp";
            for (auto& g : gates) {
                rename_map[g.name] = "w" + std::to_string(++wire_cnt);
                phi.push_back({rename_map[g.name], g.operation, {}});
                for (auto& in : g.inputs) {
                    if (rename_map.find(in.name) != rename_map.end()) {
                        phi.back().inputs.push_back({rename_map[in.name], in.sign});
                    } else {
                        print_error(("Unknown input: " + in.name).c_str());
                    }
                }
            }
            phi.push_back({"phi_" + i + "_" + j, "|", {{"i_eq_" + std::to_string(yi_map[i]), false}, {"ip_eq_" + std::to_string(yi_map[j]), false}, {rename_map[gates.back().name], true}}});
            tmp.inputs.push_back({"phi_" + i + "_" + j, true});
        }
    }
    phi.push_back(tmp);

    // x == x'
    {
        Gate tmp({"x_eq_xp", "&", {}});
        for (auto& v: p.u_vars) {
            tmp.inputs.push_back({rename_map[v] + "_eq_" + rename_map[v] + "p", true});
        }
        phi.push_back(tmp);
    }

    phi.push_back({"phi_sat", "|", {{"x_eq_xp", false}, {"phi", true}}});
    output_gate.inputs.push_back({"phi_sat", true});
    phi.push_back(output_gate);
}