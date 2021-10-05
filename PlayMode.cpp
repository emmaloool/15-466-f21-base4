#include "PlayMode.hpp"
// #include "PhaseInfo.hpp"

#include "LitColorTextureProgram.hpp"
#include "TextTextureProgram.hpp" // Load to use for text
#include "read_write_chunk.hpp"
#include "DrawLines.hpp"
#include "Mesh.hpp"
#include "Load.hpp"
#include "gl_errors.hpp"
#include "data_path.hpp"

#include <glm/gtc/type_ptr.hpp>
#include <iostream>
#include <fstream>
#include <algorithm>

#include <random>

GLuint interview_meshes_for_lit_color_texture_program = 0;
Load< MeshBuffer > interview_meshes(LoadTagDefault, []() -> MeshBuffer const * {
	MeshBuffer const *ret = new MeshBuffer(data_path("interview.pnct"));
	interview_meshes_for_lit_color_texture_program = ret->make_vao_for_program(lit_color_texture_program->program);
	return ret;
});

Load< Scene > interview_scene(LoadTagDefault, []() -> Scene const * {
	return new Scene(data_path("interview.scene"), [&](Scene &scene, Scene::Transform *transform, std::string const &mesh_name){
		Mesh const &mesh = interview_meshes->lookup(mesh_name);

		scene.drawables.emplace_back(transform);
		Scene::Drawable &drawable = scene.drawables.back();

		drawable.pipeline = lit_color_texture_program_pipeline;

		drawable.pipeline.vao = interview_meshes_for_lit_color_texture_program;
		drawable.pipeline.type = mesh.type;
		drawable.pipeline.start = mesh.start;
		drawable.pipeline.count = mesh.count;

	});
});

GLuint VAO, VBO;

void PlayMode::fill_game_state() {
	std::fstream txt_file;
    std::string line;
    txt_file.open(data_path("../script.txt"), std::ios::in);
    if (txt_file.is_open()) {
        while (getline(txt_file, line)) {
            Phase phase;

            // Read phases as clumps of lines, so we can't just loop over all lines
			auto f_i = line.find(' ');
			auto s_i = line.find(' ', f_i+1);
            phase.id = stoi(line.substr(0, f_i));
            uint8_t num_plines = stoi(line.substr(f_i+1, s_i)); 	// tells us how many phase text lines to read
            uint8_t num_options = stoi(line.substr(s_i)); 	// tells us how many option lines to read
			// std::cout << unsigned(phase.id) << " " << unsigned(num_plines) << " " << unsigned(num_options) << std::endl;

			// Add lines for phase's text
			for (uint8_t i = 0; i < num_plines; i++) {
				getline(txt_file, line);

				std::vector<char> p_text_line(line.begin(), line.end());
				phase.text.push_back(p_text_line);
			}

            if (num_options == 0) {		// Game end state, no more options
                getline(txt_file, line);
				if (line == "-") {
					// std::cout << "phase " << unsigned(phase.id) << " is bad ending!" << std::endl;
					phase.game_state = GameState::BAD;
				}
				else if (line == "+") {
					// std::cout << "phase " << unsigned(phase.id) << " is good ending!" << std::endl;
					phase.game_state = GameState::GOOD;
				}

				// Skip blank line after and move on
                getline(txt_file, line);
				continue;
            }
	
            // Fetch the phase options after
            for (uint8_t i = 0; i < num_options; i++) {				
                getline(txt_file, line);

				// Save option's corresponding phase index
                auto split_ind = line.find(',');
                phase.option_ids.push_back(stoi(line.substr(0, split_ind)));

				// Save option text
				std::vector<char> op_text(line.begin() + split_ind + 1, line.end());
				phase.option_texts.push_back(op_text);
			}

			if (num_options == 1) {
				phase.option_texts.back().clear();
			}

            getline(txt_file,line);		// Skip blank line ahead

            phases[phase.id] = phase;
        }
    }
}

