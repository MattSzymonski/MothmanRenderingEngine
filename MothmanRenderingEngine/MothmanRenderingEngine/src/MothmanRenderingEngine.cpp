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
//Postprocessing shaders are not being loaded if they are not used

//TODO
//camera nice moving, all postprocessing effect in control panel, lod in terrain settings in control panel, MothmanRenderingEngine.hpp + new, skybox delete


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


Window mainWindow;
Camera camera; //Camera class

Shader mainShader;
Shader skyboxShader;
Shader screenShader;
Shader directionalShadowShader;
Shader omniShadowShader;
Shader invertPostProcessShader;
Shader grayscalePostProcessShader;
Shader edgeDetectionPostProcessShader;
Shader colorCorrectionPostProcessShader;
Shader vignettePostProcessShader;
Shader logoOverlayPostProcessShader;
Shader terrainShader;

std::vector<Mesh*> meshList; //List of Mesh pointers
Model floorModel;
Model testModel_Human;
Model testModel_Cube;
Model testModel_TanTest;
Model testModel_CubeTan;
Model texTestModel;

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

//-----------------Misc----------------
GLfloat deltaTime = 0.0f; //To maintain same speed on every CPU
GLfloat lastTime = 0.0f; //To maintain same speed on every CPU

bool wireFrameState = false;
bool showSkybox = true;
unsigned int postProcessNumber = 0;
PostProcessingEffectsSettings postProcessingEffectsSettings;

bool showControlPanel = true;
bool showDemoWindow = false;

bool rendererFirstFrame = true;
double rendererStartTime = 0;

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

//-------------------------------------MANAGE RENDERER BUFFERS------------------------------------------
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
	//Create tetrahedron
	std::vector<GLfloat> vertices;
	vertices.insert(vertices.end(), { -1.0f, -1.0f,  0.0f,     0.0f, 0.0f,    0.0f, 0.0f, 0.0f,	 0.0f, 0.0f, 0.0f });
	vertices.insert(vertices.end(), { 0.0f,  -1.0f,  1.0f,     0.5f, 0.0f,    0.0f, 0.0f, 0.0f,	 0.0f, 0.0f, 0.0f });
	vertices.insert(vertices.end(), { 1.0f,  -1.0f,  0.0f,     1.0f, 0.0f,    0.0f, 0.0f, 0.0f,	 0.0f, 0.0f, 0.0f });
	vertices.insert(vertices.end(), { 0.0f,   1.0f,  0.0f,     0.5f, 1.0f,    0.0f, 0.0f, 0.0f,	 0.0f, 0.0f, 0.0f });

	std::vector<unsigned int> indices;
	indices.insert(indices.end(), { 0, 3, 1 });
	indices.insert(indices.end(), { 1, 3, 2 });
	indices.insert(indices.end(), { 2, 3, 0 });
	indices.insert(indices.end(), { 0, 1, 2 });

	VertexOperations::CalcAverageNormals(indices, indices.size(), vertices, vertices.size(), 11, 5); //Calculate average normal vectors for each vertex (Phong shading)
	VertexOperations::CalculateTangents(indices, indices.size(), vertices, vertices.size(), 11, 3, 8);

	Mesh *obj1 = new Mesh(); //From mesh.h
	obj1->CreateMesh(&vertices[0], &indices[0], 44, 12);
	meshList.push_back(obj1); //Add to the end of the list

	//Create floor
	std::vector<GLfloat> verticesFloor;
	verticesFloor.insert(verticesFloor.end(), { -10.0f, 0.0f, -10.0f,	0.0f, 0.0f,		0.0f, -1.0f, 0.0f,  0.0f, 0.0f, 0.0f });
	verticesFloor.insert(verticesFloor.end(), { 10.0f, 0.0f, -10.0f,	10.0f, 0.0f,	0.0f, -1.0f, 0.0f,  0.0f, 0.0f, 0.0f });
	verticesFloor.insert(verticesFloor.end(), { -10.0f, 0.0f, 10.0f,	0.0f, 10.0f,	0.0f, -1.0f, 0.0f,  0.0f, 0.0f, 0.0f });
	verticesFloor.insert(verticesFloor.end(), { 10.0f, 0.0f, 10.0f,		10.0f, 10.0f,	0.0f, -1.0f, 0.0f,  0.0f, 0.0f, 0.0f });

	std::vector<unsigned int> indicesFloor;
	//indicesFloor.insert(indicesFloor.end(), { 1, 0, 3 });
	//indicesFloor.insert(indicesFloor.end(), { 0, 3, 2 });

	indicesFloor.insert(indicesFloor.end(), { 0, 1, 2 });
	indicesFloor.insert(indicesFloor.end(), { 2, 1, 3 });

	VertexOperations::CalcAverageNormals(indicesFloor, indicesFloor.size(), verticesFloor, verticesFloor.size(), 11, 5); //Calculate average normal vectors for each vertex (Phong shading)
	VertexOperations::CalculateTangents(indicesFloor, indicesFloor.size(), verticesFloor, verticesFloor.size(), 11, 3, 8);

	Mesh *obj2 = new Mesh(); //From mesh.h
	obj2->CreateMesh(&verticesFloor[0], &indicesFloor[0], 44, 5);
	meshList.push_back(obj2); //Add to the end of the list
}

