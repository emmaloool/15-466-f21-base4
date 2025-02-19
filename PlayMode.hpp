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
	void render_text(uint32_t hb_index, float x, float y, glm::vec3 color, uint8_t font);
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
		std::vector<uint8_t> fonts;
		std::vector<std::vector<char>> text;			// Text to display

		std::vector<size_t> option_ids; 				// IDs of options for the phase
		std::vector<uint8_t> option_fonts;				// Bits setting font for option
		std::vector<std::vector<char>> option_texts; 	// Text of options for the phase
	};

	std::map<size_t, Phase> phases;		// Maps phase ID -> phase
	size_t current_phase_index = 0;		// ID of current game phase
	size_t selected_index = 0;			// Logs which choice the user selected
										// Constrained by number of options for each phase

	// -------------------- Text Drawing --------------------

	const float TEXT_START_X = 80.0f;
	const float TEXT_START_Y = 500.0f;
	const float HEIGHT = 40.0f;

	// Character->character map and glyph struct from https://learnopengl.com/In-Practice/Text-Rendering
	struct Character {
		unsigned int TextureID;  // ID handle of the glyph texture
		glm::ivec2   Size;       // Size of glyph
		glm::ivec2   Bearing;    // Offset from baseline to left/top of glyph
		unsigned int Advance;    // Offset to advance to next glyph
	};
	std::map<FT_ULong, Character> roboto_characters;	// Reuse characters previously rendered
	std::map<FT_ULong, Character> courier_characters;	// Reuse characters previously rendered

	// Two fonts are loaded for the game - Roboto and Courier New
	FT_Library roboto_lib;
	FT_Face roboto_face;
	hb_font_t *hb_roboto_font; 

	FT_Library courier_lib;
	FT_Face courier_face;
	hb_font_t *hb_courier_font; 

	// Stores text to render for a given scene
	std::vector<hb_buffer_t *> hb_buffers;

	void add_text_to_HBbuf(std::vector<char> text, hb_font_t *font) {
		// Setup HB buffer - code from https://github.com/harfbuzz/harfbuzz-tutorial/blob/master/hello-harfbuzz-freetype.c
		// and http://www.manpagez.com/html/harfbuzz/harfbuzz-2.3.1/ch03s03.php

		/* Create hb-buffer and populate. */
		hb_buffer_t *buf = hb_buffer_create();
		hb_buffer_add_utf8(buf, &text[0], (int)text.size(), 0, (int)text.size());

		/* Guess the script/language/direction */
		hb_buffer_set_direction(buf, HB_DIRECTION_LTR);
		hb_buffer_set_script(buf, HB_SCRIPT_LATIN);
		hb_buffer_set_language(buf, hb_language_from_string("en",-1));

		/* Shape it! */
		hb_shape(font, buf, NULL, 0);

		hb_buffers.push_back(buf);
	}
};
