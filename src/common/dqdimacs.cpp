#include "common/dqdimacs.hpp"

#include <fstream>

dqdimacs::dqdimacs() {
    
}

void dqdimacs::print_stat(bool detailed) {
    printf("-------- Stat --------\n%lu universal var(s):\n", u_vars.size());
    if (detailed) {
        for (auto& x : u_vars) {
            printf("%zu ", x);
        }
        putchar_unlocked('\n');
    }
    printf("%lu existential var(s):\n", e_vars.size());
    if (detailed) {
        for (auto& y : e_vars) {
            printf("%zu: ", y.first);
            for (auto& x : y.second) {
                printf("%lu ", x);
            }
            putchar_unlocked('\n');
        }
    }
    printf("%lu clause(s)\n", phi.size());
    if (detailed) {
        for (auto& clause : phi) {
            for (auto& lit : clause) {
                printf("%s%zu ", lit.sign ? "" : "-", lit.var);
            }
            putchar_unlocked('\n');
        }
    }
}

void dqdimacs::to_file(std::string path) {
    std::ofstream file(path);
    if (!file) {
        throw std::runtime_error("Error on opening file");
    }
    file << "p cnf " << var_cnt << " " << phi.size() << "\n";
    file << "a ";
    for (auto& u : u_vars) {
        file << u << " ";
    }
    file << "0\n";
    std::vector<uint64_t> dep_all_vars;
    for (auto& [y, v] : e_vars) {
        if (v == u_vars) {
            dep_all_vars.push_back(y);
        } else {
            file << "d " << y << " ";
            for (auto& u : v) {
                file << u << " ";
            }
            file << "0\n";
        }
    }
    if (dep_all_vars.size() > 0) {
        file << "e ";
        for (auto& y : dep_all_vars) {
            file << y << " ";
        }
        file << "0\n";
    }
    for (auto& clause : phi) {
        for (auto& lit : clause) {
            file << (lit.sign ? "" : "-") << lit.var << " ";
        }
        file << "0\n";
    }
    file.close();
}