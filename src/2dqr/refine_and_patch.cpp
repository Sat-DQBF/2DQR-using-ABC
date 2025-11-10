#include "2dqr/twoDQR.hpp"
#include "common/circuit.hpp"
#include "common/dqcir.hpp"
#include "abc_wrapper/abc_wrapper.hpp"
#include "common/utils.hpp"
#include "common/verilog.hpp"

#include <assert.h>
#include <fstream>
#include <regex>
#include <iostream>
#include <set>
#include <string>
#include <vector>


bool twoDQR::refine() {
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
    VerilogModule inv = VerilogModule::from_file("./twoDQR_inv.v");

    VerilogModule check_skolem("check_skolem");
    for (auto& u : p.u_vars) {
        check_skolem.PI.push_back(u);
    }
    for (auto& u : p.u_vars) {
        check_skolem.PI.push_back(u + "$$p");
    }
    check_skolem.PI.push_back(p.e_vars[0].first);
    check_skolem.PI.push_back(p.e_vars[0].first + "$$p");
    check_skolem.PO.push_back("RES");

    std::set<std::string> z1(p.e_vars[0].second.begin(), p.e_vars[0].second.end());
    auto get_f0_candidate = [&] (bool primed, VerilogModule& module) -> Circuit {
        std::vector<Circuit> args;
        // invalid, r0, r1, k, b, zk1, zk2, ..., zkn, zt1, ..., ztn;
        for (auto& u : inv.PI) {
            if (u == "invalid") {
                args.push_back(Circuit("1'b0")); // invalid = 0
            } else if (u == "r0") {
                args.push_back(Circuit("1'b1")); // r0 = 1
            } else if (u == "r1") {
                args.push_back(Circuit("1'b1")); // r1 = 1
            } else if (u == "k") {
                args.push_back(Circuit("1'b0")); // k = 0
            } else if (u == "b") {
                args.push_back(Circuit("1'b0")); // b = 0
            } else {
                int idx = std::stoi(u.substr(2));
                if (idx < p.e_vars[0].second.size()) {
                    if (primed) {
                        args.push_back(Circuit(p.e_vars[0].second[idx] + "$$p"));
                    } else {
                        args.push_back(Circuit(p.e_vars[0].second[idx]));
                    }
                } else {
                    args.push_back(Circuit("1'b0"));
                }
            }
        }

        if (primed) {
            module.submodules.push_back({.module = inv, .name = "y0$$p", .inputs = args});
            return Circuit("y0$$p_" + inv.PO[0]);
        } else {
            module.submodules.push_back({.module = inv, .name = "y0", .inputs = args});
            return Circuit("y0_" + inv.PO[0]);
        }
    };

    // phi_0
    {
        std::unordered_map<std::string, std::string> rename_map;
        for (auto& x : p.u_vars) {
            rename_map[x] = x;
        }
        rename_map[p.e_vars[0].first] = p.e_vars[0].first;
        rename_map[p.e_vars[1].first] = "1'b0";
        rename_map[p.output_var] = "phi_" + p.output_var;
        int wire_cnt = 0;
        for (auto & g : p.phi) {
            if (g.name != p.output_var) {
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
    }

    // phi_1
    {
        std::unordered_map<std::string, std::string> rename_map;
        for (auto& x : p.u_vars) {
            rename_map[x] = x + "$$p";
        }
        rename_map[p.e_vars[0].first] = p.e_vars[0].first + "$$p";
        rename_map[p.e_vars[1].first] = "1'b1";
        rename_map[p.output_var] = "phi_" + p.output_var + "$$p";
        int wire_cnt = 0;
        for (auto & g : p.phi) {
            if (g.name != p.output_var) {
                rename_map[g.name] = "w" + std::to_string(wire_cnt++) + "$$p";
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
    }

    check_skolem.wires.push_back(Wire("f0", get_f0_candidate(0, check_skolem)));
    check_skolem.wires.push_back(Wire("f0$$p", get_f0_candidate(1, check_skolem)));

    check_skolem.wires.push_back(Wire("RES"));
    check_skolem.wires.back().assign.operation = "&";
    check_skolem.wires.back().assign.children.push_back(~Circuit("phi_" + p.output_var));
    check_skolem.wires.back().assign.children.push_back(~Circuit("phi_" + p.output_var + "$$p"));
    check_skolem.wires.back().assign.children.push_back(Circuit("f0") == Circuit(p.e_vars[0].first));
    check_skolem.wires.back().assign.children.push_back(Circuit("f0$$p") == Circuit(p.e_vars[0].first + "$$p"));

    // z_1 = z_1'
    for (auto& u : p.e_vars[1].second) {
        check_skolem.wires.back().assign.children.push_back(Circuit(u) == Circuit(u + "$$p"));
    }

    std::ofstream out("check_skolem.v");
    out << check_skolem.to_string() << std::endl;
    out.close();

    auto [result, model] = abc.check_sat("./check_skolem.v", opt.keep_aux);

    if (result != ABC_result::SAT) {
        // Generate f1 by solving a 1DQBF
        VerilogModule oneDQBF("oneDQBF");

        auto u_translate = [&](std::string u) {
            return Circuit("x" + std::to_string(idx_lookup[u]));
        };

        for (auto & u: p.u_vars) {
            oneDQBF.PI.push_back(u_translate(u).name);
        }

        // get_f0_candidate with translation
        std::vector<Circuit> args;
        // invalid, r0, r1, k, b, zk1, zk2, ..., zkn, zt, ..., zt;
        for (auto& u : inv.PI) {
            if (u == "invalid") {
                args.push_back(Circuit("1'b0")); // invalid = 0
            } else if (u == "r0") {
                args.push_back(Circuit("1'b1")); // r0 = 1
            } else if (u == "r1") {
                args.push_back(Circuit("1'b1")); // r1 = 1
            } else if (u == "k") {
                args.push_back(Circuit("1'b0")); // k = 0
            } else if (u == "b") {
                args.push_back(Circuit("1'b0")); // b = 0
            } else {
                int idx = std::stoi(u.substr(2));
                if (idx < p.e_vars[0].second.size()) {
                    args.push_back(Circuit(u_translate(p.e_vars[0].second[idx]).name));
                } else {
                    args.push_back(Circuit("1'b0"));
                }
            }
        }

        oneDQBF.submodules.push_back({.module = inv, .name = "y0", .inputs = args});
        oneDQBF.wires.push_back(Wire("f0", Circuit("y0_" + inv.PO[0])));

        // phi = phi(x, f0, f1)
        {
            std::unordered_map<std::string, std::string> rename_map;
            for (auto& x : p.u_vars) {
                rename_map[x] = u_translate(x).name;
            }
            rename_map[p.e_vars[0].first] = "f0";
            rename_map[p.e_vars[1].first] = "f1";
            rename_map[p.output_var] = "phi_" + p.output_var;
            int wire_cnt = 0;
            for (auto & g : p.phi) {
                if (g.name != p.output_var) {
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
                oneDQBF.wires.push_back(w);
            }
        }

        Circuit fix_z;
        fix_z.operation = "&";
        for (int i = 0; i < p.e_vars[1].second.size(); i++) {
            auto u = p.e_vars[1].second[i];
            fix_z.children.push_back(Circuit("zk" + std::to_string(i)) == Circuit(u_translate(u)));
        }

        oneDQBF.registers.push_back(Wire("invalid",  Circuit("invalid") | (~Circuit("f1") & Circuit("phi_" + p.output_var)) | Circuit("f1")));

        oneDQBF.registers.push_back(Wire("f1", Circuit("1'b1")));

        for (int i = 0; i < p.e_vars[1].second.size(); i++) {
            auto u = p.e_vars[1].second[i];
            oneDQBF.registers.push_back(Wire("zk" + std::to_string(i), Circuit("zk" + std::to_string(i)) | (~Circuit("f1") & Circuit(u_translate(u)))));
        }
        
        oneDQBF.PO.push_back("out");
        oneDQBF.wires.push_back(Wire("out", (Circuit("f1") & ~Circuit("phi_" + p.output_var) & fix_z) & ~Circuit("invalid")));

        std::ofstream out("./oneDQBF.v");
        out << oneDQBF.to_string() << std::endl;
        out.close();


        result = abc.run_pdr("./oneDQBF.v", opt.keep_aux, false);
        assert(result == ABC_result::SAT);

    // ABC writes pla file with variables <circuit name>|<register name>, which is not supported by ABC's pla parser
        {
            std::ifstream file("./oneDQBF_inv.pla");
            std::ofstream file_out("./oneDQBF_inv_fixed.pla");
            int line_cnt = 0;
            std::string line;
            while (std::getline(file, line)) {
                if (++line_cnt == 6) {
                    line = std::regex_replace(line, std::regex("oneDQBF\\|"), "");
                }
                if (line == ".i 0") {
                    line = ".i 1\n.o 1\n.p 1\n.ilb invalid\n.ob inv\n- 0\n.e";
                    file_out << line << std::endl;
                    break;
                }
                file_out << line << std::endl;
            }
            file.close();
            file_out.close();
            std::remove("./oneDQBF_inv.pla");
            std::rename("./oneDQBF_inv_fixed.pla", "./oneDQBF_inv.pla");
        }
        abc.to_v("./oneDQBF_inv.pla", "./oneDQBF_inv.v", opt.keep_aux);

        // Save the skolem function
        VerilogModule skolem("skolem");
        skolem.PI = p.u_vars;
        if (!swapped) {
            skolem.PO.push_back(p.e_vars[0].first);
            skolem.PO.push_back(p.e_vars[1].first);
        } else {
            skolem.PO.push_back(p.e_vars[1].first);
            skolem.PO.push_back(p.e_vars[0].first);
        }

        // f0
        skolem.wires.push_back(Wire(p.e_vars[0].first, get_f0_candidate(0, skolem)));

        // f1
        VerilogModule _inv = VerilogModule::from_file("./oneDQBF_inv.v");
        std::vector<Circuit> inputs;
        for (auto& u : _inv.PI) {
            if (u == "invalid") {
                inputs.push_back(Circuit("1'b0")); // invalid = 0
            } else if (u == "f1") {
                inputs.push_back(Circuit("1'b1")); // f1 = 1
            } else {
                int idx = std::stoi(u.substr(2));
                if (idx < p.e_vars[1].second.size()) {
                    inputs.push_back(Circuit(p.e_vars[1].second[idx]));
                } else {
                    inputs.push_back(Circuit("1'b0"));
                }
            }
        }
        skolem.submodules.push_back({.module = _inv, .name = "f_1_inv", .inputs = inputs});
        skolem.wires.push_back(Wire(p.e_vars[1].first, ~Circuit("f_1_inv_" + _inv.PO[0])));

        out = std::ofstream("skolem.v");
        out << skolem.to_string() << std::endl;
        out.close();
        abc.to_aig("./skolem.v", "./skolem.aig", opt.fraig, opt.keep_aux);
        return false;
    }
    // Extract the counterexample
    std::string cex_str = "";
    for (auto& u : p.e_vars[0].second) {
        cex_str += model[u] ? "1" : "0";
    }
    cex_str += model[p.e_vars[0].first] ? "1" : "0";
    if (patch_set.find(cex_str) != patch_set.end()) {
        print_error("Repeated counterexample.");
    }
    patch_set.insert(cex_str);
    patch(model);
    return true;
}

void twoDQR::patch(std::unordered_map<std::string, bool>& model) {

    auto u_translate = [&](std::string u) {
        return Circuit("x" + std::to_string(idx_lookup[u]));
    };
    
    patch_cnt++;
    print_info(("Applying patch " + std::to_string(patch_cnt)).c_str());

    Wire patch_wire("patch_" + std::to_string(patch_cnt));
    patch_wire.assign = Circuit("");
    patch_wire.assign.operation = "&";

    patch_wire.assign.children.push_back(~Circuit("k"));
    patch_wire.assign.children.push_back(~Circuit("next_k"));

    if (model[p.e_vars[0].first]) { 
        // Assign with 1, i.e., add edge 0 -> 1
        patch_wire.assign.children.push_back(~Circuit("b"));
        patch_wire.assign.children.push_back(Circuit("y"));
    } else {
        // Assign with 0, i.e., add edge 1 -> 0
        patch_wire.assign.children.push_back(Circuit("b"));
        patch_wire.assign.children.push_back(~Circuit("y"));
    }

    for (int i = 0; i < p.e_vars[0].second.size(); i++) {
        if (model[p.e_vars[0].second[i]]) {
            patch_wire.assign.children.push_back(Circuit("zk" + std::to_string(i)));
            patch_wire.assign.children.push_back(Circuit(u_translate(p.e_vars[0].second[i])));
        } else {
            patch_wire.assign.children.push_back(~Circuit("zk" + std::to_string(i)));
            patch_wire.assign.children.push_back(~Circuit(u_translate(p.e_vars[0].second[i])));
        }
    }
    
    verilog.wires.push_back(patch_wire);
    for (auto& wires : verilog.wires) {
        if (wires.name == "T") {
            wires.assign = wires.assign | Circuit(patch_wire.name);
            break;
        }
    }
}
