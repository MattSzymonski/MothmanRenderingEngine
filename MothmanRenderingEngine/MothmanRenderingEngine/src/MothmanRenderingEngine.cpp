//http://www.opengl-tutorial.org/intermediate-tutorials/tutorial-14-render-to-texture/
//http://in2gpu.com/2014/10/02/multiple-framebuffers-mrt/
//http://www.opengl-tutorial.org/beginners-tutorials/tutorial-8-basic-shading/

//-------------------------------------------LICENSE------------------------------------------------
#pragma region LICENSE

/*
 * Mothman Rendering Engine Project
 * Copyright (c) 2019 Mateusz Szymonski
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, version 3.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#pragma endregion

 //-------------------------INFO(Important things, Comments, TODO, Changes)--------------------------
#pragma region INFO

//Changes:
//Postprocessing (what effects?), registering uniforms, ImGui, resizable window
//Postprocessing shaders are not being loaded if they are not used

//TODO
//camera nice moving, all postprocessing effect in control panel, lod in terrain settings in control panel, MothmanRenderingEngine.hpp + new, skybox delete, move scene to different file


//OpenGL project that draws a tetrahedron on black background and rotates it using matrix muliplied by position vector of tetrahedron in geometry shader, this matrix is being updated in main loop
//Additionally tetrahedron is colorful due to interpolation based on vertex average of vertex positions
//Camera movement and rotation is implemented

//Texture units:
//1 - diffuse texture
//3 - directional shadow map
//2 - normal texture
//2-8 - shadow maps 

//Important info:
//By default GL_DEPTH_TEST should be disabled if not used

/*
Clear shader.cpp, now it uses same uniforms for all shaders
In this file when adding new shader we should register all uniforms in that shader. It will be stored in unordered_map<string, GUINT> in shader class
Same with samplers.

Then using these uniforms will be done in this file also, in proper renderpass functions. shader.SetUniform("name", value); - this function will traverse map to find given uniform and will set it

For larger things like light struct, invent some solution!
*/
#pragma endregion

//-------------------------INCLUDES(Definitions, Dependencies, Namespaces)--------------------------
#pragma region INCLUDES

#define STB_IMAGE_IMPLEMENTATION
#include <random>
//Standard libraries
#include <stdio.h>
#include <iostream>
#include <string.h>
#include <stdlib.h>
#include <vector>

//OpenGL must-have libraries
#include <GL\glew.h>
#include <GLFW\glfw3.h>

//OpenGL math libraries
#include <glm\mat4x4.hpp>
#include <glm\glm.hpp>
#include <glm\gtc\matrix_transform.hpp>
#include <glm\gtc\type_ptr.hpp>

//IMGUI libraries
#include "ImGui\imgui.h"
#include "ImGui\imgui_impl_glfw.h"
#include "ImGui\imgui_impl_opengl3.h"

//Project headers
#include "CommonValues.h"
#include "Lighting/PointLight.h"
#include "Lighting/DirectionalLight.h"
#include "Lighting/SpotLight.h"

#include "Mesh.h"
#include "Shader.h"
#include "Window.h"
#include "Camera.h"
#include "Texture.h"
#include "Material.h"
#include "Model.h"
#include "Terrain/TerrainController.h"
#include "Skybox.h"
#include "VertexOperations.h"
#include "PostProcessingEffectsSettings.h"

//Namespaces
using namespace std;

#pragma endregion

//------------------------------------------VARIABLES-----------------------------------------------
#pragma region VARIABLES

const float toRadians = 3.14159265f / 180.0f;

unsigned int mainFramebuffer;
unsigned int mainTextureColorBuffer;
unsigned int mainTextureDepthBuffer;
unsigned int screenQuadVAO, screenQuadVBO;

unsigned int postProcessFramebuffer0;
unsigned int postProcessColorBuffer0;
unsigned int postProcessFramebuffer1;
unsigned int postProcessColorBuffer1;

unsigned int depthFramebuffer;
unsigned int depthTextureDepthBuffer;
unsigned int depthTextureColorBuffer;

Window mainWindow;
Camera camera; //Camera class

Shader mainShader;
Shader skyboxShader;
Shader screenShader;
Shader directionalShadowShader;
Shader omniShadowShader;
Shader ssaoPostProcessingShader;
Shader invertPostProcessingShader;
Shader grayscalePostProcessingShader;
Shader edgeDetectionPostProcessingShader;
Shader colorCorrectionPostProcessingShader;
Shader vignettePostProcessingShader;
Shader logoOverlayPostProcessingShader;
Shader terrainShader;

Shader depthShader;
Shader depthVisualizePostProcessingShader;

std::vector<Mesh*> meshList; //List of Mesh pointers
Model floorModel;
Model testModel;
Model sculptureModel;
Model debugAxisArrows;
Model quiltedCube;
Model utahTeapot;

Skybox skybox;

Texture brickTexture;
Texture dirtTexture;
Texture logoOverlayTexture;

Material shinyMaterial;
Material dullMaterial;

DirectionalLight directionalLight;
PointLight pointLights[MAX_POINT_LIGHTS];
SpotLight spotLights[MAX_SPOT_LIGHTS];
unsigned int pointLightCount = 0;
unsigned int spotLightCount = 0;

//---------------Terrain---------------
TerrainController terrainController;
bool showTerrain = false;

//-----------------Misc----------------
GLfloat deltaTime = 0.0f; //To maintain same speed on every CPU
GLfloat lastTime = 0.0f; //To maintain same speed on every CPU

bool wireFrameState = false;
bool showSkybox = true;
unsigned int postProcessNumber = 0;
PostProcessingEffectsSettings postProcessingEffectsSettings;

bool showControlPanel = true;
bool showDemoWindow = false;
bool showTeapots = false;

bool rendererFirstFrame = true;
double rendererStartTime = 0;

//Orbiting light
float orbitingLightAngle;
float orbitingLightSpeed = 1.4f;
float orbitingLightRadius = 12.0f;
glm::vec2 orbitingLightPosition;

// Rotating sculpture
float sculptureRotationAngle = 45.0f;
float sculptureRotationSpeed = 1.0f;
//--------------LOG COUNTER------------
int logFramesPassed = 0;
double logStartTime = 0;
bool logFirstFrame = true;

#pragma endregion

//--------------------------------------SUPPORT FUNCTIONS-------------------------------------------
#pragma region SUPPORT FUNCTIONS

void Log()
{
	double timePassed = glfwGetTime();

	if (timePassed - logStartTime > 0.75)
	{
		logStartTime = timePassed;
		//std::cout << "LOG: " << postProcessingEffectsSettings.edgeDetection.GetOffsetValue() << std::endl;
	}
}

#pragma endregion

//-------------------------------------MANAGE RENDERER BUFFERS--------------------------------------
#pragma region MANAGE RENDERER BUFFERS

