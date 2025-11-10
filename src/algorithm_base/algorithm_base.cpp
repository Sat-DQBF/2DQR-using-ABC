#include "algorithm_base/algorithm_base.hpp"
#include "abc_wrapper/abc_wrapper.hpp"
#include "common/utils.hpp"

#include <assert.h>
#include <fstream>
#include <iostream>

void Algorithm::run() {
    std::ofstream out("./twoDQR.v");
    out << to_string();
    out.close();
    print_info("Solving");
    ABC_result result = abc.run_pdr("./twoDQR.v", opt.keep_aux, opt.transition_relation);
    std::ofstream result_out("result.txt");
    if (result == ABC_result::UNKNOWN) {
        print_info("2DQR UNKNOWN");
        result_out << "UNKNOWN\n";
    } else if (result == ABC_result::TIMEOUT) {
        print_info("2DQR TIMEOUT");
        result_out << "TIMEOUT\n";
    } else if (result == ABC_result::UNSAT) {
        print_info("2DQR UNSAT");
        result_out << "UNSAT\n";
    } else if (result == ABC_result::SAT) {
        if (opt.gen_skolem) {
            while (refine()) {
                std::ofstream out("./twoDQR.v");
                out << to_string();
                out.close();
                result = abc.run_pdr("./twoDQR.v", opt.keep_aux, opt.transition_relation);
                if (result == ABC_result::UNSAT) {
                    result_out << "ERROR\n";
                }
                assert(result == ABC_result::SAT);
            }
            print_info(("2DQR SAT with " + std::to_string(patch_cnt) + " patch(es)").c_str());
            result_out << "SAT " << patch_cnt << "\n";
        } else {
            print_info("2DQR SAT");
            result_out << "SAT\n";
        }
    }
    result_out.close();
}
