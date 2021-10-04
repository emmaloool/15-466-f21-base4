#include <vector>
#include <string>
#include <iostream>
#include <fstream>

#include "PhaseInfo.hpp"
#include "data_path.hpp"
#include "read_write_chunk.hpp"

using namespace std;


int main(int argc, char **argv) {

    vector<Phase> phases;
    vector<Option> options;

    fstream txt_file;
    string line;
    txt_file.open(data_path("../script.txt"), std::ios::in);
    std::cout << "hello?" <<std::endl;
    if (txt_file.is_open()) {
        while (getline(txt_file, line)) {
            Phase phase;

            // Read phases as clumps of lines, so we can't just loop over all lines
            // To start, read first phase info
            // getline(txt_file, line);

            auto space_ind = line.find(' ');
            phase.id = stoi(line.substr(0, space_ind));
            phase.num_options = stoi(line.substr(space_ind, std::string::npos)); // tells us how many option lines to read

            std::cout << "phase.id: " << unsigned(stoi(line.substr(0, space_ind)));
            std::cout << "num_options: " << unsigned(stoi(line.substr(space_ind, std::string::npos))) << std::endl;

            if (phase.num_options == 0) {
                getline(txt_file, line);
                getline(txt_file, line);
                continue;
            }

            // Then get phase text
            getline(txt_file, line);
            phase.text = line;                  // *** TODO *** sussy sussy

            // Fetch the phase options after
            for (uint8_t i = 0; i < phase.num_options; i++) {
                Option option;
            
                getline(txt_file, line);
                auto split_ind = line.find(',');
                option.phase_id = stoi(line.substr(0, split_ind));
                std::cout << "option - phase id = " << unsigned(option.phase_id) << ", ";
                option.text = line.substr(split_ind+1, std::string::npos);
                std::cout << option.text << std::endl;

                options.push_back(option);
            }

            // Skip blank line ahead
            getline(txt_file,line);

            phases.push_back(phase);
        }
    }

    ofstream out_bin(data_path("phase_state.bin"), std::ios::binary);
    std::cerr << "!!!!!!!! ERRROR: FILE DOESN'T EXIST" << strerror(errno);
    write_chunk("phas", phases, &out_bin);
    write_chunk("opts", options, &out_bin);

    return 0;
}