#include "2dqr/twoDQR.hpp"
#include "2dqr_ext/twoDQR_ext.hpp"
#include <vector>
#include "abc_wrapper/abc_wrapper.hpp"
#include "common/dqcir.hpp"
#include "common/dqdimacs.hpp"
#include "common/dqcir_ext.hpp"
#include "common/utils.hpp"

#include <cxxopts.hpp>
#include <filesystem>
#include <iostream>
#include <string>

int main(int argc, char** argv) {
    cxxopts::Options options("2dqr", "2DQR");

    options.add_options()   ("i,input", "Input File", cxxopts::value<std::string>())
                            ("r,rename", "Rename variables", cxxopts::value<bool>()->default_value("false"))
                            ("s,skolem", "Generate Skolem functions", cxxopts::value<bool>()->default_value("false"))
                            ("e,ext", "Use extended 2DQR", cxxopts::value<bool>()->default_value("false"))
                            ("no_fraig", "Disable optimising Skolem functions with fraig", cxxopts::value<bool>()->default_value("false"))
                            ("keep_aux", "Keep auxiliary files", cxxopts::value<bool>()->default_value("false"))
                            ("v,verbose", "Verbose output", cxxopts::value<bool>()->default_value("false"))
                            ("h,help", "Print usage");

    auto result = options.parse(argc, argv);

    if (result.count("help")) {
        std::cout << options.help() << std::endl;
        exit(0);
    }

    // Cleaning
    {
        std::vector<std::string> files = {"./twoDQR.v", "./twoDQR_inv.v", "./twoDQR_inv.pla", "./oneDQBF.v", "./oneDQBF_inv.v", "./oneDQBF_inv.pla", "./check_skolem.v", "./skolem.v", "./skolem.aig", "./result.txt", "./cex.cex", "./model.cex"};
        for (auto& file : files) {
            if (file_exists(file)) {
                std::filesystem::remove(file);
            }
        }
    }

    bool ext = result["ext"].as<bool>();
    print_debug("DEBUG mode enabled");
    if (ext) {
        print_info("Algorithm = 2DQR_ext");
    } else {
        print_info("Algorithm = 2DQR");
    }

    ABC_wrapper abc(result["verbose"].as<bool>());

    if (!result.count("input")) {
        print_error("No input file specified");
    }
    std::string input_file = result["input"].as<std::string>();
    print_info(("file = " + input_file).c_str());
    if (ext) {
        dqcir_ext p;
        if (split_string(input_file, ".").back() == "dqcir") {
            p.from_file(input_file, result["rename"].as<bool>());
        } else if (split_string(input_file, ".").back() == "dqdimacs") {
            dqdimacs p_dimacs;
            p_dimacs.from_file(input_file);
            p.from_file(p_dimacs);
        } else {
            print_error("File extension must be .dqcir or .dqdimacs");
        }
        twoDQR_ext algo(p, abc);
        algo.opt.gen_skolem = result["skolem"].as<bool>();
        algo.opt.fraig = !result["no_fraig"].as<bool>();
        algo.opt.keep_aux = result["keep_aux"].as<bool>();
        algo.opt.transition_relation = false;
        algo.run();
    } else {
        dqcir p;
        if (split_string(input_file, ".").back() == "dqcir") {
            p.from_file(input_file, result["rename"].as<bool>());
            if (p.e_vars.size() > 2) {
                dqcir_ext q_ext;
                q_ext.from_file(input_file, true);
                p = dqcir();
                p.from_file(q_ext);
                p.to_file("./test.dqcir");
            }
        } else {
            print_error("File extension must be .dicir");
        }
        twoDQR algo(p, abc);
        algo.opt.gen_skolem = result["skolem"].as<bool>();
        algo.opt.fraig = !result["no_fraig"].as<bool>();
        algo.opt.keep_aux = result["keep_aux"].as<bool>();
        algo.opt.transition_relation = false;
        algo.run();
    }


    // Cleaning
    if (!result["keep_aux"].as<bool>()) {
        std::vector<std::string> files = {"./twoDQR.v", "./twoDQR_inv.v", "./twoDQR_inv.pla", "./oneDQBF.v", "./oneDQBF_inv.v", "./oneDQBF_inv.pla", "./check_skolem.v", "./skolem.v", "./cex.cex", "./model.cex", "./abc.history"};
        for (auto& file : files) {
            if (file_exists(file)) {
                std::filesystem::remove(file);
            }
        }
    }

}