void CreateRenderBuffers()
{
	GLint framebufferWidth = mainWindow.GetFramebufferWidth();
	GLint framebufferHeight = mainWindow.GetFramebufferHeight();

	//------------Main buffer--------------
	glGenFramebuffers(1, &mainFramebuffer);
	glBindFramebuffer(GL_FRAMEBUFFER, mainFramebuffer);

	// create a color attachment texture
	glGenTextures(1, &mainTextureColorBuffer);
	glBindTexture(GL_TEXTURE_2D, mainTextureColorBuffer);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, framebufferWidth, framebufferHeight, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, mainTextureColorBuffer, 0);
	glBindTexture(GL_TEXTURE_2D, 0);

	// create a depth attachment texture
	glGenTextures(1, &mainTextureDepthBuffer);
	glBindTexture(GL_TEXTURE_2D, mainTextureDepthBuffer);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, framebufferWidth, framebufferHeight, 0, GL_DEPTH_COMPONENT, GL_UNSIGNED_BYTE, NULL);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, mainTextureDepthBuffer, 0);
	glBindTexture(GL_TEXTURE_2D, 0);

	if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
	{
		printf("Framebuffer error");
		return;
	}
	glBindFramebuffer(GL_FRAMEBUFFER, 0);






	glGenFramebuffers(1, &depthFramebuffer);
	glBindFramebuffer(GL_FRAMEBUFFER, depthFramebuffer);

	// create a color attachment texture
	glGenTextures(1, &depthTextureColorBuffer);
	glBindTexture(GL_TEXTURE_2D, depthTextureColorBuffer);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F, framebufferWidth, framebufferHeight, 0, GL_RGB, GL_FLOAT, NULL);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, depthTextureColorBuffer, 0);
	glBindTexture(GL_TEXTURE_2D, 0);

	// create a depth attachment texture
	glGenTextures(1, &depthTextureDepthBuffer);
	glBindTexture(GL_TEXTURE_2D, depthTextureDepthBuffer);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, framebufferWidth, framebufferHeight, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
	GLfloat borderColor[] = { 1.0f, 1.0f, 1.0f, 1.0f };
	glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderColor);


	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depthTextureDepthBuffer, 0);
	
	glBindTexture(GL_TEXTURE_2D, 0);

	if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
	{
		printf("Framebuffer error");
		return;
	}
	glBindFramebuffer(GL_FRAMEBUFFER, 0);






	//------------PostProcess Buffers--------------

	//--1--
	glGenFramebuffers(1, &postProcessFramebuffer0);
	glBindFramebuffer(GL_FRAMEBUFFER, postProcessFramebuffer0);

	// create a color attachment texture
	glGenTextures(1, &postProcessColorBuffer0);
	glBindTexture(GL_TEXTURE_2D, postProcessColorBuffer0);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, framebufferWidth, framebufferHeight, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, postProcessColorBuffer0, 0);
	glBindTexture(GL_TEXTURE_2D, 0);

	if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
	{
		printf("Framebuffer error");
		return;
	}
	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	//--2--
	glGenFramebuffers(1, &postProcessFramebuffer1);
	glBindFramebuffer(GL_FRAMEBUFFER, postProcessFramebuffer1);

	// create a color attachment texture
	glGenTextures(1, &postProcessColorBuffer1);
	glBindTexture(GL_TEXTURE_2D, postProcessColorBuffer1);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, framebufferWidth, framebufferHeight, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, postProcessColorBuffer1, 0);
	glBindTexture(GL_TEXTURE_2D, 0);

	if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
	{
		printf("Framebuffer error");
		return;
	}
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void DeleteRenderBuffers()
{
	if (mainFramebuffer > 0)
	{
		glDeleteFramebuffers(1, &mainFramebuffer);
	}

	if (mainTextureColorBuffer > 0)
	{
		glDeleteTextures(1, &mainTextureColorBuffer);
	}

	if (mainTextureDepthBuffer > 0)
	{
		glDeleteTextures(1, &mainTextureColorBuffer);
	}

	if (postProcessFramebuffer0 > 0)
	{
		glDeleteFramebuffers(1, &postProcessFramebuffer0);
	}

	if (postProcessColorBuffer0 > 0)
	{
		glDeleteTextures(1, &postProcessColorBuffer0);
	}

	if (postProcessFramebuffer1 > 0)
	{
		glDeleteFramebuffers(1, &postProcessFramebuffer1);
	}

	if (postProcessColorBuffer1 > 0)
	{
		glDeleteTextures(1, &postProcessColorBuffer1);
	}
}

#pragma endregion

//-------------------------------------INITIALIZE RENDERER------------------------------------------
#pragma region INIT

void InitRenderer()
{
	CreateRenderBuffers();

	//--------Screen quad mesh VAO---------
	float quadVertices[] = { // vertex attributes for a quad that fills the entire screen in Normalized Device Coordinates.
		// positions   // texCoords
		-1.0f,  1.0f,  0.0f, 1.0f,
		-1.0f, -1.0f,  0.0f, 0.0f,
		 1.0f, -1.0f,  1.0f, 0.0f,

		-1.0f,  1.0f,  0.0f, 1.0f,
		 1.0f, -1.0f,  1.0f, 0.0f,
		 1.0f,  1.0f,  1.0f, 1.0f
	};

	glGenVertexArrays(1, &screenQuadVAO);
	glGenBuffers(1, &screenQuadVBO);
	glBindVertexArray(screenQuadVAO);
	glBindBuffer(GL_ARRAY_BUFFER, screenQuadVBO);
	glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), &quadVertices, GL_STATIC_DRAW);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
	glEnableVertexAttribArray(1);
	glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
}

#pragma endregion

//------------------------------SCENE(Models, Lights, Skybox, etc)----------------------------------
#pragma region SCENE

void CreateObjects()
{

}

void LoadModels()
{

	floorModel = Model();
	floorModel.LoadModel("res/Models/DebugPlane.fbx");

	testModel = Model();
	testModel.LoadModel("res/Models/Tester.fbx");

	sculptureModel = Model();
	sculptureModel.LoadModel("res/Models/MCh_S_12_C.fbx");

	debugAxisArrows = Model();
	debugAxisArrows.LoadModel("res/Models/DebugAxisArrows.fbx");

	quiltedCube = Model();
	quiltedCube.LoadModel("res/Models/QuiltedCube.fbx");

	utahTeapot = Model();
	utahTeapot.LoadModel("res/Models/UtahTeapot.fbx");
}

void CreateSkybox()
{
	//Creating skybox
	std::vector<std::string> skyboxFaces;
	/*
	skyboxFaces.push_back("Textures/Skybox/CupertinLake/cupertin-lake_rt.tga");
	skyboxFaces.push_back("Textures/Skybox/CupertinLake/cupertin-lake_lf.tga");
	skyboxFaces.push_back("Textures/Skybox/CupertinLake/cupertin-lake_up.tga");
	skyboxFaces.push_back("Textures/Skybox/CupertinLake/cupertin-lake_dn.tga");
	skyboxFaces.push_back("Textures/Skybox/CupertinLake/cupertin-lake_bk.tga");
	skyboxFaces.push_back("Textures/Skybox/CupertinLake/cupertin-lake_ft.tga");
	*/

	skyboxFaces.push_back("res/Textures/Skybox/DesertSky/desertsky_rt.tga");
	skyboxFaces.push_back("res/Textures/Skybox/DesertSky/desertsky_lf.tga");
	skyboxFaces.push_back("res/Textures/Skybox/DesertSky/desertsky_up.tga");
	skyboxFaces.push_back("res/Textures/Skybox/DesertSky/desertsky_dn.tga");
	skyboxFaces.push_back("res/Textures/Skybox/DesertSky/desertsky_bk.tga");
	skyboxFaces.push_back("res/Textures/Skybox/DesertSky/desertsky_ft.tga");
	skybox = Skybox(&skyboxShader, skyboxFaces);
}

void CreateLights()
{
	//Create directional light
	directionalLight = DirectionalLight(2048, 2048,
		1.0f, 1.0f, 1.0f,
		0.2f, 0.7f,
		-45.0f, -35.0f, -45.0f);


	//Create pointlights
	pointLights[0] = PointLight(1024, 1024,
		0.1f, 100.0f,
		1.0f, 0.16f, 0.79f,
		0.0f, 0.8f,
		5.0f, 5.0f, 8.0f,
		0.3f, 0.1f, 0.0f);
	pointLightCount++;

	pointLights[1] = PointLight(1024, 1024,
		0.1f, 100.0f,
		0.43f, 0.92f, 0.89f,
		0.1f, 1.9f,
		-2.0f, 5.0f, -5.0f,
		0.3f, 0.1f, 0.1f);
	pointLightCount++;


	//Create spotlights
	spotLights[0] = SpotLight(1024, 1024,
		0.1f, 100.0f,
		1.0f, 1.0f, 1.0f,
		0.0f, 2.8f,
		15.0f, 5.0f, 15.0f,
		-45.0f, -20.0f, -45.0f,
		3.40f, 0.0f, 0.0f,
		10.0f);
	spotLightCount++;

	spotLights[1] = SpotLight(1024, 1024,
		0.1f, 100.0f,
		1.0f, 1.0f, 1.0f,
		0.0f, 2.8f,
		15.0f, 5.0f, -15.0f,
		-45.0f, -20.0f, 45.0f,
		3.40f, 0.0f, 0.0f,
		10.0f);
	spotLightCount++;
}

#pragma endregion

//-----------------------------------------PLAYER INPUT---------------------------------------------
#pragma region PLAYER INPUT

void MovingMeshes()
{
	orbitingLightAngle += orbitingLightSpeed * deltaTime;
	orbitingLightPosition = glm::vec2(0 + orbitingLightRadius * cos(orbitingLightAngle), orbitingLightRadius * sin(orbitingLightAngle));

	pointLights[0].SetPosition(glm::vec3(orbitingLightPosition.x, 4, orbitingLightPosition.y));

	sculptureRotationAngle += sculptureRotationSpeed * deltaTime;

}

void ToggleControlPanel()
{
	if (mainWindow.GetKeys()[GLFW_KEY_C] == true)
	{
		showControlPanel = !showControlPanel;
		mainWindow.GetKeys()[GLFW_KEY_C] = false;
	}
}

#pragma endregion

//---------------------------------------------GUI--------------------------------------------------
#pragma region GUI

void ImGuiInit()
{
	//ImGui initialization
	ImGui::CreateContext();
	ImGui::StyleColorsDark();
	ImGui_ImplGlfw_InitForOpenGL(mainWindow.GetWindow(), true);
	ImGui_ImplOpenGL3_Init("#version 130");
}

void ImGuiNewFrame()
{
	ImGui_ImplOpenGL3_NewFrame();
	ImGui_ImplGlfw_NewFrame();
	ImGui::NewFrame();
}

