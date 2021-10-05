#include "Mode.hpp"

#include "Scene.hpp"
#include "Sound.hpp"
// #include "PhaseInfo.hpp"

#include <glm/glm.hpp>

#include <vector>
#include <deque>
#include <map>
#include <utility>

#include <ft2build.h>
#include <hb.h>
#include <hb-ft.h>

#include FT_FREETYPE_H

struct PlayMode : Mode {
	PlayMode();
	virtual ~PlayMode();

	//functions called by main loop:
	virtual bool handle_event(SDL_Event const &, glm::uvec2 const &window_size) override;
	virtual void update(float elapsed) override;
	virtual void draw(glm::uvec2 const &drawable_size) override;
	void fill_game_state();
	void render_text(uint32_t hb_index, float x, float y, glm::vec3 color);
	void setup_phase(size_t phase_id);

	// -------------------- Scene state --------------------

	//local copy of the game scene (so code can change it during gameplay):
	Scene scene;

	//camera:
	Scene::Camera *camera = nullptr;

	// -------------------- Game state --------------------
	typedef enum GS {
		ONGOING = 0,
		GOOD = 1,
		BAD = 2
	} GameState;
	
	
	struct Phase {
		GameState game_state = GameState::ONGOING;
		size_t id;										// ID of current phase
		std::vector<std::vector<char>> text;			// Text to display
		std::vector<size_t> option_ids; 				// IDs of options for the phase
		std::vector<std::vector<char>> option_texts; 	// Text of options for the phase
	};

	std::map<size_t, Phase> phases;		// Maps phase ID -> phase
	size_t current_phase_index = 0;		// ID of current game phase
	size_t selected_index = 0;			// Logs which choice the user selected
										// Constrained by number of options for each phase

	// -------------------- Text Drawing --------------------

	// Character->character map and glyph struct from https://learnopengl.com/In-Practice/Text-Rendering
	struct Character {
		unsigned int TextureID;  // ID handle of the glyph texture
		glm::ivec2   Size;       // Size of glyph
		glm::ivec2   Bearing;    // Offset from baseline to left/top of glyph
		unsigned int Advance;    // Offset to advance to next glyph
	};
	std::map<FT_ULong, Character> Characters;	// Reuse characters previously rendered

	// Since only one font is used, maintain font state globally
	FT_Library ft_lib;
	FT_Face ft_face;
	hb_font_t *hb_font; 
	std::vector<hb_buffer_t *> hb_buffers;	// Stores text to render for a given scene

	const float TEXT_START_X = 80.0f;
	const float TEXT_END_X = 1200.0f;
	const float TEXT_START_Y = 350.0f;
	const float TEXT_END_Y = 60.0f;
	const float HEIGHT = 40.0f;
};
