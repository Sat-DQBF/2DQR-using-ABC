#include "common/verilog.hpp"
#include "common/utils.hpp"

#include <fstream>
#include <string>
#include <vector>

// Warning: This function is not robust and may not work for all verilog files
VerilogModule VerilogModule::from_file(std::string filename, bool lazy) {
    if (lazy) {
        VerilogModule module;
        module._from_file = true;
        if (!file_exists(filename)) {
            print_error(("File " + filename + " does not exist").c_str());
        }
        module.filename = filename;
        std::ifstream file(filename);
        read_until(file, "module ");
        module.name = read_until(file, " ", 0);

        read_until(file, "input ");
        module.PI = split_string(read_until(file, ";", 0, " \n\t"), ",");

        read_until(file, "output ");
        module.PO = split_string(read_until(file, ";", 0, " \n\t"), ",");
        file.close();
        
        return module;
    } else {
        return __from_file(filename);
    }
}

// This function reads verilog after fraig by abc
VerilogModule VerilogModule::__from_file(std::string filename) {
    VerilogModule module;

    std::ifstream file(filename);
    if (!file.is_open()) {
        print_error(("File " + filename + " does not exist").c_str());
    }

    std::string line;
    int line_cnt = 0;
    read_until(file, "module ");
    module.name = read_until(file, " ", 0);

    read_until(file, "input ");
    module.PI = split_string(read_until(file, ";", 0, " \n\t"), ",");

    read_until(file, "output ");
    module.PO = split_string(read_until(file, ";", 0, " \n\t"), ",");

    read_until(file, "wire ");
    auto wire_list = split_string(read_until(file, ";", 0, " \n\t"), ",");

    int wire_cnt = wire_list.size() + module.PO.size();
    while (wire_cnt) {
        read_until(file, "assign ");
        std::string wire_name = read_until(file, " = ", 0);
        std::string rhs = read_until(file, ";", 0, " ");
        if (rhs.find("&") != std::string::npos) {
            std::vector<std::string> inputs = split_string(rhs, "&");
            module.wires.push_back(Wire(wire_name, (inputs[0][0] == '~' ? ~Circuit(inputs[0].substr(1)) : Circuit(inputs[0])) & (inputs[1][0] == '~' ? ~Circuit(inputs[1].substr(1)) : Circuit(inputs[1]))));
        } else if (rhs.find("|") != std::string::npos) {
            std::vector<std::string> inputs = split_string(rhs, "|");
            module.wires.push_back(Wire(wire_name, (inputs[0][0] == '~' ? ~Circuit(inputs[0].substr(1)) : Circuit(inputs[0])) | (inputs[1][0] == '~' ? ~Circuit(inputs[1].substr(1)) : Circuit(inputs[1]))));
        } else {
            print_error("Unknown operator");
        }
        // std::string input1 = read_until(file, "&", 0, " ");
        // std::string input2 = read_until(file, ";", 0, " ");
        // module.wires.push_back(Wire(wire_name, (input1[0] == '~' ? ~Circuit(input1.substr(1)) : Circuit(input1)) & (input2[0] == '~' ? ~Circuit(input2.substr(1)) : Circuit(input2))));
        wire_cnt--;
    }

    file.close();
    return module;
}
