
#include "abc_wrapper/abc_wrapper.hpp"
#include "common/circuit.hpp"
#include "common/dqcir.hpp"
#include "common/dqdimacs.hpp"
#include "common/utils.hpp"
#include "common/verilog.hpp"

#include <cstdlib>
#include <cxxopts.hpp>
#include <fstream>
#include <iostream>
#include <stdio.h>
#include <string>
#include <vector>


int main(int argc, char** argv) {
    cxxopts::Options options("2dqr", "2DQR");

    options.add_options()   ("i,input", "Input File", cxxopts::value<std::string>())
                            ("r,rename", "Rename variables", cxxopts::value<bool>()->default_value("false"))
                            ("s,skolem", "Generate Skolem functions", cxxopts::value<std::string>()->default_value("./skolem.aig"))
                            ("v,verbose", "Verbose output", cxxopts::value<bool>()->default_value("false"))
                            ("h,help", "Print usage");

    auto result = options.parse(argc, argv);

    if (result.count("help")) {
        std::cout << options.help() << std::endl;
        exit(0);
    }

    if (!result.count("input")) {
        printf("No input file specified\n");
        exit(0);
    }

    if (!result.count("skolem")) {
        printf("No skolem file specified\n");
        exit(0);
    }

    std::string skolem_file = result["skolem"].as<std::string>();

    ABC_wrapper abc(result["verbose"].as<bool>());

    if (file_exists(skolem_file)) {
        print_info(("Skolem = " + skolem_file).c_str());
    } else {
        print_error("Skolem file does not exist");
    }

    abc.to_v(skolem_file, "./skolem.v", true);
    VerilogModule skolem = VerilogModule::from_file("./skolem.v");

    std::string input_file = result["input"].as<std::string>();
    print_info(("file = " + input_file).c_str());
    dqcir p;
    if (split_string(input_file, ".").back() == "dqcir") {
        p.from_file(input_file, result["rename"].as<bool>());
    } else if (split_string(input_file, ".").back() == "dqdimacs") {
        dqdimacs p_dimacs;
        p_dimacs.from_file(input_file);
        p.from_file(p_dimacs);
    } else {
        print_error("File extension must be .dqcir or .dqdimacs");
    }

    VerilogModule check("check");
    for (auto& u : p.u_vars) {
        check.PI.push_back(u);
    }
    for (int i = 0; i < p.e_vars.size(); i++) {
        check.wires.push_back(Wire(p.e_vars[i].first, Circuit("skolem_" + skolem.PO[i])));
    }
    check.PO.push_back("RES");
    std::vector<Circuit> inputs;
    for (auto& u : p.u_vars) {
        inputs.push_back(Circuit(u));
    }

    check.submodules.push_back({.module = skolem, .name = "skolem", .inputs = inputs});
    for (auto& e : p.e_vars) {
        inputs.push_back(Circuit(e.first));
    }
    
    {
        std::unordered_map<std::string, std::string> rename_map;
        for (auto& x : p.u_vars) {
            rename_map[x] = x;
        }
        rename_map[p.e_vars[0].first] = p.e_vars[0].first;
        rename_map[p.e_vars[1].first] = p.e_vars[1].first;
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
            check.wires.push_back(w);
        }
    }

    check.wires.push_back(Wire("RES", ~Circuit("phi_" + p.output_var)));

    std::ofstream out("./check.v");
    out << check.to_string();
    out.close();

    auto [abc_result, _] = abc.check_sat("./check.v", true);
    if (abc_result == ABC_result::UNSAT) {
        print_info("2DQR: Verification successful");
    } else {
        print_error("2DQR: Verification failed");
    }
}