void ImGuiMainControlPanel()
{
	ImGui::Begin("Mothman Rendering Engine Control Panel");

	if (ImGui::CollapsingHeader("Terrain settings"))
	{
		ImGui::Checkbox("Enable terrain", &showTerrain);
		if(showTerrain)
		{
			if (terrainController.GetTerrain("Terrain1") != nullptr)
			{
				int terrainTessellationFactor = terrainController.GetTerrain("Terrain1")->GetTerrainSettings()->GetTessellationFactor();
				ImGui::SliderInt("Tessellation factor", &terrainTessellationFactor, 0, 10);
				terrainController.GetTerrain("Terrain1")->GetTerrainSettings()->SetTessellationFactor((GLuint)terrainTessellationFactor);

				float terrainTessellationSlope = terrainController.GetTerrain("Terrain1")->GetTerrainSettings()->GetTessellationSlope();
				ImGui::SliderFloat("Tessellation slope", &terrainTessellationSlope, -5.0f, 5.0f);
				terrainController.GetTerrain("Terrain1")->GetTerrainSettings()->SetTessellationSlope((GLfloat)terrainTessellationSlope);

				float terrainTessellationShift = terrainController.GetTerrain("Terrain1")->GetTerrainSettings()->GetTessellationShift();
				ImGui::SliderFloat("Tessellation shift", &terrainTessellationShift, 0.0f, 5.0f);
				terrainController.GetTerrain("Terrain1")->GetTerrainSettings()->SetTessellationShift((GLfloat)terrainTessellationShift);
			}
			else
			{
				ImGui::Text("No terrain created");
			}
		}	
	}
	if (ImGui::CollapsingHeader("Post-processing settings"))
	{
		bool postProcessingEffectsIsEnabled = postProcessingEffectsSettings.IsEnabled();
		ImGui::Checkbox("Enable post-processing", &postProcessingEffectsIsEnabled);
		if (postProcessingEffectsIsEnabled) { postProcessingEffectsSettings.Enable(); }
		else { postProcessingEffectsSettings.Disable(); }

		if (postProcessingEffectsIsEnabled)
		{
			ImGuiIO& io = ImGui::GetIO();

			if (ImGui::TreeNode("Invert"))
			{
				bool postProcessingEffectsIsEnabled_invert_enable = postProcessingEffectsSettings.invert.IsEnabled();
				ImGui::Checkbox("Enable", &postProcessingEffectsIsEnabled_invert_enable);
				if (postProcessingEffectsIsEnabled_invert_enable) { postProcessingEffectsSettings.invert.Enable(); }
				else { postProcessingEffectsSettings.invert.Disable(); }

				ImGui::TreePop();
				ImGui::Separator();
			}

			if (ImGui::TreeNode("Vignette"))
			{
				bool postProcessingEffectsIsEnabled_vingette_enable = postProcessingEffectsSettings.vignette.IsEnabled();
				ImGui::Checkbox("Enable", &postProcessingEffectsIsEnabled_vingette_enable);
				if (postProcessingEffectsIsEnabled_vingette_enable) { postProcessingEffectsSettings.vignette.Enable(); }
				else { postProcessingEffectsSettings.vignette.Disable(); }

				auto postProcessingEffectsIsEnabled_vingette_radius = postProcessingEffectsSettings.vignette.GetRadius();
				ImGui::SliderFloat("Radius", &postProcessingEffectsIsEnabled_vingette_radius, 0.0f, 3.0f);
				postProcessingEffectsSettings.vignette.SetRadius(postProcessingEffectsIsEnabled_vingette_radius);

				auto postProcessingEffectsIsEnabled_vingette_softness = postProcessingEffectsSettings.vignette.GetSoftness();
				ImGui::SliderFloat("Softness", &postProcessingEffectsIsEnabled_vingette_softness, 0.0f, 3.0f);
				postProcessingEffectsSettings.vignette.SetSoftness(postProcessingEffectsIsEnabled_vingette_softness);

				auto postProcessingEffectsIsEnabled_vingette_intensity = postProcessingEffectsSettings.vignette.GetIntensity();
				ImGui::SliderFloat("Intensity", &postProcessingEffectsIsEnabled_vingette_intensity, 0.0f, 1.0f);
				postProcessingEffectsSettings.vignette.SetIntensity(postProcessingEffectsIsEnabled_vingette_intensity);

				ImGui::TreePop();
				ImGui::Separator();
			}
			if (ImGui::TreeNode("Depth visualize"))
			{
				bool postProcessingEffectsIsEnabled_depthVisualize_enable = postProcessingEffectsSettings.depthVisualize.IsEnabled();
				ImGui::Checkbox("Enable", &postProcessingEffectsIsEnabled_depthVisualize_enable);
				if (postProcessingEffectsIsEnabled_depthVisualize_enable) { postProcessingEffectsSettings.depthVisualize.Enable(); }
				else { postProcessingEffectsSettings.depthVisualize.Disable(); }

				auto postProcessingEffectsIsEnabled_depthVisualize_near = postProcessingEffectsSettings.depthVisualize.GetNear();
				ImGui::SliderFloat("Near", &postProcessingEffectsIsEnabled_depthVisualize_near, 0.0f, 250.0f);
				postProcessingEffectsSettings.depthVisualize.SetNear(postProcessingEffectsIsEnabled_depthVisualize_near);

				auto postProcessingEffectsIsEnabled_depthVisualize_far = postProcessingEffectsSettings.depthVisualize.GetFar();
				ImGui::SliderFloat("Far", &postProcessingEffectsIsEnabled_depthVisualize_far, 0.0f, 1000.0f);
				postProcessingEffectsSettings.depthVisualize.SetFar(postProcessingEffectsIsEnabled_depthVisualize_far);

				ImGui::TreePop();
				ImGui::Separator();
			}
			ImGui::Separator();
		}
	}
	if (ImGui::CollapsingHeader("Demo orbiting light"))
	{
		ImGui::SliderFloat("Radius", &orbitingLightRadius, 0.0f, 100.0f);
		ImGui::SliderFloat("Speed", &orbitingLightSpeed, -20.0f, 20.0f);

		auto orbitingLightDiffuseIntensity = pointLights[0].GetDiffuseIntensity();
		ImGui::SliderFloat("Diffuse Intensity", &orbitingLightDiffuseIntensity, 0.0f, 10.0f);
		pointLights[0].SetDiffuseIntensity((GLfloat)orbitingLightDiffuseIntensity);

		auto orbitingLightAmbientIntensity = pointLights[0].GetAmbientIntensity();
		ImGui::SliderFloat("Ambient Intensity", &orbitingLightAmbientIntensity, 0.0f, 10.0f);
		pointLights[0].SetAmbientIntensity((GLfloat)orbitingLightAmbientIntensity);

		auto orbitingLightColor = pointLights[0].GetColor();
		ImGui::ColorEdit3("Color", (float*)&orbitingLightColor);
		pointLights[0].SetColor(orbitingLightColor);
	}

	if (ImGui::CollapsingHeader("Other Settings"))
	{
		ImGui::Checkbox("Wireframe", &wireFrameState);
		ImGui::Checkbox("Skybox", &showSkybox);
		ImGui::Checkbox("Teapots", &showTeapots);
		ImGui::Checkbox("ImGui Demo Window", &showDemoWindow);
		
	}
	ImGui::Separator();
	ImGui::Text("Window size: %d\px x %d\px", mainWindow.GetWindowWidth(), mainWindow.GetWindowHeight());
	ImGui::Text("Framebuffer size: %d\px x %d\px", mainWindow.GetFramebufferWidth(), mainWindow.GetFramebufferHeight());
	ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);


	ImGui::End();
}

void ImGuiDemoWindow()
{
	ImGui::ShowDemoWindow(&showDemoWindow);
}

void ImGuiCleanUp()
{
	ImGui_ImplOpenGL3_Shutdown();
	ImGui_ImplGlfw_Shutdown();
	ImGui::DestroyContext();
}

