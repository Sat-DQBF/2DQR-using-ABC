#include "common/dqcir_ext.hpp"
#include "common/utils.hpp"

#include <boost/algorithm/string/join.hpp>
#include <fstream>

dqcir_ext::dqcir_ext() {
    
}

void dqcir_ext::print_stat(bool detailed) {
    printf("-------- Stat --------\n%lu universal var(s):\n", u_vars.size());
    if (detailed) {
        for (auto& x : u_vars) {
            printf("%s ", x.c_str());
        }
        putchar_unlocked('\n');
    }
    printf("%lu existential var(s):\n", e_vars.size());
    if (detailed) {
        for (auto& y : e_vars) {
            printf("%s: ", y.first.c_str());
            for (auto& x : y.second) {
                printf("%s ", x.c_str());
            }
            putchar_unlocked('\n');
        }
    }
}

void dqcir_ext::to_file(std::string path) {
    std::ofstream file(path);
    if (!file) {
        throw std::runtime_error("Error on opening file");
    }
    file << "#QCIR-14\n";
    file << "forall(" << boost::join(u_vars, ", ") << ")\n";
    for (auto& y : e_vars) {
        file << "depend(" << y.first << ", " << boost::join(y.second, ", ") << ")\n";
    }
    file << "output(RES)\n\n";

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


    std::vector<std::string> last_wire;
    for (auto& [y1, v] : phi) {
        for (auto& [y2, gates] : v) {
            if (gates.size() == 0) {
                continue;
            }
            file << "#! " << y1 << " " << y2 << "\n";
            for (auto& g : gates) {
                std::vector<std::string> childs;
                file << g.name << " = " << opr_name(g) << "(";
                for (auto& c : g.inputs) {
                    childs.push_back(c.sign ? c.name : "-" + c.name);
                }
                file << boost::join(childs, ", ") << ")" << std::endl;
            }
            last_wire.push_back(gates.back().name);
            file << std::endl;
        }
    }
    file << "#! end\n";
    file << "RES = and(" << boost::join(last_wire, ", ") << ")" << std::endl;

}