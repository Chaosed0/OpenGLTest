
#include "BulletDebugDrawer.h"

#include <GL/glew.h>
#include <Windows.h>
#include <GL/GL.h>
#include <GL/GLU.h>

BulletDebugDrawer::BulletDebugDrawer()
	: currentBufferSize(0),
	i(0),
	debugMode(btIDebugDraw::DBG_NoDebug)
{ }

BulletDebugDrawer::~BulletDebugDrawer()
{ }

void BulletDebugDrawer::initialize()
{
	shader.compileAndLink("Shaders/colors.vert", "Shaders/colors.frag");

	glGenVertexArrays(1, &VAO);
	glGenBuffers(1, &VBO);
	glGenBuffers(1, &VBO_color);
	
	glBindVertexArray(VAO);
	glBindBuffer(GL_ARRAY_BUFFER, VBO);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(GLfloat), 0);
	glBindBuffer(GL_ARRAY_BUFFER, VBO_color);
	glEnableVertexAttribArray(1);
	glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(GLfloat), 0);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindVertexArray(0);
}

void BulletDebugDrawer::setCamera(Camera* camera)
{
	this->camera = camera;
}

void BulletDebugDrawer::reset()
{
	i = 0;
}

void BulletDebugDrawer::draw()
{
	shader.use();
	shader.setModelMatrix(&glm::mat4()[0][0]);
	shader.setProjectionMatrix(&camera->getProjectionMatrix()[0][0]);
	shader.setViewMatrix(&camera->getViewMatrix()[0][0]);

	glBindVertexArray(VAO);

	if (vertices.size() > currentBufferSize) {
		currentBufferSize = vertices.size();

		glBindBuffer(GL_ARRAY_BUFFER, VBO);
		glBufferData(GL_ARRAY_BUFFER, vertices.size() * 3 * sizeof(GLfloat), vertices.data(), GL_STATIC_DRAW);
		glBindBuffer(GL_ARRAY_BUFFER, VBO_color);
		glBufferData(GL_ARRAY_BUFFER, colors.size() * 3 * sizeof(GLfloat), colors.data(), GL_STATIC_DRAW);
		glBindBuffer(GL_ARRAY_BUFFER, 0);
	}

	glDrawArrays(GL_LINES, 0, vertices.size());

	glBindVertexArray(0);
}

void BulletDebugDrawer::insert(float x, float y, float z, float cx, float cy, float cz)
{
	if (i >= vertices.size()) {
		vertices.emplace_back(x, y, z);
		colors.emplace_back(cx, cy, cz);
	} else {
		vertices[i].x = x;
		vertices[i].y = y;
		vertices[i].z = z;
		colors[i].x = cx;
		colors[i].y = cy;
		colors[i].z = cz;
	}
	i++;
}

void BulletDebugDrawer::drawLine(const btVector3& from, const btVector3& to, const btVector3& fromColor, const btVector3& toColor)
{
	insert(from.getX(), from.getY(), from.getZ(), fromColor.getX(), fromColor.getY(), fromColor.getZ());
	insert(to.getX(), to.getY(), to.getZ(), toColor.getX(), toColor.getY(), toColor.getZ());
}

void BulletDebugDrawer::drawLine(const btVector3& from, const btVector3& to, const btVector3& color)
{
	drawLine(from, to, color, color);
}

void BulletDebugDrawer::drawSphere(const btVector3& p, btScalar radius, const btVector3& color)
{

}

void BulletDebugDrawer::drawTriangle(const btVector3& a, const btVector3& b, const btVector3& c, const btVector3& color, btScalar alpha)
{

}

void BulletDebugDrawer::drawContactPoint(const btVector3& PointOnB, const btVector3& normalOnB, btScalar distance, int lifeTime, const btVector3& color)
{

}

void BulletDebugDrawer::reportErrorWarning(const char* warningString)
{

}

void BulletDebugDrawer::draw3dText(const btVector3& location, const char* textString)
{

}

void BulletDebugDrawer::setDebugMode(int debugMode)
{
	this->debugMode = (DebugDrawModes)debugMode;
}

int BulletDebugDrawer::getDebugMode() const
{
	return this->debugMode;
}
