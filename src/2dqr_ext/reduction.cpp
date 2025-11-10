#include "2dqr_ext/twoDQR_ext.hpp"
#include "common/circuit.hpp"
#include "common/dqcir_ext.hpp"
#include "abc_wrapper/abc_wrapper.hpp"
#include "common/utils.hpp"
#include "common/verilog.hpp"

#include <algorithm>
#include <string>
#include <unordered_map>
#include <vector>

twoDQR_ext::twoDQR_ext(dqcir_ext& instance, ABC_wrapper& abc) : p(instance), Algorithm(abc) {
    verilog.name = "twoDQR";
    verilog.wires.resize(0);
    max_dep_size = std::max_element(p.e_vars.begin(), p.e_vars.end(), [](auto& a, auto& b) {return a.second.size() < b.second.size();})->second.size();
    uint32_t log_m = log_2(p.e_vars.size() - 1) + 1;
    print_debug(("log_m = " + std::to_string(log_m)).c_str());
    print_debug(("m = " + std::to_string(p.e_vars.size())).c_str());

    patch_cnt = 0;
    for (auto & u: p.u_vars) {
        idx_lookup[u] = verilog.PI.size();
        verilog.PI.push_back("x" + std::to_string(idx_lookup[u]));
    }
    auto u_translate = [&](std::string u) {
        return Circuit("x" + std::to_string(idx_lookup[u]));
    };

    verilog.PI.push_back("y");
    for (int i = 0; i < log_m; i++) {
        verilog.PI.push_back("next_k" + std::to_string(i));
    }

    // isValidIndex
    std::vector<bool> binary_m(log_m, false);
    for (int i = 0; i < log_m; i++) {
        binary_m[i] = ((1 << (log_m - i - 1)) & (p.e_vars.size() - 1)) != 0;
    }
    verilog.wires.push_back(Wire("isValidIndex"));
    Circuit isValidIndex;
    isValidIndex.operation = "&";
    for (int i = 0; i < log_m; i++) {
        if (!binary_m[i]) {
            Circuit tmp;
            tmp.operation = "|";
            for (int j = 0; j < i; j++) {
                if (binary_m[j]) {
                    tmp.children.push_back(~Circuit("next_k" + std::to_string(j)));
                }
            } 
            tmp.children.push_back(~Circuit("next_k" + std::to_string(i)));
            isValidIndex.children.push_back(tmp);
        }
    }
    if (isValidIndex.children.size() == 0) {
        isValidIndex.children.push_back(Circuit("1'b1"));
    }
    verilog.wires.back().assign = isValidIndex;

    // fix_z_{i}
    for (int i = 0; i < p.e_vars.size(); i++) {
        Circuit tmp;
        tmp.operation = "&";
        for (int j = 0 ; j < p.e_vars[i].second.size(); j++) {
            tmp.children.push_back(u_translate(p.e_vars[i].second[j]) == Circuit("zk" + std::to_string(j)));
        }
        if (tmp.children.size() == 0) {
            tmp.children.push_back(Circuit("1'b1"));
        }
        verilog.wires.push_back(Wire("fix_z_" + std::to_string(i), tmp));
    }

    // next_k_eq_{i}
    for (int i = 0; i < p.e_vars.size(); i++) {
        Circuit tmp;
        tmp.operation = "&";
        for (int j = 0; j < log_m; j++) {
            if (((1 << (log_m - j - 1)) & i) != 0) {
                tmp.children.push_back(Circuit("next_k" + std::to_string(j)));
            } else {
                tmp.children.push_back(~Circuit("next_k" + std::to_string(j)));
            }
        }
        verilog.wires.push_back(Wire("next_k_eq_" + std::to_string(i), tmp));
    }
    // k_eq_{i}
    for (int i = 0; i < p.e_vars.size(); i++) {
        Circuit tmp;
        tmp.operation = "&";
        for (int j = 0; j < log_m; j++) {
            if (((1 << (log_m - j - 1)) & i) != 0) {
                tmp.children.push_back(Circuit("k" + std::to_string(j)));
            } else {
                tmp.children.push_back(~Circuit("k" + std::to_string(j)));
            }
        }
        verilog.wires.push_back(Wire("k_eq_" + std::to_string(i), tmp));
    }

    verilog.registers.push_back(Wire("invalid", Circuit("invalid") | (Circuit("r0") & ~Circuit("T")) | ~Circuit("isValidIndex")));
    // r0 <= 1'b1
    verilog.registers.push_back(Wire("r0", Circuit("1'b1")));
    // r1: reached negation
    // Trick: reg is updated with reg <= reg_next in an always block
    {
        Circuit tmp;
        tmp.operation = "&";
        for (int i = 0; i < log_m; i++) {
            tmp.children.push_back(Circuit("kt" + std::to_string(i)) == Circuit("k" + std::to_string(i) + "_next"));
        }
        for (int i = 0; i < max_dep_size; i++) {
            tmp.children.push_back(Circuit("zt" + std::to_string(i)) == Circuit("zk" + std::to_string(i) + "_next"));
        }
        verilog.registers.push_back(Wire("r1", Circuit("r1") | (Circuit("r0") & ~Circuit("y") & tmp)));
    }

    // L = X_{k, zk}^{b}
    // L' = X_{next_k, x}^{y}

    // k: Current y_k
    // k <= next_k
    for (int i = 0; i < log_m; i++) {
        verilog.registers.push_back(Wire("k" + std::to_string(i), Circuit("next_k" + std::to_string(i))));
    }
    // b: Current value of y_k
    // b <= y if r0 else 1 (Start with y = 1)
    verilog.registers.push_back(Wire("b", Circuit("y") | (~Circuit("r0"))));

    // zk_i: Current value of zk_i
    for (int i = 0; i < max_dep_size; i++) {
        Circuit tmp;
        tmp.operation = "|";
        for (int k = 0; k < p.e_vars.size(); k++) {
            if (p.e_vars[k].second.size() > i) {
                tmp.children.push_back((Circuit("next_k_eq_" + std::to_string(k)) & u_translate(p.e_vars[k].second[i])));
            }
        }
        verilog.registers.push_back(Wire("zk" + std::to_string(i), tmp));
    }

    // kt: Starting k
    for (int i = 0; i < log_m; i++) {
        verilog.registers.push_back(Wire("kt" + std::to_string(i ), Circuit("kt" + std::to_string(i)) | (~Circuit("r0") & Circuit("next_k" + std::to_string(i)))));
    }
    // zt_i: Starting value of zk_i
    for (int i = 0; i < max_dep_size; i++) {
        verilog.registers.push_back(Wire("zt" + std::to_string(i), Circuit("zt" + std::to_string(i)) | (~Circuit("r0") & Circuit("zk" + std::to_string(i) + "_next"))));
    }


    // Precalculations
    std::vector<Circuit> inputs_0_1, inputs_1_0;
    for (auto& v : p.u_vars) {
        inputs_0_1.push_back(u_translate(v));
        inputs_1_0.push_back(u_translate(v));
    }
    verilog.wires.push_back(Wire("neg_y", ~Circuit("y")));
    inputs_0_1.push_back(Circuit("b"));
    inputs_0_1.push_back(Circuit("neg_y"));
    inputs_1_0.push_back(Circuit("neg_y"));
    inputs_1_0.push_back(Circuit("b"));


    // Transition
    Wire transition("T");
    transition.assign = Circuit("");
    transition.assign.operation = "|";
    int wire_cnt = 0;
    std::vector<Gate>* curr_phi = nullptr;
    std::unordered_map<std::string, std::string> global_rename_map;
    for (auto& x : p.u_vars) {
        global_rename_map[x] = "x" + std::to_string(idx_lookup[x]);
    }
    for (auto& g : p.common_phi) {
        global_rename_map[g.name] = "w" + std::to_string(wire_cnt++);
        Wire w(global_rename_map[g.name]);
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
                if (global_rename_map.find(c.name) != global_rename_map.end()) {
                    name = global_rename_map[c.name];
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

    for (auto it1 = p.e_vars.begin(); it1 < p.e_vars.end(); it1++) {
        for (auto it2 = it1 + 1; it2 < p.e_vars.end(); it2++) {
            auto y0 = it1;
            auto y1 = it2;

            uint64_t i = std::distance(p.e_vars.begin(), it1);
            uint64_t j = std::distance(p.e_vars.begin(), it2);

            if (p.phi[it1->first][it2->first].size() == 0) {
                if (p.phi[it2->first][it1->first].size() == 0) {
                    continue;
                }
                std::swap(y0, y1);
                std::swap(i, j);
            }

            // next_k = 1 means b represents y0 and ~y represents y1
            // next_k = 0 means b represents y1 and ~y represents y0
            verilog.wires.push_back(Wire(y0->first + "_" + y1->first + "_0", (Circuit("next_k_eq_" + std::to_string(j)) & Circuit("b")) | (~Circuit("next_k_eq_" + std::to_string(j)) & (~Circuit("y")))));
            verilog.wires.push_back(Wire(y0->first + "_" + y1->first + "_1", (Circuit("next_k_eq_" + std::to_string(j)) & (~Circuit("y"))) | (~Circuit("next_k_eq_" + std::to_string(j)) & Circuit("b"))));

            std::unordered_map<std::string, std::string> rename_map(global_rename_map);
            rename_map[y0->first] = y0->first + "_" + y1->first + "_0";
            rename_map[y1->first] = y0->first + "_" + y1->first + "_1";
            curr_phi = &p.phi[y0->first][y1->first];
            std::string output_var = curr_phi->back().name;
            rename_map[output_var] = "phi_" + y0->first + "_" + y1->first + "_out";
            for (auto& g : *curr_phi) {
                if (g.name != output_var) {
                    rename_map[g.name] = "w" + std::to_string(wire_cnt++);
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

            // y0 -> y1
            {   
                transition.assign.children.push_back((Circuit("k_eq_" + std::to_string(i)) & Circuit("fix_z_" + std::to_string(i)) & Circuit("next_k_eq_" + std::to_string(j)) & ~Circuit("phi_" + y0->first + "_" + y1->first + "_out")));
            }
            // y1 -> y0
            {   
                transition.assign.children.push_back((Circuit("k_eq_" + std::to_string(j)) & Circuit("fix_z_" + std::to_string(j)) & Circuit("next_k_eq_" + std::to_string(i)) & ~Circuit("phi_" + y0->first + "_" + y1->first + "_out")));
            }
        }
    }
    verilog.wires.push_back(transition);

    Wire neg_property("neg_P");
    neg_property.assign = Circuit("");
    neg_property.assign.operation = "&";
    neg_property.assign.children.push_back(Circuit("r0"));
    neg_property.assign.children.push_back(Circuit("r1"));
    for (int i = 0; i < log_m; i++) {
        neg_property.assign.children.push_back(Circuit("k" + std::to_string(i)) == Circuit("kt" + std::to_string(i)));
    }
    neg_property.assign.children.push_back(Circuit("b"));
    for (int i = 0; i < max_dep_size; i++) {
        neg_property.assign.children.push_back(Circuit("zt" + std::to_string(i)) == Circuit("zk" + std::to_string(i)));
    }
    verilog.wires.push_back(neg_property);

    verilog.PO.push_back("out");
    verilog.wires.push_back(Wire("out", Circuit("neg_P") & ~Circuit("invalid")));
}

std::string twoDQR_ext::to_string() {
    // Build verilog
    return verilog.to_string();
}