PlayMode::PlayMode() : scene(*interview_scene) {

	//get pointer to camera for convenience:
	if (scene.cameras.size() != 1) throw std::runtime_error("Expecting scene to have exactly one camera, but it has " + std::to_string(scene.cameras.size()));
	camera = &scene.cameras.front();

	// The rest of the implementation of this constructor heavily utilizes code from https://learnopengl.com/In-Practice/Text-Rendering
	
	// For text shader later
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	// Setup FT, HB font - code from https://learnopengl.com/In-Practice/Text-Rendering
	{
		if (FT_Init_FreeType(&ft_lib)) throw std::runtime_error("ERROR::FREETYPE: Could not init FreeType Library");
		if (FT_New_Face(ft_lib, const_cast<char *>(data_path("../fonts/Roboto-Regular.ttf").c_str()), 0, &ft_face)) throw std::runtime_error("ERROR::FREETYPE: Failed to load font");		// TODO use datapath for this please...
		FT_Set_Char_Size(ft_face, 0, 1800, 0,0);
		if (FT_Load_Char(ft_face, 'X', FT_LOAD_RENDER)) throw std::runtime_error("ERROR::FREETYTPE: Failed to load Glyph");
		glPixelStorei(GL_UNPACK_ALIGNMENT, 1); // disable byte-alignment restriction

		/* Create hb-ft font. */
		hb_font = hb_ft_font_create_referenced(ft_face); // want this to last the lifetime of the program so make it referenced
	}

	// Back to following https://learnopengl.com/In-Practice/Text-Rendering - 
	// configure VAO/VBO for texture quads
	// -----------------------------------
	{
		glGenVertexArrays(1, &VAO);
		glGenBuffers(1, &VBO);
		glBindVertexArray(VAO);
		glBindBuffer(GL_ARRAY_BUFFER, VBO);
		glBufferData(GL_ARRAY_BUFFER, sizeof(float) * 6 * 4, NULL, GL_DYNAMIC_DRAW);
		glEnableVertexAttribArray(0);
		glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 4 * sizeof(float), 0);
		glBindBuffer(GL_ARRAY_BUFFER, 0);
		glBindVertexArray(0);
	}
	
	fill_game_state();	// Fill game state from text script
	setup_phase(0);
}

PlayMode::~PlayMode() {
	// Destroy FT, HB objects in reverse order of creation
	for (auto buf : hb_buffers) hb_buffer_destroy(buf);
	hb_buffers.clear();

	hb_font_destroy(hb_font);
	FT_Done_Face(ft_face);
	FT_Done_FreeType(ft_lib);
}


bool PlayMode::handle_event(SDL_Event const &evt, glm::uvec2 const &window_size) {
	Phase cur_phase = phases[current_phase_index];

	if (evt.type == SDL_KEYDOWN) {
		if (evt.key.keysym.sym == SDLK_RETURN) {
			std::cout << "Currently on phase " << current_phase_index << std::endl;
			// Procure the ID of the phase associated with the choice
			current_phase_index = cur_phase.option_ids[selected_index];
			std::cout << "*** Option index was " << selected_index << ", new phase ID = " << current_phase_index << std::endl;
			setup_phase(current_phase_index);
			return true;
		} else if (evt.key.keysym.sym == SDLK_UP) {
			if (selected_index > 0) {
				selected_index--;
				std::cout << "new op index: " << unsigned(selected_index) << std::endl;
			}
			return true;
		} else if (evt.key.keysym.sym == SDLK_DOWN) {
			if (selected_index < (cur_phase.option_texts.size() - 1)) {
				selected_index++;
				std::cout << "new op index: " << unsigned(selected_index) << std::endl;
			}
			return true;
		}
	}
	assert((selected_index >= 0) && (selected_index < (cur_phase.option_texts.size())));
	return false;
}


void PlayMode::update(float elapsed) {
	// Guess there's nothing to do ¯\_(ツ)_/¯ 
}


