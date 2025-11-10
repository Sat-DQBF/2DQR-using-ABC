#include "2dqr_ext/twoDQR_ext.hpp"
#include "common/circuit.hpp"
#include "common/dqcir_ext.hpp"
#include "abc_wrapper/abc_wrapper.hpp"
#include "common/utils.hpp"
#include "common/verilog.hpp"

#include <assert.h>
#include <fstream>
#include <functional>
#include <regex>
#include <iostream>
#include <set>
#include <string>

bool twoDQR_ext::refine() {
    uint32_t log_m = log_2(p.e_vars.size() - 1) + 1;

    // ABC writes pla file with variables <circuit name>|<register name>, which is not supported by ABC's pla parser
    {
        std::ifstream file("./twoDQR_inv.pla");
        std::ofstream file_out("./twoDQR_inv_fixed.pla");
        int line_cnt = 0;
        std::string line;
        while (std::getline(file, line)) {
            if (++line_cnt == 6) {
                line = std::regex_replace(line, std::regex("twoDQR\\|"), "");
            }
            file_out << line << std::endl;
        }
        file.close();
        file_out.close();
        std::remove("./twoDQR_inv.pla");
        std::rename("./twoDQR_inv_fixed.pla", "./twoDQR_inv.pla");
    }

    abc.to_v("./twoDQR_inv.pla", "./twoDQR_inv.v", opt.keep_aux);
    std::ifstream file("./twoDQR_inv.v");
    VerilogModule inv = VerilogModule::from_file("./twoDQR_inv.v");

    VerilogModule check_skolem("check_skolem");
    for (auto& u : p.u_vars) {
        check_skolem.PI.push_back(u);
    }
    for (auto& e : p.e_vars) {
        check_skolem.PI.push_back(e.first);
    }
    check_skolem.PO.push_back("RES");

    std::function<Circuit (int)> get_candidate = [&] (int k) -> Circuit {
        std::vector<Circuit> args;
        // invalid r0 r1 k0 k1 k2 k3 b zk0 zk1 zk2 zk3 zk4 kt0 kt1 kt2 kt3 zt0 zt1 zt3 zt4
        for (auto& u : inv.PI) {
            if (u == "invalid") {
                args.push_back(Circuit("1'b0")); // invalid = 0
            } else if (u == "r0") {
                args.push_back(Circuit("1'b1")); // r0 = 1
            } else if (u == "r1") {
                args.push_back(Circuit("1'b1")); // r1 = 1
            } else if (u == "b") {
                args.push_back(Circuit("1'b0")); // b = 0
            } else if (u[0] == 'k') { // k and kt
                int idx;
                if (u[1] == 't') {
                    idx = std::stoi(u.substr(2));
                } else {
                    idx = std::stoi(u.substr(1));
                }

                if (((1 << (log_m - idx - 1)) & k) != 0) {
                    args.push_back(Circuit("1'b1"));
                } else {
                    args.push_back(Circuit("1'b0"));
                }

            } else { //zk and zt
                int idx = std::stoi(u.substr(2));
                if (idx < p.e_vars[k].second.size()) {
                    args.push_back(Circuit(p.e_vars[k].second[idx]));
                } else {
                    args.push_back(Circuit("1'b0"));
                }
            }
        }
        check_skolem.submodules.push_back({.module = inv, .name = "f_" + std::to_string(k), .inputs = args});
        return Circuit("f_" + std::to_string(k) + "_" + inv.PO[0]);
    };

    Wire phi("phi");
    phi.assign.operation = "&";

    Wire res("RES");
    res.assign.operation = "&";
    res.assign.children.push_back(~Circuit("phi"));

    for (int i = 0; i < p.e_vars.size(); i++) {
        check_skolem.wires.push_back(Wire("f_" + std::to_string(i), get_candidate(i)));
        res.assign.children.push_back(Circuit("f_" + std::to_string(i)) == Circuit(p.e_vars[i].first));
    }

    int wire_cnt = 0;
    std::vector<Gate>* curr_phi = nullptr;
    std::unordered_map<std::string, std::string> global_rename_map;
    for (auto& x : p.u_vars) {
        global_rename_map[x] = x;
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
        check_skolem.wires.push_back(w);
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


            std::unordered_map<std::string, std::string> rename_map(global_rename_map);
            rename_map[y0->first] = "f_" + std::to_string(i);
            rename_map[y1->first] = "f_" + std::to_string(j);
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
                check_skolem.wires.push_back(w);
            }
            check_skolem.PI.push_back("phi_" + std::to_string(i) + "_" + std::to_string(j));
            phi.assign.children.push_back(Circuit("phi_" + y0->first + "_" + y1->first + "_out"));
            res.assign.children.push_back(Circuit("phi_" + std::to_string(i) + "_" + std::to_string(j)) == Circuit("phi_" + y0->first + "_" + y1->first + "_out"));
        }
    }
    check_skolem.wires.push_back(phi);
    check_skolem.wires.push_back(res);

    std::ofstream out("check_skolem.v");
    out << check_skolem.to_string() << std::endl;
    out.close();

    auto [result, model] = abc.check_sat("./check_skolem.v", opt.keep_aux);
    if (result != ABC_result::SAT) {
        // Save the skolem function
        VerilogModule skolem("skolem");
        skolem.PI = p.u_vars;
        for (auto& v : p.e_vars) {
            skolem.PO.push_back(v.first);
        }

        for (auto& sub: check_skolem.submodules) {
            if (!sub.name.starts_with("phi_")) {
                skolem.submodules.push_back(sub);
            }
        }
        for (auto& w : check_skolem.wires) {
            if (w.name.starts_with("f_")) {
                skolem.wires.push_back(w);
                skolem.wires.back().name = p.e_vars[std::stoi(w.name.substr(2))].first;
            }
        }

        std::ofstream out("skolem.v");
        out << skolem.to_string() << std::endl;
        out.close();
        abc.to_aig("./skolem.v", "./skolem.aig", opt.fraig, opt.keep_aux);
        return false;
    }

    std::string cex_str = "";
    for (auto& u : check_skolem.PI) {
        cex_str += model[u] ? "1" : "0";
    }
    assert(patch_set.find(cex_str) == patch_set.end());
    patch_set.insert(cex_str);
    patch(model);
    return true;
}

