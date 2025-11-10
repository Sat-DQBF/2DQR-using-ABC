#include "common/dqcir_ext.hpp"
#include "abc_wrapper/abc_wrapper.hpp"

#include <queue>
#include <set>
#include <string>
#include <unordered_map>


void optimize(dqcir_ext& p, ABC_wrapper& abc) {

    std::vector<Gate> common_gates(p.common_phi);
    
    for (auto& [y1, v] : p.phi) {
        for (auto& [y2, gates] : v) {
            if (gates.size() == 0) {
                continue;
            }
            // Split the gates into two parts
            // 1. Gates that are on a path from y1/y2 to the output
            // 2. Gates that are not on a path from y1/y2 to the output

            std::set<std::string> keep_name;
            std::unordered_map<std::string, std::vector<std::string>> rev_gates;
            for (auto& g : gates) {
                for (auto& in : g.inputs) {
                    rev_gates[in.name].push_back(g.name);
                }
            }

            std::queue<std::string> q;
            q.push(y1);
            q.push(y2);
            while (!q.empty()) {
                std::string name = q.front();
                q.pop();
                
                for (auto& g : rev_gates[name]) {
                    if (keep_name.find(g) == keep_name.end()) {
                        keep_name.insert(g);
                        q.push(g);
                    }
                }
            }

            std::vector<Gate> keep_gates;
            std::vector<Gate> remove_gates;
            for (auto& g : gates) {
                if (keep_name.find(g.name) != keep_name.end()) {
                    keep_gates.push_back(g);
                } else {
                    remove_gates.push_back(g);
                }
            }

            

        }
    }
}