void ImGuiRender()
{
	ImGui::Render();
	ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

#pragma endregion

//---------------------------RENDERING(Create shaders, Objects to render)---------------------------
#pragma region RENDERING GENERAL

void CreateShaders()
{
	mainShader = Shader("Main Shader");
	mainShader.CreateFromFiles("res/Shaders/vertex.shader", "res/Shaders/fragment.shader");
	mainShader.RegisterUniform("mat4", "u_projection");
	mainShader.RegisterUniform("mat4", "u_model");
	mainShader.RegisterUniform("mat4", "u_view");
	mainShader.RegisterUniform("vec3", "u_cameraPosition");
	mainShader.RegisterUniform("mat4", "u_directionalLightTransform");

	mainShader.RegisterUniform("vec3", "u_directionalLight.base.color");
	mainShader.RegisterUniform("float", "u_directionalLight.base.ambientIntensity");
	mainShader.RegisterUniform("vec3", "u_directionalLight.direction");
	mainShader.RegisterUniform("float", "u_directionalLight.base.diffuseIntensity");

	mainShader.RegisterUniform("int", "u_pointLightCount");
	for (int i = 0; i < MAX_POINT_LIGHTS; i++) //For each pointLight on the scene connect light get location of variables in shader
	{
		string baseName = string("u_pointLights[") + to_string(i) + string("].");
		mainShader.RegisterUniform("vec3", baseName + "base.color");
		mainShader.RegisterUniform("float", baseName + "base.ambientIntensity");
		mainShader.RegisterUniform("float", baseName + "base.diffuseIntensity");
		mainShader.RegisterUniform("vec3", baseName + "position");
		mainShader.RegisterUniform("float", baseName + "constant");
		mainShader.RegisterUniform("float", baseName + "linear");
		mainShader.RegisterUniform("float", baseName + "exponent");
	}

	mainShader.RegisterUniform("int", "u_spotLightCount");
	for (int i = 0; i < MAX_SPOT_LIGHTS; i++) //For each spotLight on the scene connect light get location of variables in shader
	{
		string baseName = string("u_spotLights[") + to_string(i) + string("].");
		mainShader.RegisterUniform("vec3", baseName + "base.base.color");
		mainShader.RegisterUniform("float", baseName + "base.base.ambientIntensity");
		mainShader.RegisterUniform("float", baseName + "base.base.diffuseIntensity");
		mainShader.RegisterUniform("vec3", baseName + "base.position");
		mainShader.RegisterUniform("float", baseName + "base.constant");
		mainShader.RegisterUniform("float", baseName + "base.linear");
		mainShader.RegisterUniform("float", baseName + "base.exponent");
		mainShader.RegisterUniform("vec3", baseName + "direction");
		mainShader.RegisterUniform("float", baseName + "edge");
	}

	for (int i = 0; i < MAX_POINT_LIGHTS + MAX_SPOT_LIGHTS; i++) //omni-light shadow variables connect to the shader variables
	{
		string baseName = string("u_omniShadowMaps[") + to_string(i) + string("].");
		mainShader.RegisterSampler("samplerCube", baseName + "shadowMap");
		mainShader.RegisterUniform("float", baseName + "farPlane");
	}

	mainShader.RegisterSampler("sampler2D", "s_directionalShadowMap");
	mainShader.RegisterSampler("sampler2D", "s_textureDiffuse");
	mainShader.RegisterSampler("sampler2D", "s_textureNormal");

	mainShader.RegisterUniform("float", "u_material.specularIntensity");
	mainShader.RegisterUniform("float", "u_material.shininess");

	//----------------
	skyboxShader = Shader("Skybox Shader");
	skyboxShader.CreateFromFiles("res/Shaders/skybox_vertex.shader", "res/Shaders/skybox_fragment.shader");
	skyboxShader.RegisterUniform("mat4", "u_projection");
	skyboxShader.RegisterUniform("mat4", "u_view");

	//----------------
	screenShader = Shader("Screen Shader");
	screenShader.CreateFromFiles("res/Shaders/screen_vertex.shader", "res/Shaders/screen_fragment.shader");

	//----------------
	directionalShadowShader = Shader("Directional Shadow Shader");
	directionalShadowShader.CreateFromFiles("res/Shaders/directional_shadow_map_vertex.shader", "res/Shaders/directional_shadow_fragment.shader");
	directionalShadowShader.RegisterUniform("mat4", "u_model");
	directionalShadowShader.RegisterUniform("mat4", "u_directionalLightTransform");

	//----------------
	omniShadowShader = Shader("Omni Shadow Shader");
	omniShadowShader.CreateFromFiles("res/Shaders/omni_directional_shadow_map_vertex.shader", "res/Shaders/omni_directional_shadow_map_geometry.shader", "res/Shaders/omni_directional_shadow_map_fragment.shader");
	omniShadowShader.RegisterUniform("mat4", "u_model");
	omniShadowShader.RegisterUniform("vec3", "u_lightPos");
	omniShadowShader.RegisterUniform("float", "u_farPlane");

	for (int i = 0; i < 6; i++)
	{
		string baseName = string("u_lightMatrices[") + to_string(i) + string("]");
		omniShadowShader.RegisterUniform("mat4", baseName);
	}

	//----------------
	/*
	ssaoPostProcessingShader = Shader("SSAO PostProcess Shader");
	ssaoPostProcessingShader.CreateFromFiles("res/Shaders/screen_vertex.shader", "res/Shaders/PostProcessing/ssao_postprocess_fragment.shader");
	ssaoPostProcessingShader.RegisterSampler("sampler2D", "s_screenTexture");
	ssaoPostProcessingShader.RegisterSampler("sampler2D", "s_position");
	ssaoPostProcessingShader.RegisterSampler("sampler2D", "s_normal");
	ssaoPostProcessingShader.RegisterSampler("sampler2D", "s_texNoise");
	for (int i = 0; i < SSAO_MAX_SAMPLES_PER_KERNEL; i++)
	{
		string baseName = string("u_samples[") + to_string(i) + string("]");
		omniShadowShader.RegisterUniform("vec3", baseName);
	}
	ssaoPostProcessingShader.RegisterUniform("mat4", "u_projection");
	ssaoPostProcessingShader.RegisterUniform("vec2", "u_framebufferDimensions");
	ssaoPostProcessingShader.RegisterUniform("int", "u_kernelSize");
	ssaoPostProcessingShader.RegisterUniform("float", "u_kernelRadius");
	ssaoPostProcessingShader.RegisterUniform("float", "u_bias");
	*/

	//----------------
	invertPostProcessingShader = Shader("Invert PostProcess Shader");
	invertPostProcessingShader.CreateFromFiles("res/Shaders/screen_vertex.shader", "res/Shaders/PostProcessing/invert_postprocess_fragment.shader");
	invertPostProcessingShader.RegisterSampler("sampler2D", "s_screenTexture");

	//----------------
	grayscalePostProcessingShader = Shader("Grayscale PostProcess Shader");
	grayscalePostProcessingShader.CreateFromFiles("res/Shaders/screen_vertex.shader", "res/Shaders/PostProcessing/grayscale_postprocess_fragment.shader");
	grayscalePostProcessingShader.RegisterSampler("sampler2D", "s_screenTexture");

	//----------------
	edgeDetectionPostProcessingShader = Shader("Edge Detection PostProcess Shader");
	edgeDetectionPostProcessingShader.CreateFromFiles("res/Shaders/screen_vertex.shader", "res/Shaders/PostProcessing/edgedetection_postprocess_fragment.shader");
	edgeDetectionPostProcessingShader.RegisterSampler("sampler2D", "s_screenTexture");
	edgeDetectionPostProcessingShader.RegisterUniform("float", "u_offset");

	//----------------
	//colorCorrectionPostProcessingShader = Shader("Color Correction Shader");
	//colorCorrectionPostProcessingShader.CreateFromFiles("res/Shaders/screen_vertex.shader", "res/Shaders/PostProcessing/colorcorrection_postprocess_fragment.shader");
	//colorCorrectionPostProcessingShader.RegisterSampler("sampler2D", "s_screenTexture");

	//----------------
	vignettePostProcessingShader = Shader("Vignette Shader");
	vignettePostProcessingShader.CreateFromFiles("res/Shaders/screen_vertex.shader", "res/Shaders/PostProcessing/vignette_postprocess_fragment.shader");
	vignettePostProcessingShader.RegisterSampler("sampler2D", "s_screenTexture");
	vignettePostProcessingShader.RegisterUniform("vec2", "u_resolution");
	vignettePostProcessingShader.RegisterUniform("float", "u_radius");
	vignettePostProcessingShader.RegisterUniform("float", "u_softness");
	vignettePostProcessingShader.RegisterUniform("float", "u_intensity");

	//----------------
	logoOverlayPostProcessingShader = Shader("Logo Overlay Shader");
	logoOverlayPostProcessingShader.CreateFromFiles("res/Shaders/screen_vertex.shader", "res/Shaders/PostProcessing/logooverlay_postprocess_fragment.shader");
	logoOverlayPostProcessingShader.RegisterSampler("sampler2D", "s_screenTexture");
	logoOverlayPostProcessingShader.RegisterSampler("sampler2D", "s_logoOverlayTexture");
	logoOverlayPostProcessingShader.RegisterUniform("vec4", "u_logoOverlayTint");
	logoOverlayPostProcessingShader.RegisterUniform("vec2", "u_framebufferDimensions");
	logoOverlayPostProcessingShader.RegisterUniform("vec2", "u_logoOverlayTextureDimensions");


	//----------------
	terrainShader = Shader("Terrain Shader");
	terrainShader.CreateFromFiles("res/Shaders/Terrain/terrain_vertex.shader", "res/Shaders/Terrain/terrain_tessellation_control.shader", "res/Shaders/Terrain/terrain_tessellation_evaluation.shader", "res/Shaders/Terrain/terrain_geometry.shader", "res/Shaders/Terrain/terrain_fragment.shader");
	terrainShader.RegisterUniform("vec3", "u_cameraPosition");
	terrainShader.RegisterUniform("mat4", "u_localMatrix");
	terrainShader.RegisterUniform("mat4", "u_worldMatrix");
	terrainShader.RegisterUniform("float", "u_scaleY");
	terrainShader.RegisterUniform("int", "u_lod");
	terrainShader.RegisterUniform("vec2", "u_index");
	terrainShader.RegisterUniform("float", "u_gap");
	terrainShader.RegisterUniform("vec2", "u_location");
	terrainShader.RegisterSampler("sampler2D", "s_heightmap");


	for (size_t i = 0; i < 8; i++)
	{
		string baseName = string("u_lodMorphArea[") + to_string(i) + string("]");
		terrainShader.RegisterUniform("int", baseName);
	}

	terrainShader.RegisterUniform("mat4", "u_viewProjection");

	terrainShader.RegisterUniform("int", "u_tessellationFactor");
	terrainShader.RegisterUniform("float", "u_tessellationSlope");
	terrainShader.RegisterUniform("float", "u_tessellationShift");
	terrainShader.RegisterSampler("sampler2D", "s_textureNormal");



	depthShader = Shader("Depth Shader");
	depthShader.CreateFromFiles("res/Shaders/depth_vertex.shader", "res/Shaders/depth_fragment.shader");
	depthShader.RegisterUniform("mat4", "u_projection");
	depthShader.RegisterUniform("mat4", "u_model");
	depthShader.RegisterUniform("mat4", "u_view");


	depthVisualizePostProcessingShader = Shader("Depth Visualize Shader");
	depthVisualizePostProcessingShader.CreateFromFiles("res/Shaders/screen_vertex.shader", "res/Shaders/PostProcessing/depthvisualize_postprocess_fragment.shader");
	//depthVisualizePostProcessingShader.RegisterSampler("sampler2D", "s_screenTexture");
	depthVisualizePostProcessingShader.RegisterSampler("sampler2D", "s_depthColorTexture");
	depthVisualizePostProcessingShader.RegisterUniform("float", "u_near");
	depthVisualizePostProcessingShader.RegisterUniform("float", "u_far");
}

void RenderScene(Shader *shader)
{
	glm::mat4 model;

	model = glm::mat4();
	model = glm::translate(model, glm::vec3(0.0f, 0.0f, 0.0f));
	shader->SetUniform("u_model", model);
	if (shader == &mainShader) { shinyMaterial.UseMaterial(&mainShader, "u_material.specularIntensity", "u_material.shininess"); }
	floorModel.RenderModel();


	model = glm::mat4();
	model = glm::translate(model, glm::vec3(0.0f, 0.0f, 0.0f));
	shader->SetUniform("u_model", model);
	if (shader == &mainShader) { shinyMaterial.UseMaterial(&mainShader, "u_material.specularIntensity", "u_material.shininess"); }
	//testModel.RenderModel();

	if (!showTeapots)
	{
		model = glm::mat4();
		model = glm::translate(model, glm::vec3(0.0f, 3.0f, 0.0f));
		model = glm::rotate(model, sculptureRotationAngle, glm::vec3(0.0f, 1.0f, 0.0f));
		shader->SetUniform("u_model", model);
		if (shader == &mainShader) { shinyMaterial.UseMaterial(&mainShader, "u_material.specularIntensity", "u_material.shininess"); }
		sculptureModel.RenderModel();
	}
	
	model = glm::mat4();
	model = glm::translate(model, glm::vec3(0.0f, 10.0f, 0.0f));
	model = glm::scale(model, glm::vec3(0.25f, 0.25f, 0.25f));
	shader->SetUniform("u_model", model);
	if (shader == &mainShader) { shinyMaterial.UseMaterial(&mainShader, "u_material.specularIntensity", "u_material.shininess"); }
	//debugAxisArrows.RenderModel();

	model = glm::mat4();
	model = glm::translate(model, glm::vec3(3.0f, 1.75f, 5.0f));
	shader->SetUniform("u_model", model);
	if (shader == &mainShader) { dullMaterial.UseMaterial(&mainShader, "u_material.specularIntensity", "u_material.shininess"); }
	//quiltedCube.RenderModel();

	if (showTeapots)
	{
		for (size_t i = 0; i < 8; i++)
		{
			for (size_t t = 0; t < 8; t++)
			{
				model = glm::mat4();
				model = glm::translate(model, glm::vec3((i * 1.5f) - 6.0f, 0.0f, (t * 1.5f) + -6.0f));
				model = glm::rotate(model, 45.0f * toRadians, glm::vec3(0.0f, 1.0f, 0.0f));

				//model = glm::scale(model, glm::vec3(2.0f, 2.0f, 2.0f));
				shader->SetUniform("u_model", model);
				if (shader == &mainShader) { shinyMaterial.UseMaterial(&mainShader, "u_material.specularIntensity", "u_material.shininess"); }
				utahTeapot.RenderModel();
			}
		}
	}



}
#pragma endregion

//---------------------------------------RENDERING(Passes)------------------------------------------
#pragma region RENDERING PASSES

void DepthPass()
{
	glEnable(GL_DEPTH_TEST);
	glDepthMask(GL_TRUE);
	glEnable(GL_CULL_FACE);
	glCullFace(GL_BACK);
	glViewport(0, 0, mainWindow.GetFramebufferWidth(), mainWindow.GetFramebufferHeight());

	glBindFramebuffer(GL_FRAMEBUFFER, depthFramebuffer);
	glClearColor(0.0f, 0.0f, 0.0f, 1.0f); //Clears whole frame and sets color
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT); //Clear colors and depth (Each pixel stores more information than only color (depth, stencil, alpha, etc))

	depthShader.UseShader(); //Set directional shadow shader active
	depthShader.SetUniform("u_projection", camera.GetProjectionMatrix());
	depthShader.SetUniform("u_view", camera.GetViewMatrix());

	depthShader.Validate();
	RenderScene(&depthShader);

	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glDisable(GL_CULL_FACE);
	glDepthMask(GL_FALSE);
	glDisable(GL_DEPTH_TEST);
}

