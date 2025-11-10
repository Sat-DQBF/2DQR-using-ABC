#include "common/dqcir_ext.hpp"
#include "common/utils.hpp"

#include <fstream>
#include <iostream>
#include <set>

// Read from dqcir file
void dqcir_ext::from_file(std::string path, bool rename) {
    if (!path.size()) {
        print_error("ERROR: No file given");
    }
    if (split_string(path, ".").back() != "dqcir") {
        printf("WARNING: File does not ends in .dqcir");
    }
    std::ifstream file(path);
    if (!file) {
        throw std::runtime_error("Error on opening file");
    }

    uint line_cnt = 0;
    std::string line;
    std::string name;
    std::vector<std::string> parts;
    std::string output_var;

    // Read U/E variables
    while (getline(file, line)) {
        line_cnt++;
        if (line[0] == '#') {
            continue;
        }
        parts = split_string(line, "= (),\n\r");
        if (parts[0] == "forall") {
            for (auto it = parts.begin() + 1; it < parts.end(); it++) {
                if (rename) {
                    name = "u" + std::to_string(u_vars.size() + 1);
                } else {
                    name = *it;
                }
                rename_map[*it] = name;
                u_vars.push_back(name);
            }
        } else if (parts[0] == "depend") {
            if (rename) {
                name = "e" + std::to_string(e_vars.size() + 1);
            } else {
                name = parts[1];
            }
            rename_map[parts[1]] = name;
            e_vars.emplace_back(name, std::vector<std::string>());
            for (auto it = parts.begin() + 2; it < parts.end(); it++) {
                e_vars.back().second.push_back(rename_map[*it]);
            }
        } else if (parts[0] == "exists") {
            for (auto it = parts.begin() + 1; it < parts.end(); it++) {
                if (rename) {
                    name = "e" + std::to_string(e_vars.size() + 1);
                } else {
                    name = *it;
                }
                rename_map[*it] = name;
                e_vars.emplace_back(name, std::vector<std::string>(u_vars));
            }
        } else if (parts[0] == "output") {
            if (rename) {
                name = "out";
            } else {
                name = parts[1];
            }
            rename_map[parts[1]] = name;
            break;
        } else {
            parse_err_msg(line_cnt, "Expected output variable before circuit");
        }
    }

    std::set<std::string> allowed_oprs = {"and", "or", "not", "nand", "nor", "xor"};
    std::unordered_map<std::string, std::string> opr_map = {
        {"and", "&"}, {"or", "|"}, {"not", "~"}, {"nand", "~&"}, {"nor", "~|"}, {"xor", "^"}};

    std::string curr_y0 = "";
    std::string curr_y1 = "";
    int wire_cnt = 0;

    std::vector<Gate>* curr_phi = &common_phi;
    std::set<std::string> e_vars_set;
    for (auto& [y, v] : e_vars) {
        e_vars_set.insert(y);
    }

    while (getline(file, line)) {
        line_cnt++;
        if (line.size() > 0 && line[0] != '#' && line[0] != '\n' && line[0] != '\r' && line[0] != ' ') {
            parts = split_string(line, "= (),\n\r");
            if (allowed_oprs.find(parts[1]) != allowed_oprs.end()) {
                if (rename) {
                    if (rename_map.find(parts[0]) == rename_map.end()) {
                        name = "w" + std::to_string(++wire_cnt);
                    } else {
                        parse_err_msg(line_cnt, ("Variable " + parts[0] + " already defined").c_str());
                        // name = rename_map[parts[0]];
                    }
                } else {
                    name = parts[0];
                }
                rename_map[parts[0]] = name;
                curr_phi->push_back({name, opr_map[parts[1]], {}});
                for (int i = 2; i < parts.size(); i++) {
                    name = parts[i];
                    if (name[0] != '-') {
                        if (rename_map.find(name) == rename_map.end()) {
                            parse_err_msg(line_cnt, ("Undefined variable " + name).c_str());
                        }
                        name = rename_map[name];
                        if (e_vars_set.find(name) != e_vars_set.end() && name != curr_y0 && name != curr_y1) {
                            parse_err_msg(line_cnt, ("Variable " + name + " is existential, but appears in section #! " + curr_y0 + " " + curr_y1).c_str());
                        }
                        curr_phi->back().inputs.push_back({name, true});
                    } else {
                        name = name.substr(1, name.size() - 1);
                        if (rename_map.find(name) == rename_map.end()) {
                            parse_err_msg(line_cnt, ("Undefined variable " + name).c_str());
                        }
                        if (e_vars_set.find(name) != e_vars_set.end() && name != curr_y0 && name != curr_y1) {
                            parse_err_msg(line_cnt, ("Variable " + name + " is existential, but appears in section #! " + curr_y0 + " " + curr_y1).c_str());
                        }
                        name = rename_map[name];
                        curr_phi->back().inputs.push_back({name, false});
                    }
                }
            }
        } else if (line.size() >= 2 && line[0] == '#' && line[1] == '!') {
            parts = split_string(line, " \n\r");
            if (parts.size() == 2 && parts[1] == "end") {
                curr_phi = nullptr;
                break;
            }
            if (parts.size() != 3) {
                parse_err_msg(line_cnt, "Expected 2 existential variables after #!");
            }
            curr_y0 = rename_map[parts[1]];
            curr_y1 = rename_map[parts[2]];
            phi[curr_y0][curr_y1] = std::vector<Gate>();
            curr_phi = &(phi[curr_y0][curr_y1]);
        }
    }
}