void LoadModels()
{
	//Loading models
	testModel_Human = Model();
	testModel_Human.LoadModel("res/Models/CCC.fbx");

	floorModel = Model();
	floorModel.LoadModel("res/Models/Floor.fbx");

	testModel_Cube = Model();
	testModel_Cube.LoadModel("res/Models/Cube.fbx");

	testModel_TanTest = Model();
	testModel_TanTest.LoadModel("res/Models/Tan.fbx");

	testModel_CubeTan = Model();
	testModel_CubeTan.LoadModel("res/Models/CubeTan.fbx");

	texTestModel = Model();
	texTestModel.LoadModel("res/Models/w.fbx");
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
		0.0f, 0.5f,
		-45.0f, -15.0f, 0.0f);
	
	/*
	//Create pointlights
	pointLights[0] = PointLight(1024, 1024,
		0.1f, 100.0f,
		0.0f, 0.0f, 1.0f,
		0.0f, 2.0f,
		-5.0f, 1.0f, -5.0f,
		0.3f, 0.1f, 0.0f);
	pointLightCount++;
	initialPos_Light = pointLights[0].GetPosition();

	pointLights[1] = PointLight(1024, 1024,
		0.1f, 100.0f,
		0.0f, 1.0f, 0.0f,
		0.0f, 1.5f,
		-9.0f, 2.0f, -2.0f,
		0.3f, 0.1f, 0.1f);
	pointLightCount++;


	//Create spotlights
	spotLights[0] = SpotLight(1024, 1024,
		0.1f, 100.0f,
		1.0f, 1.0f, 1.0f,
		0.0f, 5.0f,
		-8.0f, 1.0f, -4.0f,
		45.0f, 0.0f, 0.0f,
		0.50f, 0.0f, 0.0f,
		10.0f);
	spotLightCount++;

	spotLights[1] = SpotLight(1024, 1024,
		0.1f, 100.0f,
		1.0f, 0.0f, 0.0f,
		0.0f, 5.0f,
		0.0f, 1.0f, 0.0f,
		0.0f, 0.0f, 0.0f,
		0.40f, 0.10f, 0.10f,
		15.0f);
	spotLightCount++;
	*/
}

#pragma endregion

//-----------------------------------------PLAYER INPUT---------------------------------------------
#pragma region PLAYER INPUT

