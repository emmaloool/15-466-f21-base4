#include "PlayMode.hpp"

#include "LitColorTextureProgram.hpp"
#include "TextTextureProgram.hpp" // Load to use for text
#include "DrawLines.hpp"
#include "Mesh.hpp"
#include "Load.hpp"
#include "gl_errors.hpp"
#include "data_path.hpp"

#include <glm/gtc/type_ptr.hpp>

#include <random>

GLuint hexapod_meshes_for_lit_color_texture_program = 0;
Load< MeshBuffer > hexapod_meshes(LoadTagDefault, []() -> MeshBuffer const * {
	MeshBuffer const *ret = new MeshBuffer(data_path("hexapod.pnct"));
	hexapod_meshes_for_lit_color_texture_program = ret->make_vao_for_program(lit_color_texture_program->program);
	return ret;
});

Load< Scene > hexapod_scene(LoadTagDefault, []() -> Scene const * {
	return new Scene(data_path("hexapod.scene"), [&](Scene &scene, Scene::Transform *transform, std::string const &mesh_name){
		Mesh const &mesh = hexapod_meshes->lookup(mesh_name);

		scene.drawables.emplace_back(transform);
		Scene::Drawable &drawable = scene.drawables.back();

		drawable.pipeline = lit_color_texture_program_pipeline;

		drawable.pipeline.vao = hexapod_meshes_for_lit_color_texture_program;
		drawable.pipeline.type = mesh.type;
		drawable.pipeline.start = mesh.start;
		drawable.pipeline.count = mesh.count;

	});
});

GLuint VAO, VBO;

PlayMode::PlayMode() : scene(*hexapod_scene) {
	//get pointer to camera for convenience:
	if (scene.cameras.size() != 1) throw std::runtime_error("Expecting scene to have exactly one camera, but it has " + std::to_string(scene.cameras.size()));
	camera = &scene.cameras.front();

	// ***** The rest of the implementation of this constructor heavily utilizes code from https://learnopengl.com/In-Practice/Text-Rendering

	// Initial setup work for the text shader
	{
		// OpenGL state
		// ------------
		glEnable(GL_CULL_FACE);
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	}

	// Setup font
	{
		// FT setup code from https://learnopengl.com/In-Practice/Text-Rendering
		if (FT_Init_FreeType(&DUMMY_FT)) throw std::runtime_error("ERROR::FREETYPE: Could not init FreeType Library");

		if (FT_New_Face(DUMMY_FT, "fonts/Roboto-Regular.ttf", 0, &DUMMY_FACE)) {		// TODO use datapath for this please...
			std::cout << "*** where is this ***" << std::endl;
			throw std::runtime_error("ERROR::FREETYPE: Failed to load font");
		}
		FT_Set_Char_Size(DUMMY_FACE, 0, 1200, 0,0); // TODO unnecessary? or use FT_Set_Char_Size to set font size
		if (FT_Load_Char(DUMMY_FACE, 'X', FT_LOAD_RENDER)) throw std::runtime_error("ERROR::FREETYTPE: Failed to load Glyph");
		glPixelStorei(GL_UNPACK_ALIGNMENT, 1); // disable byte-alignment restriction

		// Setup Harbuz to shape font - setup code from https://github.com/harfbuzz/harfbuzz-tutorial/blob/master/hello-harfbuzz-freetype.c
		/* Create hb-ft font. */
		hb_font_t *hb_font;
		hb_font = hb_ft_font_create(DUMMY_FACE, NULL);

		/* Create hb-buffer and populate. */
		hb_buffer_t *DUMMY_TEST = hb_buffer_create();
		hb_buffer_add_utf8(DUMMY_TEST, "Hello, I am on the screen!", -1, 0, -1);
		hb_buffer_guess_segment_properties(DUMMY_TEST);
		/* Shape it! */
		hb_shape(hb_font, DUMMY_TEST, NULL, 0);
		// Add it to list of tracked buffers
		HB_Buffers.push_back(DUMMY_TEST);
		
		// Back to following https://learnopengl.com/In-Practice/Text-Rendering - 
		// configure VAO/VBO for texture quads
		// -----------------------------------
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
}

PlayMode::~PlayMode() {
	// --- TODO: For dummy test, runs for entire duration of game
	// So cleanup is done here, but remove this later!
	// jk we will use a single font for the duration of the game...
	FT_Done_Face(DUMMY_FACE);
	FT_Done_FreeType(DUMMY_FT);		// Discard when done
}

bool PlayMode::handle_event(SDL_Event const &evt, glm::uvec2 const &window_size) {

	return true;
}

void PlayMode::update(float elapsed) {

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
	glUniform3f(text_texture_program->Color_vec3, 0.0f, 0.0f, 0.0f);		// TODO dynamically set colors here, read in as game struct
	glActiveTexture(GL_TEXTURE0);
	glBindVertexArray(VAO);

	// Get current HB buffer
	hb_buffer_t *DUMMY_TEST = HB_Buffers[hb_index];

	// Harbuzz buffer processing code from http://www.manpagez.com/html/harfbuzz/harfbuzz-2.3.1/ch03s03.php
	// Get glyph and position information
	unsigned int glyph_count;
	hb_glyph_info_t *glyph_info    = hb_buffer_get_glyph_infos(DUMMY_TEST, &glyph_count);

	// Iterate through all the necessary glyphs to render in this buffer
    for (uint8_t i = 0; i < glyph_count; i++)
    {
		// From https://learnopengl.com/In-Practice/Text-Rendering
		// "For each character, we generate a texture and store its relevant data 
		// into a Character struct that we add to the Characters map. 
		// This way, all data required to render each character is stored for later use."
		// So we need to lookup if this character has been previously stored in the character map
		char c = glyph_info[i].codepoint;
		if (Characters.find(c) == Characters.end()) { // Character not previously rendered
			// Load character glyph
			if (FT_Load_Char(DUMMY_FACE, c, FT_LOAD_RENDER))
				std::runtime_error("ERROR::FREETYTPE: Failed to load Glyph");
			
			// Generate texture 
			unsigned int texture;
			glGenTextures(1, &texture);
			glBindTexture(GL_TEXTURE_2D, texture);
			glTexImage2D(
				GL_TEXTURE_2D,
				0,
				GL_RED,
				DUMMY_FACE->glyph->bitmap.width,
				DUMMY_FACE->glyph->bitmap.rows,
				0,
				GL_RED,
				GL_UNSIGNED_BYTE,
				DUMMY_FACE->glyph->bitmap.buffer
			);

			// Set texture options
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

			// Now store character for later use
			Character character = {
				texture, 
				glm::ivec2(DUMMY_FACE->glyph->bitmap.width, DUMMY_FACE->glyph->bitmap.rows),
				glm::ivec2(DUMMY_FACE->glyph->bitmap_left, DUMMY_FACE->glyph->bitmap_top),
				(unsigned int) DUMMY_FACE->glyph->advance.x
			};
			Characters.insert(std::pair<char, Character>(c, character));
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

	render_text(DUMMY_HB_INDEX, 150.0f, 300.0f, glm::vec3(0.1f, 0.1f, 0.1f));
}
