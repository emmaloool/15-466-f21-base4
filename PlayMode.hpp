#include "Mode.hpp"

#include "Scene.hpp"
#include "Sound.hpp"
#include "PhaseInfo.hpp"

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
	virtual void render_text(uint32_t hb_index, float x, float y, glm::vec3 color);
	// -------------------- scene state --------------------

	//local copy of the game scene (so code can change it during gameplay):
	Scene scene;

	//camera:
	Scene::Camera *camera = nullptr;

	// -------------------- Game state --------------------
	

	// -------------------- Font Drawing --------------------

	// Character->character map and glyph struct from https://learnopengl.com/In-Practice/Text-Rendering
	struct Character {
		unsigned int TextureID;  // ID handle of the glyph texture
		glm::ivec2   Size;       // Size of glyph
		glm::ivec2   Bearing;    // Offset from baseline to left/top of glyph
		unsigned int Advance;    // Offset to advance to next glyph
	};
	std::map<FT_ULong, Character> Characters;

	// Since only one font is used, maintain font state globally
	FT_Library ft_lib;
	FT_Face ft_face;
	hb_font_t *hb_font; 
	std::vector<hb_buffer_t *> HB_Buffers;		// this
	uint32_t DUMMY_HB_INDEX = 0;

	std::vector<Phase> phases;
    std::vector<Option> options;	
};