void MovingMeshes()
{

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
			ImGui::Separator();
		}
	}

	if (ImGui::CollapsingHeader("Other Settings"))
	{
		ImGui::Checkbox("Wireframe", &wireFrameState);
		ImGui::Checkbox("Skybox", &showSkybox);
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
	invertPostProcessShader = Shader("Invert PostProcess Shader");
	invertPostProcessShader.CreateFromFiles("res/Shaders/screen_vertex.shader", "res/Shaders/PostProcessing/invert_postprocess_fragment.shader");
	invertPostProcessShader.RegisterSampler("sampler2D", "s_screenTexture");

	//----------------
	grayscalePostProcessShader = Shader("Grayscale PostProcess Shader");
	grayscalePostProcessShader.CreateFromFiles("res/Shaders/screen_vertex.shader", "res/Shaders/PostProcessing/grayscale_postprocess_fragment.shader");
	grayscalePostProcessShader.RegisterSampler("sampler2D", "s_screenTexture");

	//----------------
	edgeDetectionPostProcessShader = Shader("Edge Detection PostProcess Shader");
	edgeDetectionPostProcessShader.CreateFromFiles("res/Shaders/screen_vertex.shader", "res/Shaders/PostProcessing/edgedetection_postprocess_fragment.shader");
	edgeDetectionPostProcessShader.RegisterSampler("sampler2D", "s_screenTexture");
	edgeDetectionPostProcessShader.RegisterUniform("float", "u_offset");

	//----------------
	//colorCorrectionPostProcessShader = Shader("Color Correction Shader");
	//colorCorrectionPostProcessShader.CreateFromFiles("res/Shaders/screen_vertex.shader", "res/Shaders/PostProcessing/colorcorrection_postprocess_fragment.shader");
	//colorCorrectionPostProcessShader.RegisterSampler("sampler2D", "s_screenTexture");

	//----------------
	vignettePostProcessShader = Shader("Vignette Shader");
	vignettePostProcessShader.CreateFromFiles("res/Shaders/screen_vertex.shader", "res/Shaders/PostProcessing/vignette_postprocess_fragment.shader");
	vignettePostProcessShader.RegisterSampler("sampler2D", "s_screenTexture");
	vignettePostProcessShader.RegisterUniform("vec2", "u_resolution");
	vignettePostProcessShader.RegisterUniform("float", "u_radius");
	vignettePostProcessShader.RegisterUniform("float", "u_softness");
	vignettePostProcessShader.RegisterUniform("float", "u_intensity");

	//----------------
	logoOverlayPostProcessShader = Shader("Logo Overlay Shader");
	logoOverlayPostProcessShader.CreateFromFiles("res/Shaders/screen_vertex.shader", "res/Shaders/PostProcessing/logooverlay_postprocess_fragment.shader");
	logoOverlayPostProcessShader.RegisterSampler("sampler2D", "s_screenTexture");
	logoOverlayPostProcessShader.RegisterSampler("sampler2D", "s_logoOverlayTexture");
	logoOverlayPostProcessShader.RegisterUniform("vec4", "u_logoOverlayTint");
	logoOverlayPostProcessShader.RegisterUniform("vec2", "u_framebufferDimensions");
	logoOverlayPostProcessShader.RegisterUniform("vec2", "u_logoOverlayTextureDimensions");
	

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
}

