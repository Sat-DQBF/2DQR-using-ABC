#include "abc_wrapper/abc_wrapper.hpp"
#include "common/utils.hpp"

#include <filesystem>
#include <fstream>
#include <string>
#include <unordered_map>

ABC_wrapper::ABC_wrapper(bool verbose) {
    this->verbose = verbose;
    pAbc = Abc_FrameGetGlobalFrame();
}

ABC_wrapper::~ABC_wrapper() {
    Abc_Stop();
}

int ABC_wrapper::exec(std::string command) {
    return Cmd_CommandExecute(pAbc, command.c_str());
}
int ABC_wrapper::read(std::string filename, bool fraig) {
    int status = exec(("read " + filename).c_str());
    if (status != 0) {
        return status;
    }
    if (fraig) {
        return exec("fraig");
    } else {
        return exec("strash");
    }
}

ABC_result ABC_wrapper::run_pdr(std::string input_file, bool keep_input, bool transition_relation) {
    std::string output_file = input_file.substr(0, input_file.find_last_of('.')) + "_inv.pla";
    if (file_exists(output_file)) {
        std::filesystem::remove(output_file);
    }
    int status;
    std::string command;
    status = read(input_file);
    if (status != 0) {
        print_info(("ABC exited with status " + std::to_string(status)).c_str());
        return ABC_result::UNKNOWN;
    }
    if (transition_relation) {
        command = "pdr -ld; write_cex cex.cex";
    } else {
        command = "pdr -d; write_cex cex.cex";
    }
    print_info("ABC: Running pdr");
    
    exec(command);

    if (!keep_input) {
        std::filesystem::remove(input_file);
    }
    if (file_exists(output_file) && status == 0) {
        std::fstream file(output_file);
        std::string line;
        std::getline(file, line);
        if (line.rfind("# Inductive") == 0) {
            print_info("ABC: pdr SAT");
            return ABC_result::SAT;
        } else {
            print_info("ABC: pdr UNSAT");
            return ABC_result::UNSAT;
        }
    }
    print_info(("ABC exited with status " + std::to_string(status)).c_str());
    return ABC_result::UNKNOWN;
}

void ABC_wrapper::to_v(std::string input_file, std::string output_file, bool keep_input) {
    print_info("ABC: Converting to Verilog");
    int status;
    status = read(input_file, true);
    if (status != 0) {
        print_info(("ABC exited with status " + std::to_string(status)).c_str());
        return;
    }
    status = exec("write_verilog " + output_file);
    if (!keep_input) {
        std::filesystem::remove(input_file);
    }
    if (status == 0) {
        return;
    }
    print_error(("ABC exited with status " + std::to_string(status)).c_str());
}

void ABC_wrapper::to_aig(std::string input_file, std::string output_file, bool fraig, bool keep_input) {
    print_info(("ABC: Converting to AIG" + std::string(fraig ? " with fraig" : "")).c_str());
    int status;
    status = read(input_file, fraig);
    if (status != 0) {
        print_info(("ABC exited with status " + std::to_string(status)).c_str());
        return;
    }
    status = exec("write_aiger -s " + output_file);
    if (!keep_input) {
        std::filesystem::remove(input_file);
    }
    if (status == 0) {
        return;
    }
    print_error(("ABC exited with status " + std::to_string(status)).c_str());
}

void ABC_wrapper::v_to_v_fraig(std::string input_file, std::string output_file, bool keep_input) {
    print_info("ABC: Converting Verilog to AIG with fraig");
    int status;
    status = read(input_file, true);
    if (status != 0) {
        print_info(("ABC exited with status " + std::to_string(status)).c_str());
        return;
    }
    status = exec("write_verilog " + output_file);
    if (!keep_input) {
        std::filesystem::remove(input_file);
    }
    if (status == 0) {
        return;
    }
    print_error(("ABC exited with status " + std::to_string(status)).c_str());
}

std::tuple<ABC_result, std::unordered_map<std::string, bool>> ABC_wrapper::check_sat(std::string input_file, bool keep_input) {
    if (file_exists("./model.cex")) {
        std::filesystem::remove("./model.cex");
    }
    print_info("ABC: Running sat");
    int status;
    status = read(input_file);
    if (status != 0) {
        print_info(("ABC exited with status " + std::to_string(status)).c_str());
        return std::make_tuple(ABC_result::UNKNOWN, std::unordered_map<std::string, bool>());
    }
    status = exec("sat; write_cex -n model.cex");
    if (!keep_input) {
        std::filesystem::remove(input_file);
    }
    if (status == 0) {
        if (file_exists("./model.cex")) {
            std::ifstream model_file("./model.cex");
            std::string line;
            std::unordered_map<std::string, bool> model;
            while (std::getline(model_file, line)) {
                if (line.size() > 0) {
                    std::vector<std::string> assignment = split_string(line, "=");
                    model[assignment[0]] = (assignment[1] == "1");
                }
            }
            model_file.close();
            if (!keep_input) {
                std::filesystem::remove("./model.cex");
            }
            print_info("ABC: sat SAT");
            return std::make_pair(ABC_result::SAT, model);
        } else {
            print_info("ABC: sat UNSAT");
            return std::make_pair(ABC_result::UNSAT, std::unordered_map<std::string, bool>());
        }
    }
    print_info(("ABC exited with status " + std::to_string(status)).c_str());
    return std::make_pair(ABC_result::UNKNOWN, std::unordered_map<std::string, bool>());
}