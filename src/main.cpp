/*
CPE/CSC 471 Lab base code Wood/Dunn/Eckhardt
Modified by: <Zhisong Liang>
*/

#include <iostream>
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <ctime>
#include <fstream>
#include <limits>
#include <sstream>
#include <vector>
#include "GLCompat.h"
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include "GLSL.h"
#include "Program.h"
#include "MatrixStack.h"
#include "Pokemon.cpp"
#include "PlayerController.h"

#include "WindowManager.h"
#include "Shape.h"
// value_ptr for glm
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_transform.hpp>
#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif
using namespace std;
using namespace glm;
shared_ptr<Shape> shape;
shared_ptr<Shape> umbreon;
shared_ptr<Shape> charizard;
std::vector<std::shared_ptr<Shape>> companionShapes;

constexpr int NUM_POKEMON = 100;
constexpr int FLYING_POKEMON = NUM_POKEMON / 5;
constexpr int STARTING_POKEBALLS = 10;
constexpr int CAPTURE_GOAL = 5;
vec3 mypos;
Pokemon umbreons[NUM_POKEMON];
Pokemon charizards[FLYING_POKEMON];

double get_last_elapsed_time()
{
	static double lasttime = glfwGetTime();
	double actualtime = glfwGetTime();
	double difference = actualtime - lasttime;
	lasttime = actualtime;
	return difference;
}
class camera
{
public:
	glm::vec3 pos = glm::vec3(0.0f);
	int w = 0;
	int a = 0;
	int s = 0;
	int d = 0;
	int q = 0;
	int e = 0;
	int space = 0;

	camera()
	{
		reset();
	}

	glm::mat4 process(double ftime)
	{
		PlayerInput input;
		input.forward = static_cast<float>(w - s);
		input.turn = static_cast<float>(d - a);
		input.vertical = static_cast<float>(((q == 1 || space == 1) ? 1 : 0) - e);
		motionEvents_ = controller_.update(input, static_cast<float>(ftime));

		mypos = controller_.position();
		pos = -mypos;
		glm::mat4 R = glm::rotate(glm::mat4(1.0f), controller_.yaw(), glm::vec3(0, 1, 0));
		glm::mat4 T = glm::translate(glm::mat4(1), pos + glm::vec3(0, -5, 0));
		return R * T;
	}

	void reset()
	{
		w = a = s = d = q = e = space = 0;
		controller_.reset();
		motionEvents_ = PlayerMotionEvents();
		mypos = controller_.position();
		pos = -mypos;
	}

	bool toggleGravity()
	{
		controller_.toggleGravity();
		return controller_.gravityEnabled();
	}

	bool gravityEnabled() const
	{
		return controller_.gravityEnabled();
	}

	bool grounded() const
	{
		return controller_.grounded();
	}

	const PlayerMotionEvents &motionEvents() const
	{
		return motionEvents_;
	}

private:
	PlayerController controller_;
	PlayerMotionEvents motionEvents_;
};

camera mycam;

class Application : public EventCallbacks
{

public:
	WindowManager *windowManager = nullptr;

	// Our shader program
	std::shared_ptr<Program> prog, prog2, heightshader, pokemon, pokemon2;

	// Contains vertex information for OpenGL
	GLuint VertexArrayID, VertexArrayID2;

	GLuint VertexBufferIDScreen, VertexNormalIDBox, VertexTexIDBox, VertexBufferTexScreen;
	GLuint VertexBufferID2, VertexNormDBox2, VertexTexBox2, IndexBufferIDBox2, InstanceBuffer;

	// Data necessary to give our box to OpenGL
	GLuint MeshPosID, MeshTexID, IndexBufferIDBox;

	// texture data
	GLuint Texture, grassTexture, HeightTex, PokeballTex, fireTex, Texture5;
	GLuint grayTex;
	std::string resourceDirectory;
	int caughtCount = 0;
	int pokeballs = STARTING_POKEBALLS;
	bool captureRequested = false;
	bool resetRequested = false;
	bool gameFinished = false;
	std::string statusMessage = "Explore the field and find a Pokemon.";

	void updateWindowTitle()
	{
		if (!windowManager || !windowManager->getHandle())
		{
			return;
		}

		std::ostringstream title;
		title << "Pokemon World | W/S move  A/D turn  Q/E/Space fly  Z toggle gravity  C catch  R reset"
		      << " | Caught " << caughtCount << "/" << CAPTURE_GOAL
		      << " | Poke Balls " << pokeballs
		      << " | " << (mycam.gravityEnabled() ? "Gravity ON" : "Hover mode")
		      << " | " << (mycam.grounded() ? "Grounded" : "Airborne");
		if (!statusMessage.empty())
		{
			title << " | " << statusMessage;
		}
		glfwSetWindowTitle(windowManager->getHandle(), title.str().c_str());
	}