void RenderScene(Shader *shader)
{
	//SPECIFIC MODEL OPERATIONS
	glm::mat4 model;

	//Tetrahedron
	model = glm::mat4(); //Creating identity matrix
	model = glm::translate(model, glm::vec3(0.0f, 4.0f, -2.5f)); //Transforming identity matrix into translation matrix
	//glUniformMatrix4fv(uniformModel, 1, GL_FALSE, glm::value_ptr(model)); //Set uniform variable value in shader (uniform variable in shader, count, transpose?, pointer to matrix)	
	shader->SetUniform("u_model", model);

	brickTexture.UseTexture();
	if (shader == &mainShader) { shinyMaterial.UseMaterial(&mainShader, "u_material.specularIntensity", "u_material.shininess"); }
	meshList[0]->RenderMesh();

	//Floor
	model = glm::mat4(); //Creating identity matrix
	model = glm::translate(model, glm::vec3(0.0f, 0.0f, -0.0f)); //Transforming identity matrix into translation matrix													
	shader->SetUniform("u_model", model);
	dirtTexture.UseTexture();
	if (shader == &mainShader) { shinyMaterial.UseMaterial(&mainShader, "u_material.specularIntensity", "u_material.shininess"); }
	floorModel.RenderModel();
	//meshList[1]->RenderMesh(); //Old verices floor

	//TanTest
	model = glm::mat4(); //Creating identity matrix
	model = glm::translate(model, glm::vec3(6.0f, 1.0f, -0.0f)); //Transforming identity matrix into translation matrix
	shader->SetUniform("u_model", model);
	if (shader == &mainShader) { shinyMaterial.UseMaterial(&mainShader, "u_material.specularIntensity", "u_material.shininess"); }
	testModel_TanTest.RenderModel();

	//CubeTest
	model = glm::mat4(); //Creating identity matrix
	model = glm::translate(model, glm::vec3(0.0f, 0.5f, 1.0f)); //Transforming identity matrix into translation matrix
	shader->SetUniform("u_model", model);
	if (shader == &mainShader) { shinyMaterial.UseMaterial(&mainShader, "u_material.specularIntensity", "u_material.shininess"); }
	testModel_CubeTan.RenderModel();

	//Test Human
	model = glm::mat4(); //Creating identity matrix
	//model = glm::translate(model, glm::vec3(0.0f + movementCurrentTranslation_Human, 0.0f, -0.0f)); //Transforming identity matrix into translation matrix
	model = glm::translate(model, glm::vec3(0.0f, 0.0f, -0.0f)); //Transforming identity matrix into translation matrix
	shader->SetUniform("u_model", model);
	if (shader == &mainShader) { shinyMaterial.UseMaterial(&mainShader, "u_material.specularIntensity", "u_material.shininess"); }
	testModel_Human.RenderModel();

	//Cube
	model = glm::mat4(); //Creating identity matrix
	model = glm::translate(model, glm::vec3(0.0f, 0.5f, 3.0f)); //Transforming identity matrix into translation matrix
	shader->SetUniform("u_model", model);
	if (shader == &mainShader) { shinyMaterial.UseMaterial(&mainShader, "u_material.specularIntensity", "u_material.shininess"); }
	testModel_Cube.RenderModel();

	//Cube2
	model = glm::mat4(); //Creating identity matrix
	model = glm::translate(model, glm::vec3(1.0f, 0.5f, -4.0f)); //Transforming identity matrix into translation matrix
	shader->SetUniform("u_model", model);
	if (shader == &mainShader) { shinyMaterial.UseMaterial(&mainShader, "u_material.specularIntensity", "u_material.shininess"); }
	testModel_Cube.RenderModel();

	//Cube3
	model = glm::mat4(); //Creating identity matrix
	model = glm::translate(model, glm::vec3(-7.0f, 0.5f, -5.0f)); //Transforming identity matrix into translation matrix
	shader->SetUniform("u_model", model);
	if (shader == &mainShader) { shinyMaterial.UseMaterial(&mainShader, "u_material.specularIntensity", "u_material.shininess"); }
	testModel_Cube.RenderModel();

	//Cube4
	model = glm::mat4(); //Creating identity matrix
	model = glm::translate(model, glm::vec3(-5.0f, 0.5f, -8.0f)); //Transforming identity matrix into translation matrix
	shader->SetUniform("u_model", model);
	if (shader == &mainShader) { shinyMaterial.UseMaterial(&mainShader, "u_material.specularIntensity", "u_material.shininess"); }
	testModel_Cube.RenderModel();

	//Cube5
	model = glm::mat4(); //Creating identity matrix
	model = glm::translate(model, glm::vec3(-3.0f, 0.5f, -3.0f)); //Transforming identity matrix into translation matrix
	shader->SetUniform("u_model", model);
	if (shader == &mainShader) { shinyMaterial.UseMaterial(&mainShader, "u_material.specularIntensity", "u_material.shininess"); }
	testModel_Cube.RenderModel();

}
#pragma endregion