void DirectionalShadowMapPass(DirectionalLight* light) //Pass that renders directional shadow map
{
	glEnable(GL_DEPTH_TEST);
	glDepthMask(GL_TRUE);
	glViewport(0, 0, light->GetShadowMap()->GetShadowWidth(), light->GetShadowMap()->GetShadowHeight()); //Set viewport size to the same dimensions as framebuffer
	directionalShadowShader.UseShader(); //Set directional shadow shader active
	directionalShadowShader.SetUniform("u_directionalLightTransform", light->CalculateLightTransform());

	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, light->GetShadowMap()->GetFBO());

	glClear(GL_DEPTH_BUFFER_BIT);
	directionalShadowShader.Validate();
	RenderScene(&directionalShadowShader);

	glBindFramebuffer(GL_FRAMEBUFFER, 0); //Bind default framebuffer that will draw scene
	glDepthMask(GL_FALSE);
	glDisable(GL_DEPTH_TEST);
}

void OmniShadowMapPass(PointLight* light) //Pass that renders omni-directional shadow map
{
	glEnable(GL_DEPTH_TEST);
	glDepthMask(GL_TRUE);
	glViewport(0, 0, light->GetShadowMap()->GetShadowWidth(), light->GetShadowMap()->GetShadowHeight()); //Set viewport size to the same dimensions as framebuffer
	omniShadowShader.UseShader(); //Set directional shadow shader active
	omniShadowShader.SetUniform("u_lightPos", light->GetPosition());
	omniShadowShader.SetUniform("u_farPlane", light->GetFarPlane());

	for (int i = 0; i < 6; i++)
	{
		string baseName = string("u_lightMatrices[") + to_string(i) + string("]");
		omniShadowShader.SetUniform(baseName, light->CalculateLightTransform()[i]);
	}

	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, light->GetShadowMap()->GetFBO());

	glClear(GL_DEPTH_BUFFER_BIT);
	omniShadowShader.Validate();
	RenderScene(&omniShadowShader);

	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glDepthMask(GL_FALSE);
	glDisable(GL_DEPTH_TEST);
}