	void setStatus(const std::string &message)
	{
		statusMessage = message;
		updateWindowTitle();
#ifdef __EMSCRIPTEN__
		EM_ASM({
			if (Module.onGameStatus)
			{
				Module.onGameStatus(UTF8ToString($0));
			}
		}, message.c_str());
#endif
	}

	void resetGame()
	{
		for (int i = 0; i < NUM_POKEMON; ++i)
		{
			umbreons[i] = Pokemon(0, i);
		}
		for (int i = 0; i < FLYING_POKEMON; ++i)
		{
			charizards[i] = Pokemon(1, i);
		}

		mycam.reset();
		caughtCount = 0;
		pokeballs = STARTING_POKEBALLS;
		captureRequested = false;
		resetRequested = false;
		gameFinished = false;
		setStatus("Explore the field and find a Pokemon.");
	}

	void captureNearestPokemon()
	{
		if (gameFinished)
		{
			return;
		}
		if (pokeballs <= 0)
		{
			gameFinished = true;
			setStatus("Out of Poke Balls. Press R to try again.");
			return;
		}

		Pokemon *target = nullptr;
		float targetDistance = std::numeric_limits<float>::max();
		auto consider = [&](Pokemon &candidate, float captureRange) {
			if (candidate.getCaught())
			{
				return;
			}
			float distance = glm::distance(mypos, candidate.getPos());
			if (distance <= captureRange && distance < targetDistance)
			{
				target = &candidate;
				targetDistance = distance;
			}
		};

		for (int i = 0; i < NUM_POKEMON; ++i)
		{
			consider(umbreons[i], 5.0f);
		}
		for (int i = 0; i < FLYING_POKEMON; ++i)
		{
			consider(charizards[i], 12.0f);
		}

		if (!target)
		{
			setStatus("No Pokemon in range. Move closer and press C.");
			return;
		}

		target->setCaught(1);
		--pokeballs;
		++caughtCount;
		if (caughtCount >= CAPTURE_GOAL)
		{
			gameFinished = true;
			setStatus("Research complete! Press R to play again.");
		}
		else if (pokeballs == 0)
		{
			gameFinished = true;
			setStatus("Out of Poke Balls before the goal. Press R to retry.");
		}
		else
		{
			setStatus("Captured a Pokemon!");
		}
	}

	void keyCallback(GLFWwindow *window, int key, int scancode, int action, int mods)
	{
		if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
		{
			glfwSetWindowShouldClose(window, GL_TRUE);
		}

		if (key == GLFW_KEY_W && action == GLFW_PRESS)
		{
			mycam.w = 1;
		}
		if (key == GLFW_KEY_W && action == GLFW_RELEASE)
		{
			mycam.w = 0;
		}
		if (key == GLFW_KEY_S && action == GLFW_PRESS)
		{
			mycam.s = 1;
		}
		if (key == GLFW_KEY_S && action == GLFW_RELEASE)
		{
			mycam.s = 0;
		}
		if (key == GLFW_KEY_A && action == GLFW_PRESS)
		{
			mycam.a = 1;
		}
		if (key == GLFW_KEY_A && action == GLFW_RELEASE)
		{
			mycam.a = 0;
		}
		if (key == GLFW_KEY_D && action == GLFW_PRESS)
		{
			mycam.d = 1;
		}
		if (key == GLFW_KEY_D && action == GLFW_RELEASE)
		{
			mycam.d = 0;
		}
		if (key == GLFW_KEY_Q && action == GLFW_PRESS)
		{
			mycam.q = 1;
		}
		if (key == GLFW_KEY_Q && action == GLFW_RELEASE)
		{
			mycam.q = 0;
		}
		if (key == GLFW_KEY_E && action == GLFW_PRESS)
		{
			mycam.e = 1;
		}
		if (key == GLFW_KEY_E && action == GLFW_RELEASE)
		{
			mycam.e = 0;
		}
		if (key == GLFW_KEY_Z && action == GLFW_PRESS)
		{
			bool gravityEnabled = mycam.toggleGravity();
			setStatus(gravityEnabled ? "Gravity enabled: release lift to descend."
			                         : "Gravity disabled: hover mode active.");
		}
		if (key == GLFW_KEY_SPACE && action == GLFW_PRESS)
		{
			mycam.space = 1;
		}
		if (key == GLFW_KEY_SPACE && action == GLFW_RELEASE)
		{
			mycam.space = 0;
		}
		if (key == GLFW_KEY_C && action == GLFW_PRESS)
		{
			captureRequested = true;
		}
		if (key == GLFW_KEY_R && action == GLFW_PRESS)
		{
			resetRequested = true;
		}
	}

