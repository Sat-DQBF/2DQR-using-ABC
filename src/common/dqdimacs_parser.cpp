#include "common/dqdimacs.hpp"
#include "common/utils.hpp"

#include <fstream>
#include <iostream>

// Read from dqdimacs file
void dqdimacs::from_file(std::string path) {
    if (!path.size()) {
        print_error("No file given");
    }
    if (split_string(path, ".").back() != "dqdimacs") {
        print_warning("File does not ends in .dqdimacs");
    }
    std::ifstream file(path);
    if (!file) {
        print_error("Error on opening file");
    }

    uint line_cnt = 0;
    std::string line;
    std::string name;
    std::vector<std::string> parts;

    uint64_t header_var_cnt;
    uint64_t header_clause_cnt;

    // Read header
    while (getline(file, line)) {
        line_cnt++;
        if (line[0] == 'c') {
            continue;
        }
        parts = split_string(line, " ");
        if (parts[0] == "p") {
            if (parts[1] != "cnf") {
                print_error("Only CNF is supported");
            }
            header_var_cnt = std::stoull(parts[2]);
            header_clause_cnt = std::stoull(parts[3]);
            break;
        } else {
            parse_err_msg(line_cnt, "Expected header");
        }
    }

    // Read U/E variables
    while (getline(file, line)) {
        line_cnt++;
        if (line[0] == 'c' || line.size() == 0) {
            continue;
        }
        parts = split_string(line, " \n\r");
        if (parts.back() != "0") {
            parse_err_msg(line_cnt, "Expected 0 after at the end");
        }
        if (parts[0] == "a") {
            for (auto it = parts.begin() + 1; it < parts.end() - 1; it++) {
                if (std::stoull(*it) > header_var_cnt) {
                    parse_err_msg(line_cnt, "Variable index out of bounds");
                }
                u_vars.push_back(std::stoull(*it));
            }
        } else if (parts[0] == "d") {
            if (std::stoull(parts[1]) > header_var_cnt) {
                parse_err_msg(line_cnt, "Variable index out of bounds");
            }
            e_vars.emplace_back(std::stoull(parts[1]), std::vector<uint64_t>());
            for (auto it = parts.begin() + 2; it < parts.end() - 1; it++) {
                if (std::stoull(*it) > header_var_cnt) {
                    parse_err_msg(line_cnt, "Variable index out of bounds");
                }
                e_vars.back().second.push_back(std::stoull(*it));
            }
        } else if (parts[0] == "e") {
            for (auto it = parts.begin() + 1; it < parts.end() - 1; it++) {
                if (std::stoull(*it) > header_var_cnt) {
                    parse_err_msg(line_cnt, "Variable index out of bounds");
                }
                e_vars.emplace_back(std::stoull(*it), std::vector<uint64_t>(u_vars));
            }
        } else {
            break;
        }
    }
    if (u_vars.size() + e_vars.size() != header_var_cnt) {
        print_error("Variable count mismatch");
    }

    // Read clauses
    do {
        line_cnt++;
        if (line[0] == 'c' || line.size() == 0) {
            continue;
        }
        parts = split_string(line, " \n\r");
        Clause clause;
        for (auto it = parts.begin(); it < parts.end() - 1; it++) {
            Literal lit;
            if (it->at(0) == '-') {
                lit.sign = false;
                lit.var = std::stoull(it->substr(1));
            } else {
                lit.sign = true;
                lit.var = std::stoull(*it);
            }
            if (lit.var > header_var_cnt) {
                parse_err_msg(line_cnt, "Variable index out of bounds");
            }
            clause.push_back(lit);
        }
        phi.push_back(clause);
    } while (getline(file, line));
}