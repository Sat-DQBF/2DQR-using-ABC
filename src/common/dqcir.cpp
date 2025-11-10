#include "common/dqcir.hpp"
#include "common/utils.hpp"

#include <boost/algorithm/string/join.hpp>
#include <fstream>
#include <string>

dqcir::dqcir() {
    
}

void dqcir::print_stat(bool detailed) {
    printf("-------- Stat --------\n%lu universal var(s):\n", u_vars.size());
    if (detailed) {
        for (auto& x : u_vars) {
            printf("%s ", x.c_str());
        }
        putchar_unlocked('\n');
    }
    printf("%zu existential var(s):\n", e_vars.size());
    if (detailed) {
        for (auto& y : e_vars) {
            printf("%s: ", y.first.c_str());
            for (auto& x : y.second) {
                printf("%s ", x.c_str());
            }
            putchar_unlocked('\n');
        }
    }

    // if (detailed) {
    //     std::set<std::string> PI(phi.PI.begin(), phi.PI.end());
    //     std::unordered_map<std::string, Wire> wire_map;
    //     for (auto& w : phi.wires) {
    //         wire_map[w.name] = w;
    //     }

    //     std::set<std::string> first_layer_gates;
    //     std::queue<std::pair<std::string, bool>> q;

    //     if (wire_map[phi.PO[0]].assign.operation == "&") {
    //         for (auto& c : wire_map[phi.PO[0]].assign.children) {
    //             if (c.operation == "") {
    //                 q.push({c.name, true});
    //             } else if (c.operation == "~") {
    //                 if (c.children[0].operation == "") {
    //                     q.push({c.children[0].name, false});
    //                 }
    //             }
    //         }
    //     } else {
    //         printf("Output is not an AND gate\n");
    //         return;
    //     }

    //     while (!q.empty()) {
    //         auto [name, b] = q.front();
    //         q.pop();
    //         auto& curr_wire = wire_map[name];
    //         if (curr_wire.assign.operation == "&" && b) {
    //             for (auto& c : curr_wire.assign.children) {
    //                 if (c.operation == "") {
    //                     q.push({c.name, b});
    //                 } else if (c.operation == "~") {
    //                     if (c.children[0].operation == "") {
    //                         q.push({c.children[0].name, !b});
    //                     }
    //                 }
    //             }
    //         } else if (curr_wire.assign.operation == "|" && !b) {
    //             for (auto& c : curr_wire.assign.children) {
    //                 if (c.operation == "") {
    //                     q.push({c.name, b});
    //                 } else if (c.operation == "~") {
    //                     if (c.children[0].operation == "") {
    //                         q.push({c.children[0].name, !b});
    //                     }
    //                 }
    //             }
    //         } else {
    //             first_layer_gates.insert(name);
    //         }
    //     }

    //     // Dig each gates
    //     std::set<std::string> e_vars_set;
    //     std::unordered_map<std::string, std::set<std::string>> gate_dep_set;
    //     for (auto& [y, v] : e_vars) {
    //         e_vars_set.insert(y);
    //         gate_dep_set[y].insert(y);
    //     }
    //     for (auto& w : phi.wires) {
    //         for (auto& c : w.assign.children) {
    //             if (c.operation == "") {
    //                 for (auto& u : gate_dep_set[c.name]) {
    //                     gate_dep_set[w.name].insert(u);
    //                 }
    //             } else if (c.operation == "~") {
    //                 if (c.children[0].operation == "") {
    //                     for (auto& u : gate_dep_set[c.children[0].name]) {
    //                         gate_dep_set[w.name].insert(u);
    //                     }
    //                 }
    //             }
    //         }
    //     }

    //     printf("First layer gates: \n");
    //     for (auto& g : first_layer_gates) {
    //         printf("%s: ", g.c_str());
    //         for (auto& u : gate_dep_set[g]) {
    //             printf("%s ", u.c_str());
    //         }
    //         printf("\n");
    //     }

    // }
}

void dqcir::to_file(std::string path) {
    std::ofstream file(path);
    if (!file) {
        throw std::runtime_error("Error on opening file");
    }
    file << "#QCIR-14\n";
    file << "forall(" << join(u_vars, ", ") << ")\n";
    for (auto& [y, v] : e_vars) {
        file << "depend(" << y << ", " << join(v, ", ") << ")\n";
    }
    file << "output(" << output_var << ")\n\n"; 
    
    auto opr_name = [] (Gate& g) {
        if (g.operation == "|") {
            return "or";
        } else if (g.operation == "&") {
            return "and";
        } else {
            print_error("Unknown operation");
        }
        return "";
    };

    for (auto& g : phi) {
        std::vector<std::string> childs;
        file << g.name << " = " << opr_name(g) << "(";
        for (auto& c : g.inputs) {
            childs.push_back(c.sign ? c.name : "-" + c.name);
        }
        file << boost::join(childs, ", ") << ")" << std::endl;
    }

    file.close();
}