void RenderPass(glm::mat4 projectionMatrix, glm::mat4 viewMatrix)
{
	if (wireFrameState) { glPolygonMode(GL_FRONT_AND_BACK, GL_LINE); } //Wireframe mode

	glEnable(GL_DEPTH_TEST);
	glDepthMask(GL_TRUE);
	glViewport(0, 0, mainWindow.GetFramebufferWidth(), mainWindow.GetFramebufferHeight());

	glBindFramebuffer(GL_FRAMEBUFFER, mainFramebuffer);
	glClearColor(0.0f, 0.0f, 0.0f, 1.0f); //Clears whole frame and sets color
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT); //Clear colors and depth (Each pixel stores more information than only color (depth, stencil, alpha, etc))

	if (showSkybox)
	{
		skybox.DrawSkybox(viewMatrix, projectionMatrix);
	}

	//Terrain
	if (showTerrain)
	{
		if (terrainController.GetTerrain("Terrain1") != nullptr)
		{
			terrainController.GetTerrain("Terrain1")->GetTerrainModule()->Render(true);
		}
	}

	mainShader.UseShader(); //Set default shader active
	mainShader.SetUniform("u_projection", projectionMatrix);
	mainShader.SetUniform("u_view", viewMatrix);
	mainShader.SetUniform("u_cameraPosition", camera.GetCameraPosition());

	//Set directional light
	mainShader.SetUniform("u_directionalLightTransform", directionalLight.CalculateLightTransform());
	mainShader.SetUniform("u_directionalLight.base.color", directionalLight.GetColor());
	mainShader.SetUniform("u_directionalLight.base.ambientIntensity", directionalLight.GetAmbientIntensity());
	mainShader.SetUniform("u_directionalLight.direction", directionalLight.GetDirection());
	mainShader.SetUniform("u_directionalLight.base.diffuseIntensity", directionalLight.GetDiffuseIntensity());
	mainShader.BindSampler("s_directionalShadowMap", DIR_SHADOWMAP_TEXUNIT, directionalLight.GetShadowMap()->GetShadowMapTexture()); //Set sampler in shader to look for texture at texture unit DIR_SHADOWMAP_TEXUNIT and bind texture with that unit

	//Set point lights
	int pointLightsToProcess = pointLightCount;
	if (pointLightCount > MAX_POINT_LIGHTS) { pointLightsToProcess = MAX_POINT_LIGHTS; }
	mainShader.SetUniform("u_pointLightCount", pointLightsToProcess);

	for (int i = 0; i < pointLightsToProcess; i++) //For each pointLight on the scene connect light get location of variables in shader
	{
		string baseName;
		baseName = string("u_pointLights[") + to_string(i) + string("].");
		mainShader.SetUniform(baseName + "base.color", pointLights[i].GetColor());
		mainShader.SetUniform(baseName + "base.ambientIntensity", pointLights[i].GetAmbientIntensity());
		mainShader.SetUniform(baseName + "base.diffuseIntensity", pointLights[i].GetDiffuseIntensity());
		mainShader.SetUniform(baseName + "position", pointLights[i].GetPosition());
		mainShader.SetUniform(baseName + "constant", pointLights[i].GetConstant());
		mainShader.SetUniform(baseName + "linear", pointLights[i].GetLinear());
		mainShader.SetUniform(baseName + "exponent", pointLights[i].GetExponent());

		int offset = 0;
		baseName = string("u_omniShadowMaps[") + to_string(i + offset) + string("].");
		mainShader.BindSampler(baseName + "shadowMap", OMNIDIR_SHADOWMAP_TEXUNIT + i, pointLights[i].GetShadowMap()->GetShadowMapTexture());
		mainShader.SetUniform(baseName + "farPlane", pointLights[i].GetFarPlane());
	}

	//Set spot lights
	int spotLightsToProcess = spotLightCount;
	if (spotLightCount > MAX_SPOT_LIGHTS) { spotLightsToProcess = MAX_SPOT_LIGHTS; }
	mainShader.SetUniform("u_spotLightCount", spotLightsToProcess);

	for (int i = 0; i < spotLightsToProcess; i++) //For each pointLight on the scene connect light get location of variables in shader
	{
		string baseName;
		baseName = string("u_spotLights[") + to_string(i) + string("].");
		mainShader.SetUniform(baseName + "base.base.color", spotLights[i].GetColor());
		mainShader.SetUniform(baseName + "base.base.ambientIntensity", spotLights[i].GetAmbientIntensity());
		mainShader.SetUniform(baseName + "base.base.diffuseIntensity", spotLights[i].GetDiffuseIntensity());
		mainShader.SetUniform(baseName + "base.position", spotLights[i].GetPosition());
		mainShader.SetUniform(baseName + "base.constant", spotLights[i].GetConstant());
		mainShader.SetUniform(baseName + "base.linear", spotLights[i].GetLinear());
		mainShader.SetUniform(baseName + "base.exponent", spotLights[i].GetExponent());
		mainShader.SetUniform(baseName + "direction", spotLights[i].GetDirection());
		mainShader.SetUniform(baseName + "edge", spotLights[i].GetProcEdge());

		int offset = pointLightCount;
		baseName = string("u_omniShadowMaps[") + to_string(i + offset) + string("].");
		mainShader.BindSampler(baseName + "shadowMap", OMNIDIR_SHADOWMAP_TEXUNIT + offset + i, spotLights[i].GetShadowMap()->GetShadowMapTexture());
		mainShader.SetUniform(baseName + "farPlane", spotLights[i].GetFarPlane());
	}

	mainShader.SetSampler("s_textureDiffuse", DIFFUSE_TEXUNIT); //Set sampler in shader to look for texture at texture unit DIFFUSE_TEXUNIT
	mainShader.SetSampler("s_textureNormal", NORMAL_TEXUNIT); //Set sampler in shader to look for texture at texture unit NORMAL_TEXUNIT

	//glm::vec3 lowerLight = camera.getCameraPosition();
	//lowerLight.y -= 0.3f;
	//spotLights[0].SetFlash(lowerLight, camera.getCameraDirection()); //Flashlight

	//We should validate shader after assigning uniform variables, otherwise when we have two different types of texture(samplerCube and sampler2d) assigned to
	//the default texture unit(before assigning uniform variables it is 0) will generate an error
	mainShader.Validate();
	RenderScene(&mainShader);

	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glDepthMask(GL_FALSE);
	glDisable(GL_DEPTH_TEST);

	glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

}

void ScreenPass()
{
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glDisable(GL_DEPTH_TEST); // disable depth test so screen-space quad isn't discarded due to depth test.
	glClearColor(1.0f, 1.0f, 1.0f, 1.0f); // set clear color to white (not really necessery actually, since we won't be able to see behind the quad anyways)
	glClear(GL_COLOR_BUFFER_BIT);

	screenShader.UseShader();
	glBindVertexArray(screenQuadVAO);

	glActiveTexture(GL_TEXTURE0);

	if (postProcessNumber == 0) //Bind proper buffer (depending on number of postprocesses)
	{
		glBindTexture(GL_TEXTURE_2D, mainTextureColorBuffer); // use the color attachment texture as the texture of the quad plane
	}
	else
	{
		if (postProcessNumber % 2 == 1) //First, Third, etc
		{
			glBindTexture(GL_TEXTURE_2D, postProcessColorBuffer0);
		}
		else
		{
			glBindTexture(GL_TEXTURE_2D, postProcessColorBuffer1);
		}
	}
	postProcessNumber = 0;
	screenShader.Validate();
	glDrawArrays(GL_TRIANGLES, 0, 6);

}
#pragma endregion

//--------------------------------RENDERING(PostProcessing Passes)----------------------------------
#pragma region RENDERING POST-PROCESSING

glm::uvec2 GetPostProcessingFramebuffers() //Alternately providing correct buffer to write to (read from, write to)
{
	if (postProcessNumber == 0)
	{
		return glm::uvec2(mainTextureColorBuffer, postProcessFramebuffer0);
	}

	if (postProcessNumber % 2 == 1) //First, Third, etc
	{
		return glm::uvec2(postProcessColorBuffer0, postProcessFramebuffer1);
	}
	else
	{
		return glm::uvec2(postProcessColorBuffer1, postProcessFramebuffer0);
	}
}