void PlayMode::render_text(uint32_t hb_index, float x, float y, glm::vec3 color) {
	// Render text - again following https://learnopengl.com/In-Practice/Text-Rendering, function RenderText()
	// Setup character render state to render text 
	
	// Our render state is associated with the shader program color_texture_program
	glUseProgram(text_texture_program->program);
	// According to the text rendering tutorial:
	// "For rendering text we (usually) do not need perspective, and using an 
	// orthographic projection matrix also allows us to specify all vertex coordinates in screen coordinates"
	glm::mat4 projection = glm::ortho(0.0f, 1280.0f, 0.0f, 720.0f);
	// (Again) according to the text rendering tutorial:
	// "We set the projection matrix's bottom parameter to 0.0f 
	// and its top parameter equal to the window's height. 
	// The result is that we specify coordinates with y values ranging from 
	// the bottom part of the screen (0.0f) to the top part of the screen (600.0f). 
	// This means that the point (0.0, 0.0) now corresponds to the bottom-left corner.
	// So we have to pass a pointer to projection[0][0] to the program's projection matrix
	// as "values" parameter of glUniformMatrix4fv
	glUniformMatrix4fv(text_texture_program->OBJECT_TO_CLIP_mat4, 1, GL_FALSE, &projection[0][0]);
	// Since the shader is programmed to take in a text color as uniform, we also need to set this now
	glUniform3f(text_texture_program->Color_vec3, color.x, color.y, color.z);		// TODO dynamically set colors here, read in as game struct
	glActiveTexture(GL_TEXTURE0);
	glBindVertexArray(VAO);
	glDisable(GL_DEPTH_TEST);

	// Get current HB buffer
	hb_buffer_t *DUMMY_TEST = hb_buffers[hb_index];

	// Harbuzz buffer processing code from http://www.manpagez.com/html/harfbuzz/harfbuzz-2.3.1/ch03s03.php
	// Get glyph and position information
	unsigned int glyph_count;
	hb_glyph_info_t *glyph_info = hb_buffer_get_glyph_infos(DUMMY_TEST, &glyph_count);

	// Iterate through all the necessary glyphs to render in this buffer
    for (uint8_t i = 0; i < glyph_count; i++)
    {
		// From https://learnopengl.com/In-Practice/Text-Rendering
		// "For each character, we generate a texture and store its relevant data 
		// into a Character struct that we add to the Characters map. 
		// This way, all data required to render each character is stored for later use."
		// So we need to lookup if this character has been previously stored in the character map
		FT_ULong c = glyph_info[i].codepoint;
		if (Characters.find(c) == Characters.end()) { // Character not previously rendered
			// Load character glyph
			if (FT_Load_Glyph(ft_face, c, FT_LOAD_RENDER))
				std::runtime_error("ERROR::FREETYTPE: Failed to load Glyph");
			
			// Generate texture 
			unsigned int texture;
			glGenTextures(1, &texture);
			glBindTexture(GL_TEXTURE_2D, texture);
			glTexImage2D(
				GL_TEXTURE_2D,
				0,
				GL_RED,
				ft_face->glyph->bitmap.width,
				ft_face->glyph->bitmap.rows,
				0,
				GL_RED,
				GL_UNSIGNED_BYTE,
				ft_face->glyph->bitmap.buffer
			);

			// Set texture options
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

			// Now store character for later use
			Character character = {
				texture, 
				glm::ivec2(ft_face->glyph->bitmap.width, ft_face->glyph->bitmap.rows),
				glm::ivec2(ft_face->glyph->bitmap_left, ft_face->glyph->bitmap_top),
				(unsigned int) ft_face->glyph->advance.x
			};
			Characters.insert(std::pair<FT_ULong, Character>(c, character));
		}

		// Now character should be present in the map at the codepoint!
        Character ch = Characters[c];

        float xpos = x + ch.Bearing.x;
        float ypos = y - (ch.Size.y - ch.Bearing.y);

        float w = ch.Size.x;
        float h = ch.Size.y;
        // update VBO for each character
        float vertices[6][4] = {
            { xpos,     ypos + h,   0.0f, 0.0f },            
            { xpos,     ypos,       0.0f, 1.0f },
            { xpos + w, ypos,       1.0f, 1.0f },

            { xpos,     ypos + h,   0.0f, 0.0f },
            { xpos + w, ypos,       1.0f, 1.0f },
            { xpos + w, ypos + h,   1.0f, 0.0f }           
        };
        // render glyph texture over quad
        glBindTexture(GL_TEXTURE_2D, ch.TextureID);
        // update content of VBO memory
        glBindBuffer(GL_ARRAY_BUFFER, VBO);
        glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(vertices), vertices); 
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        // render quad
        glDrawArrays(GL_TRIANGLES, 0, 6);
        // now advance cursors for next glyph (note that advance is number of 1/64 pixels)
        x += (ch.Advance >> 6); // bitshift by 6 to get value in pixels (2^6 = 64)
    }

	glBindVertexArray(0);
    glBindTexture(GL_TEXTURE_2D, 0);
}


