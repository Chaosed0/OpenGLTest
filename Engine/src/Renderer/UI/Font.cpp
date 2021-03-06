
#include "Renderer/UI/Font.h"

#include <GL/glew.h>

#include <algorithm>
#include <sstream>

#include <ft2build.h>
#include <freetype/freetype.h>

#include <SDL.h>

#include "Renderer/Texture.h"
#include "Renderer/TexturePacker.h"

struct Font::Impl
{
	Impl() : texturePacker(glm::ivec2(64, 64)) { }

	/*! The OpenGL texture ID for the font atlas. */
	GLuint textureID;

	/*! The characters which were loaded during loadCharacters. */
	std::vector<Character> characters;

	/*! The texture packer we used to pack the character textures. */
	TexturePacker texturePacker;

	/*!
	 * \brief Packs a single character into the font atlas.
	 * \param character The character to pack. Everything must be filled except origin. The origin field
	 *		is filled with the top-left position of the character within the font atlas.
	 * \param bitmap The actual glyph data which we pack into the atlas.
	 */
	void packCharacter(Character& character, const FT_Bitmap& bitmap);
};

class SortCharacterDescending
{
public:
	SortCharacterDescending(std::vector<Character>& characters) : characters(characters) { }
	bool operator () (int i1, int i2)
	{
		Character& c1 = characters[i1];
		Character& c2 = characters[i2];
		return (c1.size.x * c1.size.y) > (c2.size.x * c2.size.y);
	}
private:
	std::vector<Character>& characters;
};

Font::Font()
	: impl(new Impl())
{ }

Font::~Font()
{ }

Font::Font(const std::string& fontPath, int height)
	: Font()
{
	this->loadCharacters(fontPath, height);
}

void Font::loadCharacters(const std::string& fontPath, int height)
{
	FT_Library ft;
	if (FT_Init_FreeType(&ft)) {
		fprintf(stderr, "ERROR::FREETYPE: Could not init FreeType Library");
		return;
	}

	FT_Face face;
	if (FT_New_Face(ft, fontPath.c_str(), 0, &face)) {
		fprintf(stderr, "ERROR::FREETYPE: Failed to load font %s\n", fontPath.c_str());
		return;
	}

	impl->characters.clear();

	FT_Set_Pixel_Sizes(face, 0, height);

	for (int i = 0; i < 128; i++)
	{
		char c = (char)i;
		// Load character glyph
		if (FT_Load_Char(face, c, FT_LOAD_RENDER))
		{
			fprintf(stderr, "ERROR::FREETYTPE: Failed to load Glyph %c", c);
			continue;
		}

		// Now store character for later use
		Character character = {
			glm::ivec2(0,0),
			glm::ivec2(face->glyph->bitmap.width, face->glyph->bitmap.rows),
			glm::ivec2(face->glyph->bitmap_left, face->glyph->bitmap_top),
			(long)face->glyph->advance.x,
		};
		impl->characters.push_back(character);
	}

	SortCharacterDescending descending(impl->characters);
	std::vector<int> sortedCharacters(impl->characters.size());
	for (size_t i = 0; i < sortedCharacters.size(); i++) {
		sortedCharacters[i] = i;
	}
	std::sort(sortedCharacters.begin(), sortedCharacters.end(), descending);

	int i = 0;
	for (size_t i = 0; i < sortedCharacters.size(); i++)
	{
		int charIndex = sortedCharacters[i];
		Character& character = impl->characters[charIndex];
		if (FT_Load_Char(face, charIndex, FT_LOAD_RENDER))
		{
			fprintf(stderr, "ERROR::FREETYTPE: Failed to load Glyph %c", charIndex);
			continue;
		}

		if (isprint(charIndex))
		{
			impl->packCharacter(character, face->glyph->bitmap);

			if (character.size.x > 0 && character.size.y > 0) {
				int basenameIndex = fontPath.find_last_of('/');
				std::string basename = fontPath.substr(basenameIndex+1);
				std::stringstream sstream;
				sstream << "data/" << basename << "_" << height << "_" << i << ".bmp";
				saveAtlasToFile(sstream.str());
			}
		}
	}

	glm::ivec2 textureSize = impl->texturePacker.getTextureSize();
	const unsigned char* buffer = impl->texturePacker.getBuffer();

	FT_Done_Face(face);
	FT_Done_FreeType(ft);

	// Give OpenGL the data
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	glGenTextures(1, &impl->textureID);
	glBindTexture(GL_TEXTURE_2D, impl->textureID);
	glTexImage2D( GL_TEXTURE_2D, 0, GL_RED, textureSize.x, textureSize.y, 0, GL_RED, GL_UNSIGNED_BYTE, buffer);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
}

void Font::saveAtlasToFile(const std::string& file) const
{
	glm::ivec2 textureSize = impl->texturePacker.getTextureSize();
	const unsigned char* buffer = impl->texturePacker.getBuffer();

	SDL_Surface* surface = SDL_CreateRGBSurface(0, textureSize.x, textureSize.y, 8,0,0,0,0);
	SDL_Color colors[256];
	for (int i = 0; i < 256; i++) {
		colors[i].r = colors[i].b = colors[i].g = i;
		colors[i].a = 255;
	}
	SDL_SetPaletteColors(surface->format->palette, colors, 0, 256);
	surface->pixels = (void*)buffer;

	SDL_SaveBMP(surface, file.c_str());
	SDL_FreeSurface(surface);
}

Character Font::getCharacter(unsigned int i) const
{
	if (i < impl->characters.size()) {
		return impl->characters[i];
	}
	return Character { glm::ivec2(0,0), glm::ivec2(0,0), glm::ivec2(0,0), 0 };
}

void Font::Impl::packCharacter(Character& character, const FT_Bitmap& bitmap)
{
	if (character.size.x == 0 || character.size.y == 0) {
		return;
	}

	glm::ivec2 origin = texturePacker.packTexture(bitmap.buffer, glm::ivec2(bitmap.pitch, bitmap.rows));
	character.origin = origin;
}

Texture Font::getTexture() const
{
	Texture texture;
	texture.impl->type = TextureType_diffuse;
	texture.impl->id = impl->textureID;
	return texture;
}

glm::ivec2 Font::getTextureSize() const
{
	return impl->texturePacker.getTextureSize();
}