//---------------------------------------RENDERING(Passes)------------------------------------------
#pragma region RENDERING PASSES

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
	terrainController.GetTerrain("Terrain1")->GetTerrainModule()->Render(true);


	mainShader.UseShader(); //Set default shader active
	mainShader.SetUniform("u_projection", projectionMatrix);
	mainShader.SetUniform("u_view", viewMatrix);
	mainShader.SetUniform("u_cameraPosition", camera.GetCameraPosition());

	//Set directional light
	mainShader.SetUniform("u_directionalLightTransform", directionalLight.CalculateLightTransform());
	mainShader.SetUniform("u_directionalLight.base.color", directionalLight.GetColour());
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
		string baseName = string("u_pointLights[") + to_string(i) + string("].");
		mainShader.SetUniform(baseName + "base.color", pointLights[i].GetColour());
		mainShader.SetUniform(baseName + "base.ambientIntensity", pointLights[i].GetAmbientIntensity());
		mainShader.SetUniform(baseName + "base.diffuseIntensity", pointLights[i].GetDiffuseIntensity());
		mainShader.SetUniform(baseName + "position", pointLights[i].GetPosition());
		mainShader.SetUniform(baseName + "constant", pointLights[i].GetConstant());
		mainShader.SetUniform(baseName + "linear", pointLights[i].GetLinear());
		mainShader.SetUniform(baseName + "exponent", pointLights[i].GetExponent());

		int offset = 0;
		string baseNameSampler = string("s_omniShadowMaps[") + to_string(i + offset) + string("].");
		string baseNameUniform = string("u_omniShadowMaps[") + to_string(i + offset) + string("].");
		mainShader.BindSampler(baseNameSampler + "shadowMap", OMNIDIR_SHADOWMAP_TEXUNIT + i, pointLights[i].GetShadowMap()->GetShadowMapTexture());
		mainShader.SetUniform(baseNameUniform + "farPlane", pointLights[i].GetFarPlane());
	}

	//Set spot lights
	int spotLightsToProcess = spotLightCount;
	if (spotLightCount > MAX_SPOT_LIGHTS) { spotLightsToProcess = MAX_SPOT_LIGHTS; }
	mainShader.SetUniform("u_spotLightCount", spotLightsToProcess);

	for (int i = 0; i < spotLightsToProcess; i++) //For each pointLight on the scene connect light get location of variables in shader
	{
		string baseName = string("u_spotLights[") + to_string(i) + string("].");
		mainShader.SetUniform(baseName + "base.base.color", spotLights[i].GetColour());
		mainShader.SetUniform(baseName + "base.base.ambientIntensity", spotLights[i].GetAmbientIntensity());
		mainShader.SetUniform(baseName + "base.base.diffuseIntensity", spotLights[i].GetDiffuseIntensity());
		mainShader.SetUniform(baseName + "base.position", spotLights[i].GetPosition());
		mainShader.SetUniform(baseName + "base.constant", spotLights[i].GetConstant());
		mainShader.SetUniform(baseName + "base.linear", spotLights[i].GetLinear());
		mainShader.SetUniform(baseName + "base.exponent", spotLights[i].GetExponent());
		mainShader.SetUniform(baseName + "direction", spotLights[i].GetDirection());
		mainShader.SetUniform(baseName + "edge", spotLights[i].GetProcEdge());

		int offset = pointLightCount;
		string baseNameSampler = string("s_omniShadowMaps[") + to_string(i + offset) + string("].");
		string baseNameUniform = string("u_omniShadowMaps[") + to_string(i + offset) + string("].");
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

glm::uvec2 GetPostProcessFramebuffers() //Alternately providing correct buffer to write to (read from, write to)
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

void InvertPostProcessPass()
{
	if (postProcessingEffectsSettings.invert.IsEnabled())
	{
		glm::uvec2 buffers = GetPostProcessFramebuffers();

		glBindFramebuffer(GL_FRAMEBUFFER, buffers.y);
		glDisable(GL_DEPTH_TEST); // disable depth test so screen-space quad isn't discarded due to depth test.
		glClearColor(1.0f, 1.0f, 1.0f, 1.0f); // set clear color to white (not really necessery actually, since we won't be able to see behind the quad anyways)
		glClear(GL_COLOR_BUFFER_BIT);

		invertPostProcessShader.UseShader();
		glBindVertexArray(screenQuadVAO);

		logoOverlayPostProcessShader.BindSampler("s_screenTexture", 0, buffers.x); // use the color attachment texture as the texture of the quad plane

		invertPostProcessShader.Validate();
		glDrawArrays(GL_TRIANGLES, 0, 6);
		
		glBindFramebuffer(GL_FRAMEBUFFER, 0);

		postProcessNumber++;
	}
}

void GrayscalePostProcessPass()
{
	if (postProcessingEffectsSettings.grayscale.IsEnabled())
	{
		glm::uvec2 buffers = GetPostProcessFramebuffers();

		glBindFramebuffer(GL_FRAMEBUFFER, buffers.y);
		glDisable(GL_DEPTH_TEST); // disable depth test so screen-space quad isn't discarded due to depth test.
		glClearColor(1.0f, 1.0f, 1.0f, 1.0f); // set clear color to white (not really necessery actually, since we won't be able to see behind the quad anyways)
		glClear(GL_COLOR_BUFFER_BIT);

		grayscalePostProcessShader.UseShader();
		glBindVertexArray(screenQuadVAO);

		logoOverlayPostProcessShader.BindSampler("s_screenTexture", 0, buffers.x); // use the color attachment texture as the texture of the quad plane

		grayscalePostProcessShader.Validate();
		glDrawArrays(GL_TRIANGLES, 0, 6);

		glBindFramebuffer(GL_FRAMEBUFFER, 0);

		postProcessNumber++;
	}
}

void EdgeDetectionPostProcessPass()
{
	if (postProcessingEffectsSettings.edgeDetection.IsEnabled())
	{
		glm::uvec2 buffers = GetPostProcessFramebuffers();

		glBindFramebuffer(GL_FRAMEBUFFER, buffers.y);
		glDisable(GL_DEPTH_TEST); // disable depth test so screen-space quad isn't discarded due to depth test.
		glClearColor(1.0f, 1.0f, 1.0f, 1.0f); // set clear color to white (not really necessery actually, since we won't be able to see behind the quad anyways)
		glClear(GL_COLOR_BUFFER_BIT);

		edgeDetectionPostProcessShader.UseShader();
		glBindVertexArray(screenQuadVAO);

		logoOverlayPostProcessShader.BindSampler("s_screenTexture", 0, buffers.x); // use the color attachment texture as the texture of the quad plane


		//Manipulating shader uniform variables
		edgeDetectionPostProcessShader.SetUniform("u_offset", postProcessingEffectsSettings.edgeDetection.GetOffsetValue());


		edgeDetectionPostProcessShader.Validate();
		glDrawArrays(GL_TRIANGLES, 0, 6);

		glBindFramebuffer(GL_FRAMEBUFFER, 0);

		postProcessNumber++;
	}
}

void ColorCorrectionPostProcessPass() 
{
	if (postProcessingEffectsSettings.colorCorrection.IsEnabled())
	{
		glm::uvec2 buffers = GetPostProcessFramebuffers();

		glBindFramebuffer(GL_FRAMEBUFFER, buffers.y);
		glDisable(GL_DEPTH_TEST); // disable depth test so screen-space quad isn't discarded due to depth test.
		glClearColor(1.0f, 1.0f, 1.0f, 1.0f); // set clear color to white (not really necessery actually, since we won't be able to see behind the quad anyways)
		glClear(GL_COLOR_BUFFER_BIT);

		colorCorrectionPostProcessShader.UseShader();
		glBindVertexArray(screenQuadVAO);

		colorCorrectionPostProcessShader.BindSampler("s_screenTexture", 0, buffers.x); // use the color attachment texture as the texture of the quad plane

		colorCorrectionPostProcessShader.Validate();
		glDrawArrays(GL_TRIANGLES, 0, 6);

		glBindFramebuffer(GL_FRAMEBUFFER, 0);

		postProcessNumber++;
	}
}

void VingettePostProcessPass()
{
	if (postProcessingEffectsSettings.vignette.IsEnabled())
	{
		glm::uvec2 buffers = GetPostProcessFramebuffers();

		glBindFramebuffer(GL_FRAMEBUFFER, buffers.y);
		glDisable(GL_DEPTH_TEST); // disable depth test so screen-space quad isn't discarded due to depth test.
		glClearColor(1.0f, 1.0f, 1.0f, 1.0f); // set clear color to white (not really necessery actually, since we won't be able to see behind the quad anyways)
		glClear(GL_COLOR_BUFFER_BIT);

		vignettePostProcessShader.UseShader();
		glBindVertexArray(screenQuadVAO);

		vignettePostProcessShader.BindSampler("s_screenTexture", 0, buffers.x); // use the color attachment texture as the texture of the quad plane
		vignettePostProcessShader.SetUniform("u_resolution", glm::vec2(mainWindow.GetWindowWidth(), mainWindow.GetWindowHeight()));
		vignettePostProcessShader.SetUniform("u_radius", postProcessingEffectsSettings.vignette.GetRadius());
		vignettePostProcessShader.SetUniform("u_softness", postProcessingEffectsSettings.vignette.GetSoftness());
		vignettePostProcessShader.SetUniform("u_intensity", postProcessingEffectsSettings.vignette.GetIntensity());
		
		edgeDetectionPostProcessShader.Validate();
		glDrawArrays(GL_TRIANGLES, 0, 6);

		glBindFramebuffer(GL_FRAMEBUFFER, 0);

		postProcessNumber++;
	}
}

void LogoOverlayPostProcessPass()
{
	if (postProcessingEffectsSettings.logoOverlay.IsEnabled())
	{
		glm::uvec2 buffers = GetPostProcessFramebuffers();

		glBindFramebuffer(GL_FRAMEBUFFER, buffers.y);
		glDisable(GL_DEPTH_TEST); // disable depth test so screen-space quad isn't discarded due to depth test.
		glClearColor(1.0f, 1.0f, 1.0f, 1.0f); // set clear color to white (not really necessery actually, since we won't be able to see behind the quad anyways)
		glClear(GL_COLOR_BUFFER_BIT);

		logoOverlayPostProcessShader.UseShader();
		glBindVertexArray(screenQuadVAO);

		logoOverlayPostProcessShader.BindSampler("s_screenTexture", 0, buffers.x);
		logoOverlayPostProcessShader.BindSampler("s_logoOverlayTexture", 1, logoOverlayTexture.GetTextureID());
		logoOverlayPostProcessShader.SetUniform("u_logoOverlayTint", glm::vec4(0.2f, 0.2f, 0.2f, 0.0f));
		logoOverlayPostProcessShader.SetUniform("u_framebufferDimensions", mainWindow.GetFramebufferWidth(), mainWindow.GetFramebufferHeight());
		logoOverlayPostProcessShader.SetUniform("u_logoOverlayTextureDimensions", logoOverlayTexture.GetWidth(), logoOverlayTexture.GetHeight());

		logoOverlayPostProcessShader.Validate();
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
	dullMaterial = Material(0.3f, 4);

	CreateLights();
	CreateSkybox();

	camera.CalculateProjectionMatrix(60.0f, (GLfloat)WINDOW_SIZE_WIDTH_START, (GLfloat)WINDOW_SIZE_HEIGHT_START, 0.1f, 200.0f); 

	postProcessingEffectsSettings = PostProcessingEffectsSettings();
	//postProcessingEffectsSettings.invert.Enable();
	postProcessingEffectsSettings.vignette.Enable();
	//postProcessingEffectsSettings.edgeDetection.Enable();
	//postProcessingEffectsSettings.edgeDetection.SetOffsetValue(2);
	postProcessingEffectsSettings.logoOverlay.Enable();

	terrainController = TerrainController(&terrainShader, &camera);
	GLuint Terrain1LODRange[8] = {30, 15, 5, 2, 0, 0, 0, 0};
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
		DirectionalShadowMapPass(&directionalLight); //Shadow pass for directional shadows (once because we have only one light)
		for (size_t i = 0; i < pointLightCount; i++) { OmniShadowMapPass(&pointLights[i]); } //Shadow pass for omni-directional point light shadows (as many times as we have point lights)
		for (size_t i = 0; i < spotLightCount; i++) { OmniShadowMapPass(&spotLights[i]); } //Shadow pass for omni-directional spot light shadows (as many times as we have spot lights)

		camera.CalculateViewMatrix(); //If camera changes its position this needs to be calculated each frame
		RenderPass(camera.GetProjectionMatrix(), camera.GetViewMatrix());
		if (postProcessingEffectsSettings.IsEnabled())
		{
			InvertPostProcessPass();
			GrayscalePostProcessPass();
			EdgeDetectionPostProcessPass();
			VingettePostProcessPass();
			LogoOverlayPostProcessPass();
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


