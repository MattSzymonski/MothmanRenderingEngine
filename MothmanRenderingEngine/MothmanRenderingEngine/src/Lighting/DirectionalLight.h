#pragma once
#include "Light.h"
#include "../Shader.h"

class DirectionalLight :
	public Light
{
public:
	DirectionalLight();
	DirectionalLight(GLuint shadowWidth, GLuint shadowHeight, 
		GLfloat red, GLfloat green, GLfloat blue,
		GLfloat aIntensity, GLfloat dIntensity,
		GLfloat xDir, GLfloat yDir, GLfloat zDir);

	glm::vec3 GetDirection() { return direction; }
	glm::mat4 CalculateLightTransform();
	~DirectionalLight();

private:
	glm::vec3 direction;
};