void SSAOPostProcessingPass()
{
	if (postProcessingEffectsSettings.ssao.IsEnabled())
	{
		glm::uvec2 buffers = GetPostProcessingFramebuffers();

		glBindFramebuffer(GL_FRAMEBUFFER, buffers.y);
		glDisable(GL_DEPTH_TEST); // disable depth test so screen-space quad isn't discarded due to depth test.
		glClearColor(1.0f, 1.0f, 1.0f, 1.0f); // set clear color to white (not really necessery actually, since we won't be able to see behind the quad anyways)
		glClear(GL_COLOR_BUFFER_BIT);
		
		//Generate random sample values in hemisphere kernel sample
		std::uniform_real_distribution<float> randomFloats(0.0, 1.0); // random floats between 0.0 - 1.0
		std::default_random_engine generator;
		std::vector<glm::vec3> ssaoKernel;
		for (unsigned int i = 0; i < 64; ++i)
		{
			glm::vec3 sample(randomFloats(generator) * 2.0 - 1.0, randomFloats(generator) * 2.0 - 1.0, randomFloats(generator));
			sample = glm::normalize(sample);
			sample *= randomFloats(generator);
			float scale = (float)i / 64.0;
			auto lerp = [](float a, float b, float f) { return a + f * (b - a); };
			scale = lerp(0.1f, 1.0f, scale * scale); //Move samples closer to origin of the hemisphere
			sample *= scale;
			ssaoKernel.push_back(sample);
		}

		//Random kernel rotations (using texture tiled over the screen)
		std::vector<glm::vec3> ssaoNoise;
		for (unsigned int i = 0; i < 16; i++)
		{
			glm::vec3 noise(randomFloats(generator) * 2.0 - 1.0, randomFloats(generator) * 2.0 - 1.0, 0.0f);
			ssaoNoise.push_back(noise);
		}

		GLuint noiseTexture;
		glGenTextures(1, &noiseTexture);
		glBindTexture(GL_TEXTURE_2D, noiseTexture);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F, 4, 4, 0, GL_RGB, GL_FLOAT, &ssaoNoise[0]);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);



		ssaoPostProcessingShader.UseShader();
		glBindVertexArray(screenQuadVAO);

		ssaoPostProcessingShader.BindSampler("s_screenTexture", 0, buffers.x);
		//ssaoPostProcessingShader.BindSampler("s_position", 1, null);
		//ssaoPostProcessingShader.BindSampler("s_normal", 2, normal);
		ssaoPostProcessingShader.BindSampler("s_texNoise", 3, noiseTexture);

		for (int i = 0; i < 64; i++)
		{
			string baseName = string("u_samples[") + to_string(i) + string("]");
			ssaoPostProcessingShader.SetUniform(baseName, ssaoKernel[i]);
		}


		ssaoPostProcessingShader.SetUniform("u_projection", camera.GetProjectionMatrix());
		ssaoPostProcessingShader.SetUniform("u_framebufferDimensions", mainWindow.GetFramebufferWidth(), mainWindow.GetFramebufferHeight());
		ssaoPostProcessingShader.SetUniform("u_kernelSize", 64);
		ssaoPostProcessingShader.SetUniform("u_kernelRadius", 0.1f);
		ssaoPostProcessingShader.SetUniform("u_bias", 0.025f);

		
		

		//logoOverlayPostProcessShader.BindSampler("s_screenTexture", 0, buffers.x); // use the color attachment texture as the texture of the quad plane

		ssaoPostProcessingShader.Validate();
		glDrawArrays(GL_TRIANGLES, 0, 6);

		glBindFramebuffer(GL_FRAMEBUFFER, 0);

		postProcessNumber++;
	}
}

void DepthVisualizePostProcessingPass()
{
	if (postProcessingEffectsSettings.depthVisualize.IsEnabled())
	{
		glm::uvec2 buffers = GetPostProcessingFramebuffers();

		glBindFramebuffer(GL_FRAMEBUFFER, buffers.y);
		glDisable(GL_DEPTH_TEST); // disable depth test so screen-space quad isn't discarded due to depth test.
		glClearColor(1.0f, 1.0f, 1.0f, 1.0f); // set clear color to white (not really necessery actually, since we won't be able to see behind the quad anyways)
		glClear(GL_COLOR_BUFFER_BIT);

		depthVisualizePostProcessingShader.UseShader();
		glBindVertexArray(screenQuadVAO);

		//depthVisualizePostProcessingShader.BindSampler("s_screenTexture", 0, buffers.x); // use the color attachment texture as the texture of the quad plane
		depthVisualizePostProcessingShader.BindSampler("s_depthColorTexture", 1, depthTextureColorBuffer);
		depthVisualizePostProcessingShader.SetUniform("u_near", postProcessingEffectsSettings.depthVisualize.GetNear());
		depthVisualizePostProcessingShader.SetUniform("u_far", postProcessingEffectsSettings.depthVisualize.GetFar());


		depthVisualizePostProcessingShader.Validate();
		glDrawArrays(GL_TRIANGLES, 0, 6);

		glBindFramebuffer(GL_FRAMEBUFFER, 0);

		postProcessNumber++;
	}
}

void InvertPostProcessingPass()
{
	if (postProcessingEffectsSettings.invert.IsEnabled())
	{
		glm::uvec2 buffers = GetPostProcessingFramebuffers();

		glBindFramebuffer(GL_FRAMEBUFFER, buffers.y);
		glDisable(GL_DEPTH_TEST); // disable depth test so screen-space quad isn't discarded due to depth test.
		glClearColor(1.0f, 1.0f, 1.0f, 1.0f); // set clear color to white (not really necessery actually, since we won't be able to see behind the quad anyways)
		glClear(GL_COLOR_BUFFER_BIT);

		invertPostProcessingShader.UseShader();
		glBindVertexArray(screenQuadVAO);

		logoOverlayPostProcessingShader.BindSampler("s_screenTexture", 0, buffers.x); // use the color attachment texture as the texture of the quad plane

		invertPostProcessingShader.Validate();
		glDrawArrays(GL_TRIANGLES, 0, 6);

		glBindFramebuffer(GL_FRAMEBUFFER, 0);

		postProcessNumber++;
	}
}

void GrayscalePostProcessingPass()
{
	if (postProcessingEffectsSettings.grayscale.IsEnabled())
	{
		glm::uvec2 buffers = GetPostProcessingFramebuffers();

		glBindFramebuffer(GL_FRAMEBUFFER, buffers.y);
		glDisable(GL_DEPTH_TEST); // disable depth test so screen-space quad isn't discarded due to depth test.
		glClearColor(1.0f, 1.0f, 1.0f, 1.0f); // set clear color to white (not really necessery actually, since we won't be able to see behind the quad anyways)
		glClear(GL_COLOR_BUFFER_BIT);

		grayscalePostProcessingShader.UseShader();
		glBindVertexArray(screenQuadVAO);

		logoOverlayPostProcessingShader.BindSampler("s_screenTexture", 0, buffers.x); // use the color attachment texture as the texture of the quad plane

		grayscalePostProcessingShader.Validate();
		glDrawArrays(GL_TRIANGLES, 0, 6);

		glBindFramebuffer(GL_FRAMEBUFFER, 0);

		postProcessNumber++;
	}
}

void EdgeDetectionPostProcessingPass()
{
	if (postProcessingEffectsSettings.edgeDetection.IsEnabled())
	{
		glm::uvec2 buffers = GetPostProcessingFramebuffers();

		glBindFramebuffer(GL_FRAMEBUFFER, buffers.y);
		glDisable(GL_DEPTH_TEST); // disable depth test so screen-space quad isn't discarded due to depth test.
		glClearColor(1.0f, 1.0f, 1.0f, 1.0f); // set clear color to white (not really necessery actually, since we won't be able to see behind the quad anyways)
		glClear(GL_COLOR_BUFFER_BIT);

		edgeDetectionPostProcessingShader.UseShader();
		glBindVertexArray(screenQuadVAO);

		logoOverlayPostProcessingShader.BindSampler("s_screenTexture", 0, buffers.x); // use the color attachment texture as the texture of the quad plane


		//Manipulating shader uniform variables
		edgeDetectionPostProcessingShader.SetUniform("u_offset", postProcessingEffectsSettings.edgeDetection.GetOffsetValue());


		edgeDetectionPostProcessingShader.Validate();
		glDrawArrays(GL_TRIANGLES, 0, 6);

		glBindFramebuffer(GL_FRAMEBUFFER, 0);

		postProcessNumber++;
	}
}

void ColorCorrectionPostProcessingPass()
{
	if (postProcessingEffectsSettings.colorCorrection.IsEnabled())
	{
		glm::uvec2 buffers = GetPostProcessingFramebuffers();

		glBindFramebuffer(GL_FRAMEBUFFER, buffers.y);
		glDisable(GL_DEPTH_TEST); // disable depth test so screen-space quad isn't discarded due to depth test.
		glClearColor(1.0f, 1.0f, 1.0f, 1.0f); // set clear color to white (not really necessery actually, since we won't be able to see behind the quad anyways)
		glClear(GL_COLOR_BUFFER_BIT);

		colorCorrectionPostProcessingShader.UseShader();
		glBindVertexArray(screenQuadVAO);

		colorCorrectionPostProcessingShader.BindSampler("s_screenTexture", 0, buffers.x); // use the color attachment texture as the texture of the quad plane

		colorCorrectionPostProcessingShader.Validate();
		glDrawArrays(GL_TRIANGLES, 0, 6);

		glBindFramebuffer(GL_FRAMEBUFFER, 0);

		postProcessNumber++;
	}
}