void PlayMode::setup_phase(size_t phase_id) {

	// Clear text buffer
	for (auto buf : hb_buffers) hb_buffer_destroy(buf);
	hb_buffers.clear();

	// Reset option index
	selected_index = 0;

	// Procur phase at current ID
	Phase current_phase = phases[phase_id];

	// Populate HB buffers needed to render the entire scene
	auto add_text_to_HBbuf = [this](std::vector<char> text) {
		// Setup HB buffer - code from https://github.com/harfbuzz/harfbuzz-tutorial/blob/master/hello-harfbuzz-freetype.c
		// and http://www.manpagez.com/html/harfbuzz/harfbuzz-2.3.1/ch03s03.php

		/* Create hb-buffer and populate. */
		hb_buffer_t *buf = hb_buffer_create();
		hb_buffer_add_utf8(buf, &text[0], text.size(), 0, text.size());

		/* Guess the script/language/direction */
		hb_buffer_set_direction(buf, HB_DIRECTION_LTR);
		hb_buffer_set_script(buf, HB_SCRIPT_LATIN);
		hb_buffer_set_language(buf, hb_language_from_string("en",-1));

		/* Shape it! */
		hb_shape(hb_font, buf, NULL, 0);

		hb_buffers.push_back(buf);
	};

	for (std::vector<char> phase_txt : current_phase.text) {
		add_text_to_HBbuf(phase_txt);
	}
	for (std::vector<char> op_text : current_phase.option_texts) {
		add_text_to_HBbuf(op_text);
	}
}


void PlayMode::draw(glm::uvec2 const &drawable_size) {
	// Camera
	{
		//update camera aspect ratio for drawable:
		camera->aspect = float(drawable_size.x) / float(drawable_size.y);

		//set up light type and position for lit_color_texture_program:
		// TODO: consider using the Light(s) in the scene to do this
		glUseProgram(lit_color_texture_program->program);
		glUniform1i(lit_color_texture_program->LIGHT_TYPE_int, 1);
		glUniform3fv(lit_color_texture_program->LIGHT_DIRECTION_vec3, 1, glm::value_ptr(glm::vec3(0.0f, 0.0f,-1.0f)));
		glUniform3fv(lit_color_texture_program->LIGHT_ENERGY_vec3, 1, glm::value_ptr(glm::vec3(1.0f, 1.0f, 0.95f)));
		glUseProgram(0);

		glClearColor(0.5f, 0.5f, 0.5f, 1.0f);
		glClearDepth(1.0f); //1.0 is actually the default value to clear the depth buffer to, but FYI you can change it.
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		glEnable(GL_DEPTH_TEST);
		glDepthFunc(GL_LESS); //this is the default depth comparison function, but FYI you can change it.

		scene.draw(*camera);
	}	

	if (phases[current_phase_index].game_state != GameState::ONGOING) {
		// render_text(0, ); 	// TODO: make sure to clear HBbuffer before and add one hbbuffer that says "game over"
		
		std::cout << "WE ARE STOPPING THE GAME" << std::endl;
		return;
	}

	uint8_t i = 0;
	for (i = 0; i < phases[current_phase_index].text.size(); i++) {
		render_text(i, TEXT_START_X, TEXT_START_Y - HEIGHT*i, glm::vec3(0.0f, 0.0f, 0.0f));
	}
	for (uint8_t j = i; j < hb_buffers.size(); j++) {
		glm::vec3 color = ((j - i) == selected_index) ? glm::vec3(0.25f, 0.9f, 0.25f) : glm::vec3(0.8f, 0.8f, 0.8f);
		render_text(j, TEXT_START_X, TEXT_START_Y - HEIGHT*(j+1), color);
	}
}
