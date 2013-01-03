#include "gfx/texture.h"
#include <memory.h>

namespace gfx
{

	Texture::Texture(int w, int h) :m_data(w * h), m_width(w), m_height(h) { }

	Texture::Texture(Texture &&rhs) :m_width(rhs.m_width), m_height(rhs.m_height) {
		m_data = std::move(rhs.m_data);
	}

	Texture::Texture() : m_width(0), m_height(0) { }

	void Texture::resize(int w, int h) {
		m_width  = w;
		m_height = h;
		m_data.resize(m_width * m_height);
	}
		
	void Texture::swap(Texture &tex) {
		std::swap(m_width, tex.m_width);
		std::swap(m_height, tex.m_height);
		m_data.swap(tex.m_data);
	}

	void Texture::clear() {
		m_width = m_height = 0;
		m_data.clear();
	}

	void Texture::fill(Color color) {
		for(int n = 0; n < m_data.size(); n++)
			m_data[n] = color;
	}

	bool Texture::testPixel(const int2 &pos) const {
		if(pos.x < 0 || pos.y < 0 || pos.x >= m_width || pos.y >= m_height)
			return false;

		return (*this)(pos.x, pos.y).a > 0;
	}

	void Texture::serialize(Serializer &sr) {
		if(sr.isSaving()) {
			saveTGA(sr);
			return;
		}

		string fileName = sr.name(), ext;
		{
			ext = fileName.substr(std::max(size_t(0), fileName.length() - 4));
			for(int n = 0, end = ext.length(); n < end; n++)
				ext[n] = tolower(ext[n]);
		}

		if(sr.isLoading()) {
			if(ext == ".png") loadPNG(sr);
			else if(ext == ".bmp") loadBMP(sr);
			else if(ext == ".tga") loadTGA(sr);
			else if(ext == ".zar") {
				PackedTexture packed;
				packed.legacyLoad(sr);
				packed.toTexture(*this);
			}
			else THROW("%s format is not supported (Only BMP, TGA and PNG for now)", ext.c_str());
		}
	}

}