void VingettePostProcessingPass()
{
	if (postProcessingEffectsSettings.vignette.IsEnabled())
	{
		glm::uvec2 buffers = GetPostProcessingFramebuffers();

		glBindFramebuffer(GL_FRAMEBUFFER, buffers.y);
		glDisable(GL_DEPTH_TEST); // disable depth test so screen-space quad isn't discarded due to depth test.
		glClearColor(1.0f, 1.0f, 1.0f, 1.0f); // set clear color to white (not really necessery actually, since we won't be able to see behind the quad anyways)
		glClear(GL_COLOR_BUFFER_BIT);

		vignettePostProcessingShader.UseShader();
		glBindVertexArray(screenQuadVAO);

		vignettePostProcessingShader.BindSampler("s_screenTexture", 0, buffers.x); // use the color attachment texture as the texture of the quad plane
		vignettePostProcessingShader.SetUniform("u_resolution", glm::vec2(mainWindow.GetWindowWidth(), mainWindow.GetWindowHeight()));
		vignettePostProcessingShader.SetUniform("u_radius", postProcessingEffectsSettings.vignette.GetRadius());
		vignettePostProcessingShader.SetUniform("u_softness", postProcessingEffectsSettings.vignette.GetSoftness());
		vignettePostProcessingShader.SetUniform("u_intensity", postProcessingEffectsSettings.vignette.GetIntensity());

		edgeDetectionPostProcessingShader.Validate();
		glDrawArrays(GL_TRIANGLES, 0, 6);

		glBindFramebuffer(GL_FRAMEBUFFER, 0);

		postProcessNumber++;
	}
}

void LogoOverlayPostProcessingPass()
{
	if (postProcessingEffectsSettings.logoOverlay.IsEnabled())
	{
		glm::uvec2 buffers = GetPostProcessingFramebuffers();

		glBindFramebuffer(GL_FRAMEBUFFER, buffers.y);
		glDisable(GL_DEPTH_TEST); // disable depth test so screen-space quad isn't discarded due to depth test.
		glClearColor(1.0f, 1.0f, 1.0f, 1.0f); // set clear color to white (not really necessery actually, since we won't be able to see behind the quad anyways)
		glClear(GL_COLOR_BUFFER_BIT);

		logoOverlayPostProcessingShader.UseShader();
		glBindVertexArray(screenQuadVAO);

		logoOverlayPostProcessingShader.BindSampler("s_screenTexture", 0, buffers.x);
		logoOverlayPostProcessingShader.BindSampler("s_logoOverlayTexture", 1, logoOverlayTexture.GetTextureID());
		logoOverlayPostProcessingShader.SetUniform("u_logoOverlayTint", glm::vec4(0.2f, 0.2f, 0.2f, 0.0f));
		logoOverlayPostProcessingShader.SetUniform("u_framebufferDimensions", mainWindow.GetFramebufferWidth(), mainWindow.GetFramebufferHeight());
		logoOverlayPostProcessingShader.SetUniform("u_logoOverlayTextureDimensions", logoOverlayTexture.GetWidth(), logoOverlayTexture.GetHeight());

		logoOverlayPostProcessingShader.Validate();
		glDrawArrays(GL_TRIANGLES, 0, 6);

		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		postProcessNumber++;
	}
}

#pragma endregion

//---------------------------------------------MAIN-------------------------------------------------
int main()
{
	cout << "----> Mothman Rendering Engine Initialization <----" << endl;

	//mainWindow = Window()

	mainWindow = Window(WINDOW_SIZE_WIDTH_START, WINDOW_SIZE_HEIGHT_START);
	mainWindow.Initialise();

	InitRenderer();
	CreateShaders();
	CreateObjects();

	camera = Camera(glm::vec3(5.0f, 2.0f, -5.0f), glm::vec3(0.0f, 1.0f, 0.0f), 135.0f, 0.0f, 13.0f, 0.4f); //Creating camera

	LoadModels();

	//Loading textures
	brickTexture = Texture("res/Textures/brick.png", TexType::Diffuse);
	brickTexture.LoadTexture();
	dirtTexture = Texture("res/Textures/dirt.png", TexType::Diffuse);
	dirtTexture.LoadTexture();
	//brickTexture.UseTexture(); //This texture will be used for the rest of the code

	logoOverlayTexture = Texture("res/Textures/MothmanRenderingEngineLogoTextOverlay.png", TexType::None);
	logoOverlayTexture.LoadTexture();

	//Creating materials
	shinyMaterial = Material(1.0f, 32);
	dullMaterial = Material(0.2f, 2);

	CreateLights();
	CreateSkybox();

	camera.CalculateProjectionMatrix(60.0f, (GLfloat)WINDOW_SIZE_WIDTH_START, (GLfloat)WINDOW_SIZE_HEIGHT_START, 0.1f, 200.0f);

	postProcessingEffectsSettings = PostProcessingEffectsSettings();
	//postProcessingEffectsSettings.ssao.Enable();
	//postProcessingEffectsSettings.invert.Enable();
	postProcessingEffectsSettings.vignette.Enable();
	//postProcessingEffectsSettings.edgeDetection.Enable();
	//postProcessingEffectsSettings.edgeDetection.SetOffsetValue(2);
	postProcessingEffectsSettings.logoOverlay.Enable();
	//postProcessingEffectsSettings.depthVisualize.Enable();

	terrainController = TerrainController(&terrainShader, &camera);
	GLuint Terrain1LODRange[8] = { 50, 15, 5, 2, 0, 0, 0, 0 };
	terrainController.AddTerrain("Terrain1", 8, 100, 30, Terrain1LODRange, 1, 0.03f, 0.01f, "res/Textures/Terrain/Terrain1_Height.bmp", "res/Textures/Terrain/Terrain1_Normal.png");
	terrainController.GetTerrain("Terrain1")->GetTerrainSettings()->SetTessellationFactor(8);
	terrainController.GetTerrain("Terrain1")->GetTerrainSettings()->SetTessellationSlope(1.5f);
	terrainController.GetTerrain("Terrain1")->GetTerrainSettings()->SetTessellationShift(0.05f);

	ImGuiInit();

	while (!mainWindow.GetShouldClose()) //It will loop until this function detects that the window should be closed based on variable hidden inside GLFW 
	{
		if (mainWindow.CheckWindowSize()) //Check if window size has changed
		{
			DeleteRenderBuffers();
			CreateRenderBuffers();
			camera.CalculateProjectionMatrix(60.0f, (GLfloat)mainWindow.GetWindowWidth(), (GLfloat)mainWindow.GetWindowHeight(), 0.1f, 200.0f); //Enough to calculate once
		}

		ImGuiNewFrame();

		if (showControlPanel)
		{
			ImGuiMainControlPanel();
		}

		if (showDemoWindow) //ImGui demo window
		{
			ImGuiDemoWindow();
		}

		//Time calculations
		if (rendererFirstFrame)
		{
			rendererStartTime = glfwGetTime();
			rendererFirstFrame = false;
		}

		GLfloat now = glfwGetTime(); // = SDL_GetPerformanceCounter(); - if using STL instead of GLFW
		deltaTime = now - lastTime; // = (now - lastTime) * 1000/SDL_GetPerformanceFrequency(); - if using STL instead of GLFW
		lastTime = now;

		//Get and handle user input events
		glfwPollEvents();
		if (ImGui::IsWindowFocused(ImGuiHoveredFlags_AnyWindow) == false)
		{
			camera.KeyControl(mainWindow.GetKeys(), deltaTime);
			camera.MouseControl(mainWindow.GetKeys(), mainWindow.GetMouseButtons(), mainWindow.GetXChange(), mainWindow.GetYChange(), deltaTime);
		}

		//Misc player controls
		MovingMeshes();
		ToggleControlPanel();
		Log();

		//---------------------RENDER PASSES-----------------------
		DepthPass();
		DirectionalShadowMapPass(&directionalLight); //Shadow pass for directional shadows (once because we have only one light)
		for (size_t i = 0; i < pointLightCount; i++) { OmniShadowMapPass(&pointLights[i]); } //Shadow pass for omni-directional point light shadows (as many times as we have point lights)
		for (size_t i = 0; i < spotLightCount; i++) { OmniShadowMapPass(&spotLights[i]); } //Shadow pass for omni-directional spot light shadows (as many times as we have spot lights)

		camera.CalculateViewMatrix(); //If camera changes its position this needs to be calculated each frame
		RenderPass(camera.GetProjectionMatrix(), camera.GetViewMatrix());
		if (postProcessingEffectsSettings.IsEnabled())
		{
			SSAOPostProcessingPass();
			InvertPostProcessingPass();
			GrayscalePostProcessingPass();
			EdgeDetectionPostProcessingPass();
			VingettePostProcessingPass();
			LogoOverlayPostProcessingPass();
			DepthVisualizePostProcessingPass();
		}
		ScreenPass();

		glUseProgram(0);
		//---------------------------------------------------------

		ImGuiRender();

		mainWindow.SwapBuffers();//Swap back buffer and front buffer
	}

	ImGuiCleanUp();
	DeleteRenderBuffers();

	//Window cleanup
	glfwDestroyWindow(mainWindow.GetWindow());
	glfwTerminate();

	return 0;
}


