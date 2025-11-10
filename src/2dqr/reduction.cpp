#include "2dqr/twoDQR.hpp"
#include "common/circuit.hpp"
#include "common/dqcir.hpp"
#include "abc_wrapper/abc_wrapper.hpp"
#include "common/utils.hpp"
#include "common/verilog.hpp"

#include <algorithm>
#include <assert.h>
#include <set>
#include <string>
#include <unordered_map>

twoDQR::twoDQR(dqcir& p, ABC_wrapper& abc) : p(p), Algorithm(abc) {
    if (p.e_vars.size() != 2) {
        print_error("2DQR requires exactly 2 existential variables");
    }

    verilog.name = "twoDQR";

    patch_cnt = 0;

    verilog.wires.resize(0);
    max_dep_size = std::max(p.e_vars[0].second.size(), p.e_vars[1].second.size());
    min_dep_size = std::min(p.e_vars[0].second.size(), p.e_vars[1].second.size());

    // target k and target y_k are always 0
    if (min_dep_size != p.e_vars[0].second.size()) {
        std::swap(p.e_vars[0], p.e_vars[1]);
        // std::swap(p.phi.PI[p.u_vars.size()], p.phi.PI[p.u_vars.size() + 1]);
        swapped = true;
    }

    // Precalculations
    std::set<std::string> x(p.u_vars.begin(), p.u_vars.end());
    std::set<std::string> z0(p.e_vars[0].second.begin(), p.e_vars[0].second.end());
    std::set<std::string> z1(p.e_vars[1].second.begin(), p.e_vars[1].second.end());
    std::set<std::string> z0_intersect_z1;
    std::set_intersection(z0.begin(), z0.end(), z1.begin(), z1.end(), std::inserter(z0_intersect_z1, z0_intersect_z1.end()));
    std::set<std::string> z0_minus_z1;
    std::set_difference(z0.begin(), z0.end(), z0_intersect_z1.begin(), z0_intersect_z1.end(), std::inserter(z0_minus_z1, z0_minus_z1.end()));
    std::set<std::string> z1_minus_z0;
    std::set_difference(z1.begin(), z1.end(), z0_intersect_z1.begin(), z0_intersect_z1.end(), std::inserter(z1_minus_z0, z1_minus_z0.end()));

    for (auto & u: p.u_vars) {
        idx_lookup[u] = verilog.PI.size();
        verilog.PI.push_back("x" + std::to_string(idx_lookup[u]));
    }
    auto u_translate = [&](std::string u) {
        return Circuit("x" + std::to_string(idx_lookup[u]));
    };
    verilog.PI.push_back("y");
    verilog.PI.push_back("next_k");

    verilog.registers.push_back(Wire("invalid", Circuit("invalid") | (Circuit("r0") & ~Circuit("T"))));
    // r0 <= 1'b1
    verilog.registers.push_back(Wire("r0", Circuit("1'b1")));
    // r1: reached negation
    // r1 <= r1 | (r0 & ~next_k & y)
    {
        Circuit tmp;
        tmp.operation = "&";
        for (int i = 0; i < min_dep_size; i++) {
            tmp.children.push_back(Circuit("zt" + std::to_string(i)) == Circuit(u_translate(p.e_vars[0].second[i])));
        }
        verilog.registers.push_back(Wire("r1", Circuit("r1") | (Circuit("r0") & ~Circuit("next_k") & ~Circuit("y") & tmp)));
    }
    // L = X_{k, zk}^{b}
    // L' = X_{next_k, x}^{y}

    // k: Current y_k
    // k <= next_k if r0 else 0
    verilog.registers.push_back(Wire("k", Circuit("next_k") & Circuit("r0")));
    // b: Current value of y_k
    // b <= y if r0 else 1 (Start with y = 1)
    verilog.registers.push_back(Wire("b", Circuit("y") | (~Circuit("r0"))));

    // zk_i: Current value of zk_i
    for (int i = 0; i < min_dep_size; i++) {
        verilog.registers.push_back(Wire("zk" + std::to_string(i), (~(Circuit("next_k") & Circuit("r0")) & Circuit(u_translate(p.e_vars[0].second[i])) | (Circuit("next_k") & Circuit("r0") & Circuit(u_translate(p.e_vars[1].second[i]))))));
    }
    for (int i = min_dep_size; i < max_dep_size; i++) {
        verilog.registers.push_back(Wire("zk" + std::to_string(i), Circuit("next_k") & Circuit("r0") & Circuit(u_translate(p.e_vars[1].second[i]))));
    }
    // zt_i: Starting value of zk_i
    // zt_i <= zt_i | (~r0 & z0_i)
    for (int i = 0; i < min_dep_size; i++) {
        verilog.registers.push_back(Wire("zt" + std::to_string(i), Circuit("zt" + std::to_string(i)) | ((~Circuit("r0")) & Circuit(u_translate(p.e_vars[0].second[i])))));
    }

    // next_k = 1 means b represents y0 and ~y represents y1
    // next_k = 0 means b represents y1 and ~y represents y0
    verilog.wires.push_back(Wire(p.e_vars[0].first, (Circuit("next_k") & Circuit("b")) | (~Circuit("next_k") & (~Circuit("y")))));
    verilog.wires.push_back(Wire(p.e_vars[1].first, (Circuit("next_k") & (~Circuit("y"))) | (~Circuit("next_k") & Circuit("b"))));

    std::unordered_map<std::string, std::string> rename_map;
    for (auto& x : p.u_vars) {
        rename_map[x] = "x" + std::to_string(idx_lookup[x]);
    }
    for (auto& y : p.e_vars) {
        rename_map[y.first] = y.first;
    }
    int wire_cnt = 0;
    for (auto& g : p.phi) {
        if (g.name != p.output_var) {
            rename_map[g.name] = "w" + std::to_string(wire_cnt++);
        } else {
            rename_map[g.name] = "phi_" + p.output_var;
        }
        Wire w(rename_map[g.name]);
        if (g.inputs.size() == 0) {
            if (g.operation == "&") {
                w.assign = Circuit("1'b1");
            } else if (g.operation == "|") {
                w.assign = Circuit("1'b0");
            } else {
                print_error("Gate with no inputs must be AND or OR");
            }
        } else {
            w.assign.operation = g.operation;
            for (auto& c : g.inputs) {
                std::string name;
                if (rename_map.find(c.name) != rename_map.end()) {
                    name = rename_map[c.name];
                } else {
                    print_error(("Undefined variable " + c.name).c_str());
                }
                if (c.sign) {
                    w.assign.children.push_back(Circuit(name));
                } else {
                    w.assign.children.push_back(~Circuit(name));
                }
            }
        }
        verilog.wires.push_back(w);
    }

    Wire transition("T");
    transition.assign = Circuit();
    transition.assign.operation = "&";
    transition.assign.children.push_back(Circuit("next_k") == (~Circuit("k")));
    // k = 0 -> k = 1
    {
        Circuit tmp;
        tmp.operation = "&";
        for (auto& v : z0) {
            int idx = std::distance(p.e_vars[0].second.begin(), std::find(p.e_vars[0].second.begin(), p.e_vars[0].second.end(), v));
            tmp.children.push_back(Circuit("zk" + std::to_string(idx)) == Circuit(u_translate(v)));
        }
        tmp.children.push_back(~Circuit("phi_" + p.output_var));
        transition.assign.children.push_back((Circuit("next_k")).implies(tmp));
    }
    // k = 1 -> k = 0
    {
        Circuit tmp;
        tmp.operation = "&";
        for (auto& v : z1) {
            int idx = std::distance(p.e_vars[1].second.begin(), std::find(p.e_vars[1].second.begin(), p.e_vars[1].second.end(), v));
            tmp.children.push_back(Circuit("zk" + std::to_string(idx)) == Circuit(u_translate(v)));
        }
        tmp.children.push_back(~Circuit("phi_" + p.output_var));
        transition.assign.children.push_back((~Circuit("next_k")).implies(tmp));
    }
    verilog.wires.push_back(transition);

    Wire neg_property("neg_P");
    neg_property.assign = Circuit();
    neg_property.assign.operation = "&";
    neg_property.assign.children.push_back(Circuit("r0"));
    neg_property.assign.children.push_back(Circuit("r1"));
    neg_property.assign.children.push_back(~Circuit("k"));
    // neg_property.assign.children.push_back(Circuit("next_k"));
    neg_property.assign.children.push_back(Circuit("b"));
    for (int i = 0; i < min_dep_size; i++) {
        neg_property.assign.children.push_back(Circuit("zt" + std::to_string(i)) == Circuit("zk" + std::to_string(i)));
    }
    verilog.wires.push_back(neg_property);

    verilog.PO.push_back("out");
    verilog.wires.push_back(Wire("out", Circuit("neg_P") & ~Circuit("invalid")));
    // verilog.wires.push_back(Wire("out", Circuit("r0") & ~Circuit("invalid_next") & (Circuit("k") == Circuit("next_k"))));
}

std::string twoDQR::to_string() {
    // Build verilog
    return verilog.to_string();
}
