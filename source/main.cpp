#include "../glew/include/GL/glew.h"

#include "Renderer/Renderer.h"
#include "Renderer/camera.h"
#include "models/VoxelCharacter.h"
#include "utils/Interpolator.h"

#include <windows.h>
#include <gl/gl.h>
#include <gl/glu.h>

#pragma comment (lib, "opengl32")
#pragma comment (lib, "glu32")

#include <stdio.h>
#include <stdlib.h>

#include <GLFW/glfw3.h>

#include "VoxGame.h"
#include "input.h"


bool modelWireframe = false;
bool modelTalking = false;
int modelAnimationIndex = 0;
VoxelCharacter* pVoxelCharacter = NULL;
bool multiSampling = true;
int weaponIndex = 0;
string weaponString = "NONE";

int main(void)
{
	VoxGame* m_pVoxGame = new VoxGame();

	GLFWwindow* window;

	/* Initialize the library */
	if (!glfwInit())
	{
		exit(EXIT_FAILURE);
	}

	/* Initialize any rendering params */
	int samples = 8;
	glfwWindowHint(GLFW_SAMPLES, samples);
	glfwWindowHint(GLFW_VISIBLE, GL_FALSE);
	glGetIntegerv(GL_SAMPLES_ARB, &samples);

	/* Create a windowed mode window and it's OpenGL context */
	int windowWidth = 800;
	int windowHeight = 800;
	window = glfwCreateWindow(windowWidth, windowHeight, "Vox", NULL, NULL);
	if (!window)
	{
		glfwTerminate();
		exit(EXIT_FAILURE);
	}

	/* Input callbacks */
	glfwSetKeyCallback(window, key_callback);
	glfwSetMouseButtonCallback(window, mouse_button_callback);

	/* Center on screen */
	int width;
	int height;
	const GLFWvidmode* vidmode = glfwGetVideoMode(glfwGetPrimaryMonitor());
	glfwGetWindowSize(window, &width, &height);
	glfwSetWindowPos(window, (vidmode->width - width) / 2, (vidmode->height - height) / 2);
	
	/* Make the window's context current */
	glfwMakeContextCurrent(window);
	glfwSwapInterval(0); // Disable v-sync

	/* Show the window */
	glfwShowWindow(window);

	/* Create the renderer */
	Renderer* pRenderer = new Renderer(windowWidth, windowHeight, 32, 8);

	/* Create cameras */
	Camera* pGameCamera = new Camera(pRenderer);
	pGameCamera->SetPosition(Vector3d(0.0f, 1.25f, 3.0f));
	pGameCamera->SetFacing(Vector3d(0.0f, 0.0f, -1.0f));
	pGameCamera->SetUp(Vector3d(0.0f, 1.0f, 0.0f));
	pGameCamera->SetRight(Vector3d(1.0f, 0.0f, 0.0f));

	/* Create viewports */
	unsigned int defaultViewport;
	pRenderer->CreateViewport(0, 0, windowWidth, windowHeight, 60.0f, &defaultViewport);

	/* Create fonts */
	unsigned int defaultFont;
	pRenderer->CreateFreeTypeFont("media/fonts/arial.ttf", 12, &defaultFont);

	/* Setup the FPS counters */
	LARGE_INTEGER fps_previousTicks;
	LARGE_INTEGER fps_ticksPerSecond;
	QueryPerformanceCounter(&fps_previousTicks);
	QueryPerformanceFrequency(&fps_ticksPerSecond);

	/* Create the qubicle binary file manager */
	QubicleBinaryManager* pQubicleBinaryManager = new QubicleBinaryManager(pRenderer);

	/* Create test voxel character */
	pVoxelCharacter = new VoxelCharacter(pRenderer, pQubicleBinaryManager);
	char characterBaseFolder[128];
	char qbFilename[128];
	char ms3dFilename[128];
	char animListFilename[128];
	char facesFilename[128];
	char characterFilename[128];
	string modelName = "Steve";
	string typeName = "Human";
	sprintf_s(characterBaseFolder, 128, "media/gamedata/models");
	sprintf_s(qbFilename, 128, "media/gamedata/models/%s/%s.qb", typeName.c_str(), modelName.c_str());
	sprintf_s(ms3dFilename, 128, "media/gamedata/models/%s/%s.ms3d", typeName.c_str(), typeName.c_str());
	sprintf_s(animListFilename, 128, "media/gamedata/models/%s/%s.animlist", typeName.c_str(), typeName.c_str());
	sprintf_s(facesFilename, 128, "media/gamedata/models/%s/%s.faces", typeName.c_str(), modelName.c_str());
	sprintf_s(characterFilename, 128, "media/gamedata/models/%s/%s.character", typeName.c_str(), modelName.c_str());
	pVoxelCharacter->LoadVoxelCharacter(typeName.c_str(), qbFilename, ms3dFilename, animListFilename, facesFilename, characterFilename, characterBaseFolder);
	pVoxelCharacter->SetBreathingAnimationEnabled(true);
	pVoxelCharacter->SetWinkAnimationEnabled(true);
	pVoxelCharacter->SetTalkingAnimationEnabled(false);
	pVoxelCharacter->SetRandomMouthSelection(true);
	pVoxelCharacter->SetRandomLookDirection(true);
	pVoxelCharacter->SetWireFrameRender(false);
	pVoxelCharacter->SetCharacterScale(0.08f);

	/* Loop until the user closes the window */
	while (!glfwWindowShouldClose(window))
	{
		// Delta time
		double timeNow = (double)timeGetTime() / 1000.0;
		static double timeOld = timeNow - (1.0 / 50.0);
		float deltaTime = (float)timeNow - (float)timeOld;
		timeOld = timeNow;

		// FPS
		LARGE_INTEGER fps_currentTicks;
		QueryPerformanceCounter(&fps_currentTicks);
		float fps = 1.0f / ((float)(fps_currentTicks.QuadPart - fps_previousTicks.QuadPart) / (float)fps_ticksPerSecond.QuadPart);
		fps_previousTicks = fps_currentTicks;

		// Update interpolator singleton
		Interpolator::GetInstance()->Update();

		// Update the voxel model
		float animationSpeeds[AnimationSections_NUMSECTIONS] = { 1.0f, 1.0f, 1.0f, 1.0f, 1.0f };
		Matrix4x4 worldMatrix;
		pVoxelCharacter->Update(deltaTime, animationSpeeds);
		pVoxelCharacter->UpdateWeaponTrails(deltaTime, worldMatrix);

		// Begin rendering
		pRenderer->BeginScene(true, true, true);

		// ---------------------------------------
		// Render 3d
		// ---------------------------------------
		pRenderer->PushMatrix();
			// Set the default projection mode
			pRenderer->SetProjectionMode(PM_PERSPECTIVE, defaultViewport);

			// Set the lookat camera
			pGameCamera->Look();

			if (multiSampling)
			{
				pRenderer->EnableMultiSampling();
			}
			else
			{
				pRenderer->DisableMultiSampling();
			}

			// Render the voxel character
			Colour OulineColour(1.0f, 1.0f, 0.0f, 1.0f);
			pRenderer->PushMatrix();
				pRenderer->MultiplyWorldMatrix(worldMatrix);

				pVoxelCharacter->RenderWeapons(false, false, false, OulineColour);
				pVoxelCharacter->Render(false, false, false, OulineColour, false);
			pRenderer->PopMatrix();

			// Render the voxel character Face
			pRenderer->PushMatrix();
			pRenderer->MultiplyWorldMatrix(worldMatrix);
				glActiveTextureARB(GL_TEXTURE0_ARB);
				glDisable(GL_TEXTURE_2D);
				glBindTexture(GL_TEXTURE_2D, 0);

				pVoxelCharacter->RenderFace();
			pRenderer->PopMatrix();

		pRenderer->PopMatrix();

		// ---------------------------------------
		// Render 2d
		// ---------------------------------------
		char lFPSBuff[128];
		sprintf_s(lFPSBuff, "FPS: %.0f  Delta: %.4f", fps, deltaTime);
		char lAnimationBuff[128];
		sprintf_s(lAnimationBuff, "Animation [%i/%i]: %s", modelAnimationIndex, pVoxelCharacter->GetNumAnimations()-1, pVoxelCharacter->GetAnimationName(modelAnimationIndex));
		char lWeaponBuff[128];
		sprintf_s(lWeaponBuff, "Weapon: %s", weaponString.c_str());

		pRenderer->PushMatrix();
			glActiveTextureARB(GL_TEXTURE0_ARB);
			glDisable(GL_TEXTURE_2D);
			glBindTexture(GL_TEXTURE_2D, 0);

			pRenderer->SetRenderMode(RM_SOLID);
			pRenderer->SetProjectionMode(PM_2D, defaultViewport);
			pRenderer->SetLookAtCamera(Vector3d(0.0f, 0.0f, 50.0f), Vector3d(0.0f, 0.0f, 0.0f), Vector3d(0.0f, 1.0f, 0.0f));

			pRenderer->RenderFreeTypeText(defaultFont, 15.0f, 15.0f, 1.0f, Colour(1.0f, 1.0f, 1.0f), 1.0f, lFPSBuff);

			pRenderer->RenderFreeTypeText(defaultFont, 335.0f, 35.0f, 1.0f, Colour(1.0f, 1.0f, 1.0f), 1.0f, lAnimationBuff);
			pRenderer->RenderFreeTypeText(defaultFont, 335.0f, 15.0f, 1.0f, Colour(1.0f, 1.0f, 1.0f), 1.0f, lWeaponBuff);

			pRenderer->RenderFreeTypeText(defaultFont, 635.0f, 95.0f, 1.0f, Colour(1.0f, 1.0f, 1.0f), 1.0f, "R - Toggle MSAA");
			pRenderer->RenderFreeTypeText(defaultFont, 635.0f, 75.0f, 1.0f, Colour(1.0f, 1.0f, 1.0f), 1.0f, "E - Toggle Talking");
			pRenderer->RenderFreeTypeText(defaultFont, 635.0f, 55.0f, 1.0f, Colour(1.0f, 1.0f, 1.0f), 1.0f, "W - Toggle Wireframe");
			pRenderer->RenderFreeTypeText(defaultFont, 635.0f, 35.0f, 1.0f, Colour(1.0f, 1.0f, 1.0f), 1.0f, "Q - Cycle Animations");
			pRenderer->RenderFreeTypeText(defaultFont, 635.0f, 15.0f, 1.0f, Colour(1.0f, 1.0f, 1.0f), 1.0f, "A - Cycle Weapons");
		pRenderer->PopMatrix();

		// End rendering
		pRenderer->EndScene();

		/* Swap front and back buffers */
		glfwSwapBuffers(window);

		/* Poll for and process events */
		glfwPollEvents();
	}

	glfwTerminate();

	/* Cleanup */
	delete m_pVoxGame;

	exit(EXIT_SUCCESS);
}