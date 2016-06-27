#pragma once

#include <Windows.h>
#include <GL/glew.h>
#include <GL/gl.h>
#include <GL/glu.h>

#include <glm/glm.hpp>

#include <unordered_map>
#include <map>

#include "Model.h"
#include "Material.h"
#include "Camera.h"
#include "RenderUtil.h"
#include "Shader.h"

/*! A point light in space. */
struct PointLight
{
    glm::vec3 position;

	float constant;
	float linear;
	float quadratic;
  
    glm::vec3 ambient;
    glm::vec3 diffuse;
    glm::vec3 specular;
};

/*! A global directional light. */
struct DirLight
{
    glm::vec3 direction;
  
    glm::vec3 ambient;
    glm::vec3 diffuse;
    glm::vec3 specular;
};  

/*! Shader cache. Stores a shader along with its uniform locations. */
struct ShaderCache
{
	ShaderCache(const Shader& shader);

	struct PointLightCache
	{
		GLuint position;

		GLuint constant;
		GLuint linear;
		GLuint quadratic;
	  
		GLuint ambient;
		GLuint diffuse;
		GLuint specular;
	};

	struct DirLightCache
	{
		GLuint direction;
		GLuint ambient;
		GLuint diffuse;
		GLuint specular;
	};  

	Shader shader;
	DirLightCache dirLight;
	std::vector<PointLightCache> pointLights;
	std::vector<GLuint> bones;
};

/*! A renderable, stored internally in the renderer.
	Renderables can be equated to entities. They reference a model and a shader, but have
	their own transforms. */
struct Renderable
{
	Renderable(const ShaderCache& shaderCache, const Model& model, Transform transform)
		: shaderCache(shaderCache), model(model), transform(transform) { }

	ShaderCache shaderCache;
	Model model;
	Transform transform;

	/*! Current animation playing. */
	std::string animName;
	
	/*! Current time within the animation. */
	float time;
};

class Renderer
{
public:
	Renderer();

	/*!
	 * \brief Initializes the renderer. Must be called after GL initialization, and before draw() is called.
	 */
	bool initialize();

	/*!
	 * \brief Sets a callback to call in the event of an opengl debug message.
	 */
	void setDebugLogCallback(const DebugLogCallback& callback);

	/*!
	 * \brief Sets the current camera which this renderer will use.
	 */
	void setCamera(Camera* camera);

	/*!
	 * \brief Sets the directional light. Currently, only one is supported.
	 */
	void setDirLight(DirLight dirLight);

	/*!
	 * \brief Sets a point light.
	 * \param index Must be less than maxPointLights().
	 */
	void setPointLight(unsigned int index, PointLight pointLight);

	/*!
	 * \ brief Gets the maximum point lights allowed by the renderer.
	 */
	unsigned int getMaxPointLights();

	/*!
	 * \brief Gets a handle to a renderable object.
	 *
	 * If type is RenderableTypeDynamic, then model must have a unique ID assigned to the
	 * id field.
	 * If type is RenderableTypeStatic, then the model id is ignored (one is assigned
	 * internally).
	 */
	unsigned int getHandle(const Model& model, const Shader& shader);

	/*!
	 * \brief Updates the transform of a renderable object. The handle must have been
	 *		specified as RenderableTypeDynamic.
	 */
	void updateTransform(unsigned int handle, const Transform& transform);

	/*!
	 * \brief Updates the animation of a renderable object.
	 *		The animation will loop.
	 */
	void setAnimation(unsigned int handle, const std::string& animation);

	/*! 
	 * \brief Sets the time at which to start the currently playing
	 *		animation for the given object.
	 */
	void setAnimationTime(unsigned int handle, float time);

	/*!
	 * \brief Draws all renderable objects requested using getHandle.
	 */
	void draw();

	/*!
	 * \brief Steps the animations of all renderable objects by the given amount.
	 * \param dt Step, in seconds.
	 */
	void update(float dt);
private:
	/*! Reference to the current camera. */
	Camera* camera;

	/*! The global directional light. */
	DirLight dirLight;

	/*! Point lights. */
	std::vector<PointLight> pointLights;

	/*! Map of shader ids to shaders. */
	std::map<unsigned, Shader> shaderMap;
	
	/*! Map of renderable handles to Renderables. */
	std::map<unsigned, Renderable> renderableMap;

	/*! The callback to call when an OpenGL debug message is emitted. */
	DebugLogCallback debugLogCallback;

	/*! ID to assign to the next renderable requested through getHandle(). */
	unsigned nextHandle;
};