	// callback for the mouse when clicked move the triangle when helper functions
	// written
	void mouseCallback(GLFWwindow *window, int button, int action, int mods)
	{
		double posX, posY;
		float newPt[2];
		if (action == GLFW_PRESS)
		{
			glfwGetCursorPos(window, &posX, &posY);
			std::cout << "Pos X " << posX << " Pos Y " << posY << std::endl;

			// // change this to be the points converted to WORLD
			// // THIS IS BROKEN< YOU GET TO FIX IT - yay!
			// newPt[0] = 0;
			// newPt[1] = 0;

			// std::cout << "converted:" << newPt[0] << " " << newPt[1] << std::endl;
			// glBindBuffer(GL_ARRAY_BUFFER, MeshPosID);
			// // update the vertex array with the updated points
			// glBufferSubData(GL_ARRAY_BUFFER, sizeof(float) * 6, sizeof(float) * 2, newPt);
			// glBindBuffer(GL_ARRAY_BUFFER, 0);
		}
	}

	// if the window is resized, capture the new size and reset the viewport
	void resizeCallback(GLFWwindow *window, int in_width, int in_height)
	{
		// get the window size - may be different then pixels for retina
		int width, height;
		glfwGetFramebufferSize(window, &width, &height);
		glViewport(0, 0, width, height);
	}
#define MESHSIZE 100
	void init_mesh()
	{
		// generate the VAO
		glGenVertexArrays(1, &VertexArrayID);
		glBindVertexArray(VertexArrayID);

		// generate vertex buffer to hand off to OGL
		glGenBuffers(1, &MeshPosID);
		glBindBuffer(GL_ARRAY_BUFFER, MeshPosID);
		std::vector<vec3> vertices(MESHSIZE * MESHSIZE * 4);
		for (int x = 0; x < MESHSIZE; x++)
			for (int z = 0; z < MESHSIZE; z++)
			{
				vertices[x * 4 + z * MESHSIZE * 4 + 0] = vec3(0.0, 0.0, 0.0) + vec3(x, 0, z);
				vertices[x * 4 + z * MESHSIZE * 4 + 1] = vec3(1.0, 0.0, 0.0) + vec3(x, 0, z);
				vertices[x * 4 + z * MESHSIZE * 4 + 2] = vec3(1.0, 0.0, 1.0) + vec3(x, 0, z);
				vertices[x * 4 + z * MESHSIZE * 4 + 3] = vec3(0.0, 0.0, 1.0) + vec3(x, 0, z);
			}
		glBufferData(GL_ARRAY_BUFFER, sizeof(vec3) * MESHSIZE * MESHSIZE * 4, vertices.data(), GL_DYNAMIC_DRAW);
		glEnableVertexAttribArray(0);
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, (void *)0);
		// tex coords
		float t = 1. / 100;
		std::vector<vec2> tex(MESHSIZE * MESHSIZE * 4);
		for (int x = 0; x < MESHSIZE; x++)
			for (int y = 0; y < MESHSIZE; y++)
			{
				tex[x * 4 + y * MESHSIZE * 4 + 0] = vec2(0.0, 0.0) + vec2(x, y) * t;
				tex[x * 4 + y * MESHSIZE * 4 + 1] = vec2(t, 0.0) + vec2(x, y) * t;
				tex[x * 4 + y * MESHSIZE * 4 + 2] = vec2(t, t) + vec2(x, y) * t;
				tex[x * 4 + y * MESHSIZE * 4 + 3] = vec2(0.0, t) + vec2(x, y) * t;
			}
		glGenBuffers(1, &MeshTexID);
		// set the current state to focus on our vertex buffer
		glBindBuffer(GL_ARRAY_BUFFER, MeshTexID);
		glBufferData(GL_ARRAY_BUFFER, sizeof(vec2) * MESHSIZE * MESHSIZE * 4, tex.data(), GL_STATIC_DRAW);
		glEnableVertexAttribArray(1);
		glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 0, (void *)0);

		glGenBuffers(1, &IndexBufferIDBox);
		// set the current state to focus on our vertex buffer
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, IndexBufferIDBox);
		std::vector<GLushort> elements(MESHSIZE * MESHSIZE * 6);
		int ind = 0;
		for (int i = 0; i < MESHSIZE * MESHSIZE * 6; i += 6, ind += 4)
		{
			elements[i + 0] = ind + 0;
			elements[i + 1] = ind + 1;
			elements[i + 2] = ind + 2;
			elements[i + 3] = ind + 0;
			elements[i + 4] = ind + 2;
			elements[i + 5] = ind + 3;
		}
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(GLushort) * MESHSIZE * MESHSIZE * 6, elements.data(), GL_STATIC_DRAW);
		glBindVertexArray(0);
	}
	/*Note that any gl calls must always happen after a GL state is initialized */
	void initGeom()
	{
		// initialize the net mesh
		init_mesh();

		for (int i = 0; i < NUM_POKEMON; i++)
		{
			umbreons[i] = Pokemon(0, i);
			// // initialize the unmbreon
			// unmbreonPos[i].x = -rand() / (float)RAND_MAX * 100;
			// unmbreonPos[i].y = 0;
			// unmbreonPos[i].z = -rand() / (float)RAND_MAX * 100;
		}

		for (int i = 0; i < FLYING_POKEMON; i++)
		{
			charizards[i] = Pokemon(1, i);
			// initialize the pokemon
			// charizaPos[i].x = -rand() / (float)RAND_MAX * 100;
			// charizaPos[i].y = rand() / (float)RAND_MAX * 50 + 20;
			// charizaPos[i].z = -rand() / (float)RAND_MAX * 100;
		}

		// generate the VAO
		glGenVertexArrays(1, &VertexArrayID2);
		glBindVertexArray(VertexArrayID2);

		// generate vertex buffer to hand off to OGL
		glGenBuffers(1, &VertexBufferID2);
		// set the current state to focus on our vertex buffer
		glBindBuffer(GL_ARRAY_BUFFER, VertexBufferID2);

		GLfloat cube_vertices[] = {
			// front
			-1.0, -1.0, 1.0, // LD
			1.0, -1.0, 1.0,	 // RD
			1.0, 1.0, 1.0,	 // RU
			-1.0, 1.0, 1.0,	 // LU
		};
		// make it a bit smaller
		for (int i = 0; i < 12; i++)
			cube_vertices[i] *= 0.5;
		// actually memcopy the data - only do this once
		glBufferData(GL_ARRAY_BUFFER, sizeof(cube_vertices), cube_vertices, GL_DYNAMIC_DRAW);

		// we need to set up the vertex array
		glEnableVertexAttribArray(0);
		// key function to get up how many elements to pull out at a time (3)
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, (void *)0);

		// color
		GLfloat cube_norm[] = {
			// front colors
			0.0,
			0.0,
			1.0,
			0.0,
			0.0,
			1.0,
			0.0,
			0.0,
			1.0,
			0.0,
			0.0,
			1.0,

		};
		glGenBuffers(1, &VertexNormDBox2);
		// set the current state to focus on our vertex buffer
		glBindBuffer(GL_ARRAY_BUFFER, VertexNormDBox2);
		glBufferData(GL_ARRAY_BUFFER, sizeof(cube_norm), cube_norm, GL_STATIC_DRAW);
		glEnableVertexAttribArray(1);
		glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 0, (void *)0);

		// color
		glm::vec2 cube_tex[] = {
			// front colors
			glm::vec2(0.0, 1.0),
			glm::vec2(1.0, 1.0),
			glm::vec2(1.0, 0.0),
			glm::vec2(0.0, 0.0),

		};
		glGenBuffers(1, &VertexTexBox2);
		// set the current state to focus on our vertex buffer
		glBindBuffer(GL_ARRAY_BUFFER, VertexTexBox2);
		glBufferData(GL_ARRAY_BUFFER, sizeof(cube_tex), cube_tex, GL_STATIC_DRAW);
		glEnableVertexAttribArray(2);
		glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 0, (void *)0);

		glGenBuffers(1, &IndexBufferIDBox2);
		// set the current state to focus on our vertex buffer
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, IndexBufferIDBox2);
		GLushort cube_elements[] = {

			// front
			0,
			1,
			2,
			2,
			3,
			0,
		};
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(cube_elements), cube_elements, GL_STATIC_DRAW);

		glBindVertexArray(0);

		// Initialize mesh.
		shape = make_shared<Shape>();
		shape->loadMesh(resourceDirectory + "/sphere.obj");
		shape->resize();
		shape->init();

		umbreon = make_shared<Shape>();
		umbreon->loadMesh(resourceDirectory + "/pokemon/umbreon.obj");
		umbreon->resize();
		umbreon->init();

		string str1 = resourceDirectory + "/pokemon";
		charizard = make_shared<Shape>();
		charizard->loadMesh(resourceDirectory + "/pokemon/charizard.obj", &str1);
		charizard->resize();
		charizard->init();

		const char *companionNames[] = {"animal-bunny", "animal-cat", "animal-chick", "animal-parrot"};
		string companionDirectory = resourceDirectory + "/pokemon/companions/";
		for (const char *companionName : companionNames)
		{
			shared_ptr<Shape> companion = make_shared<Shape>();
			companion->loadMesh(companionDirectory + companionName + ".obj",
			                    &companionDirectory, stbi_load);
			companion->resize();
			companion->init();
			companionShapes.push_back(companion);
		}

		int width, height, channels;
		char filepath[1000];

		// texture 1
		string str = resourceDirectory + "/Texture/storm.jpg";
		strcpy(filepath, str.c_str());
		unsigned char *data = stbi_load(filepath, &width, &height, &channels, 4);
		glGenTextures(1, &Texture);
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, Texture);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
		// generate mipmap, this will automatically generate all the required mipmaps for the currently bound texture object.
		glGenerateMipmap(GL_TEXTURE_2D);

		// texture 2
		str = resourceDirectory + "/Texture/grass.jpg";
		strcpy(filepath, str.c_str());
		data = stbi_load(filepath, &width, &height, &channels, 4);
		glGenTextures(1, &grassTexture);
		glActiveTexture(GL_TEXTURE1);
		glBindTexture(GL_TEXTURE_2D, grassTexture);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
		glGenerateMipmap(GL_TEXTURE_2D);

		// texture 3
		str = resourceDirectory + "/height.png";
		strcpy(filepath, str.c_str());
		data = stbi_load(filepath, &width, &height, &channels, 4);
		glGenTextures(1, &HeightTex);
		glActiveTexture(GL_TEXTURE1);
		glBindTexture(GL_TEXTURE_2D, HeightTex);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
		glGenerateMipmap(GL_TEXTURE_2D);

		// texture 4
		str = resourceDirectory + "/Texture/pokeball.jpg";
		strcpy(filepath, str.c_str());
		data = stbi_load(filepath, &width, &height, &channels, 4);
		glGenTextures(1, &PokeballTex);
		glActiveTexture(GL_TEXTURE1);
		glBindTexture(GL_TEXTURE_2D, PokeballTex);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
		glGenerateMipmap(GL_TEXTURE_2D);

		// texture 5: the Charizard UV atlas that ships with the original model.
		str = resourceDirectory + "/Texture/chariza.png";
		strcpy(filepath, str.c_str());
		data = stbi_load(filepath, &width, &height, &channels, 4);
		glGenTextures(1, &fireTex);
		glActiveTexture(GL_TEXTURE1);
		glBindTexture(GL_TEXTURE_2D, fireTex);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
		glGenerateMipmap(GL_TEXTURE_2D);

		// texture 6
		str = resourceDirectory + "/Texture/thunder.jpg";
		strcpy(filepath, str.c_str());
		data = stbi_load(filepath, &width, &height, &channels, 4);
		glGenTextures(1, &Texture5);
		glActiveTexture(GL_TEXTURE1);
		glBindTexture(GL_TEXTURE_2D, Texture5);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
		glGenerateMipmap(GL_TEXTURE_2D);

		// texture 7
		str = resourceDirectory + "/Texture/clouds.jpg";
		strcpy(filepath, str.c_str());
		data = stbi_load(filepath, &width, &height, &channels, 4);
		glGenTextures(1, &grayTex);
		glActiveTexture(GL_TEXTURE1);
		glBindTexture(GL_TEXTURE_2D, grayTex);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
		glGenerateMipmap(GL_TEXTURE_2D);

		//[TWOTEXTURES]
		// set the 2 textures to the correct samplers in the fragment shader:
		GLuint Tex1Location = glGetUniformLocation(prog->pid, "tex"); // tex, tex2... sampler in the fragment shader
		GLuint Tex2Location = glGetUniformLocation(prog->pid, "tex2");
		// Then bind the uniform samplers to texture units:
		glUseProgram(prog->pid);
		glUniform1i(Tex1Location, 0);
		glUniform1i(Tex2Location, 1);

		Tex1Location = glGetUniformLocation(heightshader->pid, "tex"); // tex, tex2... sampler in the fragment shader
		Tex2Location = glGetUniformLocation(heightshader->pid, "tex2");
		// Then bind the uniform samplers to texture units:
		glUseProgram(heightshader->pid);
		glUniform1i(Tex1Location, 0);
		glUniform1i(Tex2Location, 1);

		Tex1Location = glGetUniformLocation(pokemon->pid, "tex"); // tex, tex2... sampler in the fragment shader
		Tex2Location = glGetUniformLocation(pokemon->pid, "tex2");
		// Then bind the uniform samplers to texture units:
		glUseProgram(pokemon->pid);
		glUniform1i(Tex1Location, 0);
		glUniform1i(Tex2Location, 1);

		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		updateWindowTitle();
	}

	// General OGL initialization - set OGL state here
	void init(const std::string &resourceDirectory)
	{
		this->resourceDirectory = resourceDirectory;
		GLSL::checkVersion();

		// Set background color.
		glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
		// Enable z-buffer test.
		glEnable(GL_DEPTH_TEST);

		// Initialize the GLSL program.
		prog = std::make_shared<Program>();
		prog->setVerbose(true);
		prog->setShaderNames(resourceDirectory + "/shader_vertex.glsl", resourceDirectory + "/shader_fragment.glsl");
		if (!prog->init())
		{
			std::cerr << "One or more shaders failed to compile... exiting!" << std::endl;
			exit(1);
		}
		prog->addUniform("P");
		prog->addUniform("V");
		prog->addUniform("M");
		prog->addUniform("campos");
		prog->addAttribute("vertPos");
		prog->addAttribute("vertNor");
		prog->addAttribute("vertTex");

		prog2 = std::make_shared<Program>();
		prog2->setVerbose(true);
		prog2->setShaderNames(resourceDirectory + "/shader_vertex2.glsl", resourceDirectory + "/shader_fragment2.glsl");
		if (!prog2->init())
		{
			std::cerr << "One or more shaders failed to compile... exiting!" << std::endl;
			exit(1);
		}
		prog2->addUniform("P");
		prog2->addUniform("V");
		prog2->addUniform("M");
		prog2->addUniform("campos");
		prog2->addAttribute("vertPos");
		prog2->addAttribute("vertNor");
		prog2->addAttribute("vertTex");

		// Initialize the GLSL program.
		heightshader = std::make_shared<Program>();
		heightshader->setVerbose(true);
		heightshader->setShaderNames(resourceDirectory + "/height_vertex.glsl", resourceDirectory + "/height_frag.glsl");
		if (!heightshader->init())
		{
			std::cerr << "One or more shaders failed to compile... exiting!" << std::endl;
			exit(1);
		}
		heightshader->addUniform("P");
		heightshader->addUniform("V");
		heightshader->addUniform("M");
		heightshader->addUniform("camoff");
		heightshader->addUniform("campos");
		heightshader->addAttribute("vertPos");
		heightshader->addAttribute("vertTex");

		pokemon = std::make_shared<Program>();
		pokemon->setVerbose(true);
		pokemon->setShaderNames(resourceDirectory + "/pokemon_vertex.glsl", resourceDirectory + "/pokemon_frag.glsl");
		if (!pokemon->init())
		{
			std::cerr << "One or more shaders failed to compile... exiting!" << std::endl;
			exit(1);
		}
		pokemon->addUniform("P");
		pokemon->addUniform("V");
		pokemon->addUniform("M");
		pokemon->addUniform("camoff");
		pokemon->addUniform("campos");
		pokemon->addAttribute("vertPos");
		pokemon->addAttribute("vertTex");
		pokemon->addAttribute("vertNor");

		pokemon2 = std::make_shared<Program>();
		pokemon2->setVerbose(true);
		pokemon2->setShaderNames(resourceDirectory + "/psyduck_vertex.glsl", resourceDirectory + "/psyduck_frag.glsl");
		if (!pokemon2->init())
		{
			std::cerr << "One or more shaders failed to compile... exiting!" << std::endl;
			exit(1);
		}
		pokemon2->addUniform("P");
		pokemon2->addUniform("V");
		pokemon2->addUniform("M");
		pokemon2->addUniform("camoff");
		pokemon2->addUniform("campos");
		pokemon2->addAttribute("vertPos");
		pokemon2->addAttribute("vertTex");
		pokemon2->addAttribute("vertNor");
	}

	/****DRAW
	This is the most important function in your program - this is where you
	will actually issue the commands to draw any geometry you have set up to
	draw
	********/
	void render()
	{
		double frametime = get_last_elapsed_time();

		// Get current frame buffer size.
		int width, height;
		glfwGetFramebufferSize(windowManager->getHandle(), &width, &height);
		if (width <= 0 || height <= 0)
		{
			return;
		}
		float aspect = width / (float)height;
		glViewport(0, 0, width, height);

		// Clear framebuffer.
		glClearColor(0.8f, 0.8f, 1.0f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		// Create the matrix stacks - please leave these alone for now

		glm::mat4 V, M, P; // View, Model and Perspective matrix
		V = glm::mat4(1);
		M = glm::mat4(1);
		// Apply orthographic projection....
		P = glm::ortho(-1 * aspect, 1 * aspect, -1.0f, 1.0f, -2.0f, 100.0f);
		if (width < height)
		{
			P = glm::ortho(-1.0f, 1.0f, -1.0f / aspect, 1.0f / aspect, -2.0f, 100.0f);
		}
		// ...but we overwrite it (optional) with a perspective projection.
		P = glm::perspective((float)(3.14159 / 4.), (float)((float)width / (float)height), 0.1f, 1000.0f); // so much type casting... GLM metods are quite funny ones
		if (resetRequested)
		{
			resetGame();
		}
		glm::mat4 playerView = mycam.process(frametime);
		const PlayerMotionEvents &motionEvents = mycam.motionEvents();
		if (motionEvents.hitCeiling)
		{
			setStatus("Maximum flight altitude reached.");
		}
		else if (motionEvents.landed)
		{
			setStatus("Landed safely.");
		}
		else if (motionEvents.hitBoundary)
		{
			setStatus("Field boundary reached.");
		}
		if (captureRequested)
		{
			captureNearestPokemon();
			captureRequested = false;
		}

		// animation with the model matrix:
		static float w = 0.0;
		w += 1.0 * frametime; // rotation angle
		float trans = 0;	  // sin(t) * 2;
		glm::mat4 RotateY = glm::rotate(glm::mat4(1.0f), w, glm::vec3(0.0f, 1.0f, 0.0f));
		float angle = -3.1415926 / 2.0;
		glm::mat4 RotateX = glm::rotate(glm::mat4(1.0f), angle, glm::vec3(1.0f, 0.0f, 0.0f));
		glm::mat4 TransZ = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, -3 + trans));
		glm::mat4 S = glm::scale(glm::mat4(1.0f), glm::vec3(0.8f, 0.8f, 0.8f));
		S = glm::scale(glm::mat4(1.0f), glm::vec3(100.0f, 100.0f, 100.0f));
		M = TransZ * RotateY * RotateX * S;

		// Draw the box using GLSL.

		prog->bind();

		V = mat4(1);
		// storm effect
		glUniformMatrix4fv(prog->getUniform("P"), 1, GL_FALSE, &P[0][0]);
		glUniformMatrix4fv(prog->getUniform("V"), 1, GL_FALSE, &V[0][0]);
		glUniformMatrix4fv(prog->getUniform("M"), 1, GL_FALSE, &M[0][0]);
		glUniform3fv(prog->getUniform("campos"), 1, &mycam.pos[0]);
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, Texture);

		glDisable(GL_DEPTH_TEST);
		shape->draw(prog, false);
		glEnable(GL_DEPTH_TEST);

		prog->unbind();

		heightshader->bind();
		V = playerView;
		// glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
		glm::mat4 TransY = glm::translate(glm::mat4(1.0f), glm::vec3(-50.0f, 0.0f, -50));
		M = TransY;
		glUniformMatrix4fv(heightshader->getUniform("M"), 1, GL_FALSE, &M[0][0]);
		glUniformMatrix4fv(heightshader->getUniform("P"), 1, GL_FALSE, &P[0][0]);
		glUniformMatrix4fv(heightshader->getUniform("V"), 1, GL_FALSE, &V[0][0]);

		vec3 offset = mycam.pos;
		offset.y = 0;
		offset.x = (int)offset.x;
		offset.z = (int)offset.z;
		glUniform3fv(heightshader->getUniform("camoff"), 1, &offset[0]);
		glUniform3fv(heightshader->getUniform("campos"), 1, &mycam.pos[0]);
		glBindVertexArray(VertexArrayID);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, IndexBufferIDBox);
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, HeightTex);
		glActiveTexture(GL_TEXTURE1);
		glBindTexture(GL_TEXTURE_2D, grassTexture);
		glDrawElements(GL_TRIANGLES, MESHSIZE * MESHSIZE * 6, GL_UNSIGNED_SHORT, (void *)0);

		heightshader->unbind();

		prog2->bind();
		V = mat4(1);
		glUniformMatrix4fv(prog2->getUniform("P"), 1, GL_FALSE, &P[0][0]);
		glUniformMatrix4fv(prog2->getUniform("V"), 1, GL_FALSE, &V[0][0]);
		glUniform3fv(prog2->getUniform("campos"), 1, &mycam.pos[0]);
		glBindVertexArray(VertexArrayID2);
		// actually draw from vertex 0, 3 vertices
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, IndexBufferIDBox2);
		mat4 Vi = glm::transpose(V);
		Vi[0][3] = 0;
		Vi[1][3] = 0;
		Vi[2][3] = 0;
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, PokeballTex);

		S = glm::scale(glm::mat4(1.0f), glm::vec3(0.1f, 0.1f, 0.1f));
		// Show the remaining Poke Balls as a small in-world inventory.
		for (int i = 0; i < pokeballs; i++)
		{
			mat4 TranPokeball = glm::translate(glm::mat4(1.0f), glm::vec3(1.8f, 1.0f - i * 0.1f, 0.0f));
			M = TransZ * TranPokeball * S * Vi;
			glUniformMatrix4fv(prog2->getUniform("M"), 1, GL_FALSE, &M[0][0]);
			glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, (void *)0);
		}

		prog2->unbind();

		pokemon->bind();
		/*main character*/
		static float y = 0.0;
		static int flag = 0;
		if (flag == 0)
		{
			y += 0.001;
			if (y > 0.05)
			{
				flag = 1;
			}
		}
		else
		{
			y -= 0.001;
			if (y < -0.05)
			{
				flag = 0;
			}
		}
		V = mat4(1);
		S = glm::scale(glm::mat4(1.0f), glm::vec3(0.24f, 0.24f, 0.24f));
		// Keep the player model in a stable camera-space layer instead of
		// placing it almost on the near clipping plane.
		mat4 T = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, -0.55f + y, -2.6f));

		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, fireTex);
		glUniformMatrix4fv(pokemon->getUniform("P"), 1, GL_FALSE, &P[0][0]);
		glUniformMatrix4fv(pokemon->getUniform("V"), 1, GL_FALSE, &V[0][0]);
		glUniform3fv(pokemon->getUniform("camoff"), 1, &offset[0]);
		glUniform3fv(pokemon->getUniform("campos"), 1, &mycam.pos[0]);
		// rotate x 180 degree
		mat4 R = glm::rotate(glm::mat4(1.0f), 3.14f, glm::vec3(0.0f, 1.0f, 0.0f));
		M = T * R * S;
		glUniformMatrix4fv(pokemon->getUniform("M"), 1, GL_FALSE, &M[0][0]);
		charizard->draw(pokemon, false);
		pokemon->unbind();
		/***main character***/

		pokemon2->bind();
		glUniformMatrix4fv(pokemon2->getUniform("P"), 1, GL_FALSE, &P[0][0]);
		V = playerView;
		glUniformMatrix4fv(pokemon2->getUniform("V"), 1, GL_FALSE, &V[0][0]);
		// rotate Y 90 degree
		RotateY = glm::rotate(glm::mat4(1.0f), glm::radians(90.0f), glm::vec3(-1.0f, 0.0f, 0.0f));

		S = glm::scale(glm::mat4(1.0f), glm::vec3(0.34f, 0.34f, 0.34f));

		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, grayTex);
		float motionTime = static_cast<float>(glfwGetTime());
		// umbreon
		for (int i = 0; i < NUM_POKEMON; i++)
		{
			// distance between pokemon and camera
			float distance = sqrt(pow(mypos.x - umbreons[i].getPos().x, 2) + pow(mypos.z - umbreons[i].getPos().z, 2));
			// if flag been caught, then don't draw, if too far, don't draw
			if (umbreons[i].getCaught() == 1)
			{
				continue;
			}

			umbreons[i].update(frametime);

			if (distance > 50)
			{
				continue;
			}
			vec3 wildPosition = umbreons[i].getPos();
			wildPosition.y += 0.2f + 0.04f * std::sin(motionTime * 2.4f + umbreons[i].getMotionPhase());
			T = glm::translate(glm::mat4(1.0f), wildPosition);
			R = glm::rotate(glm::mat4(1.0f), umbreons[i].getHeading(), glm::vec3(0.0f, 1.0f, 0.0f));
			M = T * R * S;
			glUniformMatrix4fv(pokemon2->getUniform("M"), 1, GL_FALSE, &M[0][0]);
			shared_ptr<Shape> wildShape = companionShapes.empty()
				? umbreon
				: companionShapes[static_cast<size_t>(i) % companionShapes.size()];
			wildShape->draw(pokemon2, false);
		}

		S = glm::scale(glm::mat4(1.0f), glm::vec3(1.6f, 1.6f, 1.6f));
		// charizard
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, fireTex);
		for (int i = 0; i < FLYING_POKEMON; i++)
		{
			float distance = sqrt(pow(mypos.x - charizards[i].getPos().x, 2) + pow(mypos.z - charizards[i].getPos().z, 2));
			if (charizards[i].getCaught() == 1)
			{
				continue;
			}
			charizards[i].update(frametime);
			if (distance > 100)
			{
				continue;
			}
			vec3 flightPosition = charizards[i].getPos();
			flightPosition.y += 0.45f * std::sin(motionTime * 1.8f + charizards[i].getMotionPhase());
			T = glm::translate(glm::mat4(1.0f), flightPosition);
			R = glm::rotate(glm::mat4(1.0f), charizards[i].getHeading(), glm::vec3(0.0f, 1.0f, 0.0f));
			M = T * R * S;
			glUniformMatrix4fv(pokemon2->getUniform("M"), 1, GL_FALSE, &M[0][0]);
			charizard->draw(pokemon2, false);
		}

		pokemon2->unbind();
	}

	void frame()
	{
		render();
		glfwSwapBuffers(windowManager->getHandle());
		glfwPollEvents();
	}
};
#ifdef __EMSCRIPTEN__
void webMainLoop(void *userData)
{
	static_cast<Application *>(userData)->frame();
}
#endif
//******************************************************************************************
int main(int argc, char **argv)
{
	srand(time(NULL));
	std::string resourceDir = "resources"; // Where the resources are loaded from
	if (argc >= 2)
	{
		resourceDir = argv[1];
	}
	auto hasResources = [](const std::string &directory) {
		std::ifstream sphere(directory + "/sphere.obj");
		return sphere.good();
	};
	if (argc < 2 && !hasResources(resourceDir))
	{
		resourceDir = "../resources";
	}
	if (!hasResources(resourceDir))
	{
		std::cerr << "Resource directory not found: " << resourceDir << std::endl;
		std::cerr << "Run from the project root or pass the resources path as an argument." << std::endl;
		return 1;
	}

	Application *application = new Application();

	/* your main will always include a similar set up to establish your window
		and GL context, etc. */
	WindowManager *windowManager = new WindowManager();
#ifdef __EMSCRIPTEN__
	const int windowWidth = 1280;
	const int windowHeight = 720;
#else
	const int windowWidth = 1920;
	const int windowHeight = 1080;
#endif
	if (!windowManager->init(windowWidth, windowHeight))
	{
		std::cerr << "Unable to initialize the game window." << std::endl;
		delete windowManager;
		delete application;
		return 1;
	}
	windowManager->setEventCallbacks(application);
	application->windowManager = windowManager;

	/* This is the code that will likely change program to program as you
		may need to initialize or set up different data and state */
	// Initialize scene.
	application->init(resourceDir);
	application->initGeom();

	// Native builds own the event loop. Browsers must return control to the
	// JavaScript event loop, so Emscripten calls one frame at a time instead.
#ifdef __EMSCRIPTEN__
	emscripten_set_main_loop_arg(webMainLoop, application, 0, 1);
#else
	while (!glfwWindowShouldClose(windowManager->getHandle()))
	{
		application->frame();
	}

	// Quit program.
	windowManager->shutdown();
#endif
	return 0;
}