void twoDQR_ext::patch(std::unordered_map<std::string, bool>& model) {
    uint32_t log_m = log_2(p.e_vars.size() - 1) + 1;

    patch_cnt++;
    print_info(("Applying patch " + std::to_string(patch_cnt)).c_str());
    
    int i = -1, j = -1;
    for (auto& [k, v] : model) {
        if (k.starts_with("phi_") && !v) {
            std::vector<std::string> idx = split_string(k, "_");
            i = std::stoi(idx[1]);
            j = std::stoi(idx[2]);
            print_debug(k.c_str());
            break;
        }
    }
    assert(i != -1 && j != -1);

    Wire patch_wire("patch_" + std::to_string(patch_cnt));
    patch_wire.assign = Circuit("");
    patch_wire.assign.operation = "&";
    patch_wire.assign.children.push_back("k_eq_" + std::to_string(i));
    patch_wire.assign.children.push_back("fix_z_" + std::to_string(i));
    for (int k = 0; k < p.e_vars[i].second.size(); k++) {
        if (model[p.e_vars[i].second[k]]) {
            patch_wire.assign.children.push_back(Circuit("zk" + std::to_string(k)));
        } else {
            patch_wire.assign.children.push_back(~Circuit("zk" + std::to_string(k)));
        }
    }

    patch_wire.assign.children.push_back("next_k_eq_" + std::to_string(i));
    
    if (model[p.e_vars[i].first]) {
        patch_wire.assign.children.push_back(~Circuit("b"));
        patch_wire.assign.children.push_back(Circuit("y"));
    } else {
        patch_wire.assign.children.push_back(Circuit("b"));
        patch_wire.assign.children.push_back(~Circuit("y"));
    }

    verilog.wires.push_back(patch_wire);
    for (auto& wire : verilog.wires) {
        if (wire.name == "T") {
            wire.assign.children.push_back(Circuit(patch_wire.name));
        }
    }
}
