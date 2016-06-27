#pragma once

#include <string>
#include <map>

#include <glm/glm.hpp>

#include "Texture.h"
#include "Shader.h"

enum MaterialPropertyType
{
	MaterialPropertyType_vec3 = 0,
	MaterialPropertyType_vec4,
	MaterialPropertyType_texture,
	MaterialPropertyType_float,
	MaterialPropertyType_invalid
};

union MaterialPropertyValue
{
	MaterialPropertyValue() { memset(this, 0, sizeof(MaterialPropertyValue)); }
	MaterialPropertyValue(glm::vec3 vec3) : vec3(vec3) { }
	MaterialPropertyValue(glm::vec4 vec4) : vec4(vec4) { }
	MaterialPropertyValue(float flt) : flt(flt) { }
	MaterialPropertyValue(Texture texture) : texture(texture) { }

	glm::vec3 vec3;
	glm::vec4 vec4;
	float flt;
	Texture texture;
};

struct MaterialProperty
{
	MaterialProperty() : type(MaterialPropertyType_invalid) { }
	MaterialProperty(glm::vec3 vec3) : type(MaterialPropertyType_vec3), value(vec3) { }
	MaterialProperty(glm::vec4 vec4) : type(MaterialPropertyType_vec4), value(vec4) { }
	MaterialProperty(float flt) : type(MaterialPropertyType_float), value(flt) { }
	MaterialProperty(Texture texture) : type(MaterialPropertyType_texture), value(texture) { }

	MaterialPropertyType type;
	MaterialPropertyValue value;
};

class Material
{
public:
	Material();
	int drawOrder;
	MaterialProperty getProperty(const std::string& key);
	void setProperty(const std::string& key, MaterialProperty property);
	void apply(const Shader& shader) const;
private:
	std::map<std::string, MaterialProperty> properties;
};