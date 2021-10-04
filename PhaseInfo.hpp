#pragma once
#include <vector>
#include <string>

struct Phase {
    uint8_t id;                   // ID of current phase
    std::string text;			  // Text to display
    uint8_t num_options;          // Stores # of options
};

struct Option {
    uint8_t phase_id;					// Stores ID of phase associated with this option (next phase)
    std::string text;						// Stores abbrev. text to display as phase option
};