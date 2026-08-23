/*
CPE/CSC 471 Lab base code Wood/Dunn/Eckhardt
Modified by: <Zhisong Liang>
*/

#include <iostream>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <utility>
#include <vector>
#include "GLCompat.h"
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include "GLSL.h"
#include "BattleMechanics.h"
#include "BattleMoveLoadout.h"
#include "BattleSequence.h"
#include "CaptureMechanics.h"
#include "CaptureSequence.h"
#include "GameSave.h"
#include "GameSaveStorage.h"
#include "Program.h"
#include "MatrixStack.h"
#include "Pokemon.h"
#include "PokemonAnimation.h"
#include "PokemonTargeting.h"
#include "PlayerController.h"
#include "ResearchMission.h"
#include "TerrainHeightMap.h"
#include "ThirdPersonCamera.h"

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
shared_ptr<Shape> bulbasaur;
shared_ptr<Shape> charizard;

constexpr int NUM_POKEMON = 48;
constexpr int FLYING_POKEMON = 8;
constexpr int STARTING_POKEBALLS = 10;
constexpr int CAPTURE_GOAL = 5;

struct RockPlacement
{
	glm::vec2 center;
	glm::vec3 scale;
	float yaw;
};

struct CaptureBallVisualPose
{
	glm::vec3 position = glm::vec3(0.0f);
	float pitch = 0.0f;
	float roll = 0.0f;
	float scale = 0.17f;
};

struct BattleEffectPalette
{
	glm::vec3 effectColor;
	glm::vec3 coreColor;
};

float easedBattleProgress(float progress)
{
	const float clamped = glm::clamp(progress, 0.0f, 1.0f);
	return clamped * clamped * (3.0f - 2.0f * clamped);
}

glm::vec3 battleProjectilePosition(const glm::vec3 &start,
	                               const glm::vec3 &end, float progress)
{
	const float eased = easedBattleProgress(progress);
	const float distance = glm::length(end - start);
	const float arcHeight = glm::clamp(distance * 0.055f, 0.32f, 0.92f);
	return glm::mix(start, end, eased) +
	       glm::vec3(0.0f, std::sin(eased * 3.1415926f) * arcHeight, 0.0f);
}

BattleEffectPalette battleEffectPalette(PokemonType type)
{
	switch (type)
	{
	case PokemonType::Fire:
		return {glm::vec3(1.0f, 0.18f, 0.015f),
		        glm::vec3(1.0f, 0.86f, 0.18f)};
	case PokemonType::Grass:
		return {glm::vec3(0.10f, 0.74f, 0.20f),
		        glm::vec3(0.72f, 1.0f, 0.30f)};
	case PokemonType::Flying:
		return {glm::vec3(0.12f, 0.62f, 1.0f),
		        glm::vec3(0.78f, 0.96f, 1.0f)};
	case PokemonType::Dark:
		return {glm::vec3(0.42f, 0.10f, 0.72f),
		        glm::vec3(1.0f, 0.38f, 0.82f)};
	case PokemonType::Normal:
		return {glm::vec3(0.72f, 0.78f, 0.86f),
		        glm::vec3(1.0f)};
	}
	return {glm::vec3(0.72f), glm::vec3(1.0f)};
}

float battleMoveVisualScale(BattleMoveId move)
{
	switch (move)
	{
	case BattleMoveId::Ember:
		return 0.90f;
	case BattleMoveId::AirSlash:
		return 1.08f;
	case BattleMoveId::Flamethrower:
		return 1.38f;
	case BattleMoveId::VineWhip:
	case BattleMoveId::Bite:
	case BattleMoveId::WingAttack:
		return 1.0f;
	}
	return 1.0f;
}

int battleMoveTrailCount(BattleMoveId move)
{
	switch (move)
	{
	case BattleMoveId::AirSlash:
		return 3;
	case BattleMoveId::Flamethrower:
		return 6;
	case BattleMoveId::Ember:
	case BattleMoveId::VineWhip:
	case BattleMoveId::Bite:
	case BattleMoveId::WingAttack:
		return 1;
	}
	return 1;
}

void applyPlayerBattlePose(PokemonAnimationPose &pose,
	                       const BattleSequenceSample &sample,
	                       const BattleMove &move)
{
	const float eased = easedBattleProgress(sample.phaseProgress);
	const bool airSlash = move.id == BattleMoveId::AirSlash;
	const bool flamethrower = move.id == BattleMoveId::Flamethrower;
	if (sample.phase == BattlePhase::PlayerWindup)
	{
		pose.bodyPitch -= eased * (flamethrower ? 0.17f : 0.11f);
		pose.wingAngle = airSlash
		                     ? pose.wingAngle + eased * 0.28f
		                     : pose.wingAngle * 0.55f;
		pose.breathingScale += eased * (flamethrower ? 0.032f : 0.018f);
	}
	else if (sample.phase == BattlePhase::PlayerProjectile)
	{
		const float release = std::sin(sample.phaseProgress * 3.1415926f);
		pose.bodyPitch -= (1.0f - eased) * (flamethrower ? 0.17f : 0.11f);
		pose.bodyBob += release * (airSlash ? 0.09f : 0.055f);
		pose.wingAngle += release * (airSlash ? 0.38f : 0.13f);
		if (airSlash)
		{
			pose.bodyRoll += std::sin(sample.phaseProgress * 6.2831853f) * 0.10f;
		}
	}
	else if (sample.phase == BattlePhase::PlayerImpact)
	{
		const float recoil = std::sin(sample.phaseProgress * 3.1415926f);
		pose.bodyPitch += recoil * 0.11f;
		pose.bodyRoll += recoil * 0.16f;
	}
}

void applyWildBattlePose(PokemonAnimationPose &pose,
	                     const BattleSequenceSample &sample)
{
	const float eased = easedBattleProgress(sample.phaseProgress);
	if (sample.phase == BattlePhase::TargetImpact)
	{
		const float recoil = std::sin(sample.phaseProgress * 3.1415926f);
		pose.bodyPitch += recoil * 0.13f;
		pose.bodyRoll -= recoil * 0.19f;
		pose.bodyBob += recoil * 0.08f;
	}
	else if (sample.phase == BattlePhase::WildWindup)
	{
		pose.bodyPitch -= eased * 0.10f;
		pose.breathingScale += eased * 0.022f;
	}
	else if (sample.phase == BattlePhase::WildProjectile)
	{
		pose.bodyPitch -= (1.0f - eased) * 0.10f;
	}
}

const std::array<RockPlacement, 10> ROCK_PLACEMENTS = {{
	{glm::vec2(5.0f, -7.0f), glm::vec3(1.0f, 0.70f, 0.85f), 0.25f},
	{glm::vec2(-7.0f, -10.0f), glm::vec3(0.8f, 0.90f, 0.75f), -0.4f},
	{glm::vec2(12.0f, -4.0f), glm::vec3(1.25f, 0.75f, 0.95f), 0.75f},
	{glm::vec2(-13.0f, 4.0f), glm::vec3(1.05f, 0.60f, 0.90f), 0.1f},
	{glm::vec2(8.0f, 12.0f), glm::vec3(0.9f, 1.05f, 0.80f), -0.65f},
	{glm::vec2(-5.0f, 15.0f), glm::vec3(1.15f, 0.70f, 1.0f), 0.45f},
	{glm::vec2(18.0f, -16.0f), glm::vec3(1.4f, 0.85f, 1.05f), -0.2f},
	{glm::vec2(-19.0f, -14.0f), glm::vec3(1.0f, 1.15f, 0.90f), 0.6f},
	{glm::vec2(22.0f, 9.0f), glm::vec3(1.15f, 0.65f, 1.30f), -0.8f},
	{glm::vec2(-23.0f, 18.0f), glm::vec3(1.3f, 0.75f, 1.0f), 0.3f},
}};
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
		cameraPose_ = cameraRig_.update(mypos, controller_.yaw(), static_cast<float>(ftime));
		return glm::lookAt(cameraPose_.position, cameraPose_.target, glm::vec3(0.0f, 1.0f, 0.0f));
	}

	void reset()
	{
		w = a = s = d = q = e = space = 0;
		controller_.reset();
		cameraRig_.reset();
		motionEvents_ = PlayerMotionEvents();
		mypos = controller_.position();
		pos = -mypos;
		cameraPose_ = cameraRig_.update(mypos, controller_.yaw(), 0.0f);
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

	void setGroundHeightProvider(PlayerController::GroundHeightProvider provider)
	{
		controller_.setGroundHeightProvider(provider);
		cameraRig_.setGroundHeightProvider(std::move(provider));
		cameraRig_.reset();
		mypos = controller_.position();
		pos = -mypos;
		cameraPose_ = cameraRig_.update(mypos, controller_.yaw(), 0.0f);
	}

	void setStaticObstacles(std::vector<StaticCollisionCylinder> obstacles)
	{
		cameraRig_.setStaticObstacles(obstacles);
		controller_.setStaticObstacles(std::move(obstacles));
	}

	float yaw() const
	{
		return controller_.yaw();
	}

	glm::vec3 velocity() const
	{
		return controller_.velocity();
	}

	float turnRatio() const
	{
		return static_cast<float>(d - a);
	}

	const PlayerMotionEvents &motionEvents() const
	{
		return motionEvents_;
	}

private:
	PlayerController controller_;
	ThirdPersonCamera cameraRig_;
	ThirdPersonCameraPose cameraPose_;
	PlayerMotionEvents motionEvents_;
};

camera mycam;

class Application : public EventCallbacks
{

public:
	WindowManager *windowManager = nullptr;

	// Our shader program
	std::shared_ptr<Program> prog, prog2, heightshader, pokemon, pokemon2,
	                         targetshader, pokeballShader, battleEffectShader;

	// Contains vertex information for OpenGL
	GLuint VertexArrayID, VertexArrayID2;

	GLuint VertexBufferIDScreen, VertexNormalIDBox, VertexTexIDBox, VertexBufferTexScreen;
	GLuint VertexBufferID2, VertexNormDBox2, VertexTexBox2, IndexBufferIDBox2, InstanceBuffer;

	// Data necessary to give our box to OpenGL
	GLuint MeshPosID, MeshTexID, IndexBufferIDBox;

	// texture data
GLuint Texture, grassTexture, HeightTex, PokeballTex, fireTex, Texture5;
GLuint rockTex, umbreonTex;
	GLuint bulbasaurBodyTex = 0;
	GLuint bulbasaurSpotTex = 0;
	GLuint bulbasaurBulbTex = 0;
	GLuint bulbasaurLeafTex = 0;
	GLuint bulbasaurEyeWhiteTex = 0;
	GLuint bulbasaurEyeRedTex = 0;
	GLuint bulbasaurEyeDarkTex = 0;
	TerrainHeightMap terrainHeightMap;
	std::string resourceDirectory;
	int caughtCount = 0;
	int pokeballs = STARTING_POKEBALLS;
	bool captureRequested = false;
	bool attackRequested = false;
	bool resetRequested = false;
	bool gameFinished = false;
	std::string statusMessage = "Explore the field and find a Pokemon.";
	PokemonTargetSelection currentTarget;
	double nextTelemetryUpdate = 0.0;
	glm::vec3 captureEffectPosition = glm::vec3(0.0f);
	double captureEffectStarted = -100.0;
	bool captureEffectSucceeded = true;
	CaptureRandom captureRandom{static_cast<std::uint32_t>(std::time(nullptr))};
	bool captureSequenceActive = false;
	CaptureResult pendingCaptureResult;
	Pokemon *pendingCaptureTarget = nullptr;
	double captureSequenceStarted = -100.0;
	CapturePhase lastCapturePhase = CapturePhase::Inactive;
	int lastCaptureShake = 0;
	std::string pendingCaptureSpecies;
	glm::vec3 captureThrowStart = glm::vec3(0.0f);
	glm::vec3 captureHitPosition = glm::vec3(0.0f);
	glm::vec3 captureBallRestPosition = glm::vec3(0.0f);
	int playerHealth = 118;
	int defeatedCount = 0;
	bool battleSequenceActive = false;
	BattleSequencePlan pendingBattlePlan;
	BattleDamageResult pendingPlayerDamage;
	BattleDamageResult pendingWildDamage;
	BattleMove pendingPlayerMove;
	BattleMove pendingWildMove;
	BattleMoveLoadout playerMoveLoadout;
	Pokemon *pendingBattleTarget = nullptr;
	double battleSequenceStarted = -100.0;
	BattlePhase lastBattlePhase = BattlePhase::Inactive;
	bool targetDamageApplied = false;
	bool playerDamageApplied = false;
	std::string pendingBattleSpecies;
	glm::vec3 battlePlayerOrigin = glm::vec3(0.0f);
	glm::vec3 battleTargetPosition = glm::vec3(0.0f);
	glm::vec3 battlePlayerHitPosition = glm::vec3(0.0f);
	ResearchMissionProgress researchProgress;
	double resetConfirmationExpires = -100.0;
	float playerAnimationPhase = 0.0f;

	void updateWindowTitle()
	{
		if (!windowManager || !windowManager->getHandle())
		{
			return;
		}

		std::ostringstream title;
		title << "Pokemon World | W/S move  A/D turn  Q/E/Space fly  Z gravity  1/2/3 moves  X attack  C catch  R reset"
		      << " | Caught " << caughtCount << "/" << CAPTURE_GOAL
		      << " | Defeated " << defeatedCount
		      << " | Poke Balls " << pokeballs
		      << " | HP " << playerHealth << "/"
		      << battleStatsFor(PokemonSpecies::Charizard).maximumHealth
		      << " | Move " << playerMoveLoadout.selectedMove().name
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

	void notifySaveState(const std::string &message, bool available)
	{
#ifdef __EMSCRIPTEN__
		EM_ASM({
			if (Module.onSaveState)
			{
				Module.onSaveState(UTF8ToString($0), $1 !== 0);
			}
		}, message.c_str(), available ? 1 : 0);
#else
		(void)message;
		(void)available;
#endif
	}

	void emitGameCue(const char *cue, PokemonType type = PokemonType::Normal)
	{
#ifdef __EMSCRIPTEN__
		EM_ASM({
			if (Module.onGameCue)
			{
				Module.onGameCue(UTF8ToString($0), $1);
			}
		}, cue, static_cast<int>(type));
#else
		(void)cue;
		(void)type;
#endif
	}

	GameSaveLimits gameSaveLimits() const
	{
		GameSaveLimits limits;
		limits.captureGoal = CAPTURE_GOAL;
		limits.startingPokeballs = STARTING_POKEBALLS;
		limits.playerMaximumHealth =
			battleStatsFor(PokemonSpecies::Charizard).maximumHealth;
		limits.groundMaximumHealth.reserve(NUM_POKEMON);
		for (int index = 0; index < NUM_POKEMON; ++index)
		{
			limits.groundMaximumHealth.push_back(
				battleStatsFor(groundPokemonSpeciesForIndex(index)).maximumHealth);
		}
		limits.flyingMaximumHealth.assign(
			FLYING_POKEMON,
			battleStatsFor(PokemonSpecies::Charizard).maximumHealth);
		return limits;
	}

	GameSaveData currentGameSave() const
	{
		GameSaveData data;
		data.caughtCount = caughtCount;
		data.pokeballs = pokeballs;
		data.defeatedCount = defeatedCount;
		data.playerHealth = playerHealth;
		data.missionProgress = researchProgress;
		data.groundPokemon.reserve(NUM_POKEMON);
		for (int index = 0; index < NUM_POKEMON; ++index)
		{
			data.groundPokemon.push_back(
				{umbreons[index].getHealth(), umbreons[index].getCaught() != 0});
		}
		data.flyingPokemon.reserve(FLYING_POKEMON);
		for (int index = 0; index < FLYING_POKEMON; ++index)
		{
			data.flyingPokemon.push_back(
				{charizards[index].getHealth(), charizards[index].getCaught() != 0});
		}
		return data;
	}

	bool saveGameProgress(const std::string &message = "Saved")
	{
		const std::string payload =
			encodeGameSave(currentGameSave(), gameSaveLimits());
		if (payload.empty())
		{
			std::cerr << "Autosave was blocked because runtime progress was invalid."
			          << std::endl;
			notifySaveState("Save blocked", false);
			return false;
		}
		const bool saved = writeGameSaveStorage(payload);
		notifySaveState(saved ? message : "Save unavailable", saved);
		if (!saved)
		{
			std::cerr << "Unable to write Pokemon World autosave." << std::endl;
		}
		return saved;
	}

	bool applySavedProgress(const GameSaveData &data)
	{
		if (!validateGameSave(data, gameSaveLimits()))
		{
			return false;
		}
		for (int index = 0; index < NUM_POKEMON; ++index)
		{
			if (!umbreons[index].setHealth(data.groundPokemon[index].health))
			{
				return false;
			}
			umbreons[index].setCaught(data.groundPokemon[index].caught ? 1 : 0);
		}
		for (int index = 0; index < FLYING_POKEMON; ++index)
		{
			if (!charizards[index].setHealth(data.flyingPokemon[index].health))
			{
				return false;
			}
			charizards[index].setCaught(data.flyingPokemon[index].caught ? 1 : 0);
		}

		mycam.reset();
		caughtCount = data.caughtCount;
		pokeballs = data.pokeballs;
		defeatedCount = data.defeatedCount;
		playerHealth = data.playerHealth;
		researchProgress = data.missionProgress;
		captureRequested = false;
		attackRequested = false;
		resetRequested = false;
		currentTarget = PokemonTargetSelection();
		nextTelemetryUpdate = 0.0;
		captureSequenceActive = false;
		pendingCaptureTarget = nullptr;
		battleSequenceActive = false;
		pendingBattleTarget = nullptr;
		playerMoveLoadout.reset();
		resetConfirmationExpires = -100.0;
		gameFinished = caughtCount >= CAPTURE_GOAL || playerHealth <= 0 ||
		               (pokeballs == 0 && caughtCount < CAPTURE_GOAL);

		std::ostringstream message;
		if (caughtCount >= CAPTURE_GOAL)
		{
			message << "Research complete! Autosave restored. Press R twice for a new run.";
		}
		else if (playerHealth <= 0)
		{
			message << "Charizard needs recovery. Autosave restored; press R twice to retry.";
		}
		else if (pokeballs == 0)
		{
			message << "Out of Poke Balls. Autosave restored; press R twice to retry.";
		}
		else
		{
			message << "Autosave restored: " << caughtCount << "/" << CAPTURE_GOAL
			        << " research samples.";
		}
		setStatus(message.str());
		notifySaveState("Progress restored", true);
		return true;
	}

	void restoreGameProgress()
	{
		const GameSaveStorageReadResult stored = readGameSaveStorage();
		if (stored.status == GameSaveStorageStatus::NotFound)
		{
			setStatus("Explore the field and find a Pokemon.");
			notifySaveState("Autosave ready", true);
			return;
		}
		if (stored.status == GameSaveStorageStatus::Error)
		{
			setStatus("Autosave is unavailable; this run will continue without loading it.");
			notifySaveState("Save unavailable", false);
			return;
		}
		const GameSaveParseResult parsed =
			parseGameSave(stored.payload, gameSaveLimits());
		if (!parsed.valid || !applySavedProgress(parsed.data))
		{
			setStatus("Autosave was invalid and was ignored. Press R twice to replace it.");
			notifySaveState("Invalid save ignored", false);
		}
	}

	void addSceneLightingUniforms(const std::shared_ptr<Program> &program)
	{
		program->addUniform("sunDirection");
		program->addUniform("sunColor");
		program->addUniform("ambientColor");
		program->addUniform("fogColor");
		program->addUniform("fogStart");
		program->addUniform("fogEnd");
	}

	void applySceneLighting(const std::shared_ptr<Program> &program, const glm::mat4 &view)
	{
		const glm::vec3 sunDirectionWorld = glm::normalize(glm::vec3(-0.38f, 0.82f, -0.42f));
		const glm::vec3 sunDirectionView = glm::normalize(glm::mat3(view) * sunDirectionWorld);
		const glm::vec3 sunColor(1.0f, 0.91f, 0.74f);
		const glm::vec3 ambientColor(0.52f, 0.60f, 0.70f);
		const glm::vec3 fogColor(0.66f, 0.84f, 0.96f);
		glUniform3fv(program->getUniform("sunDirection"), 1, &sunDirectionView[0]);
		glUniform3fv(program->getUniform("sunColor"), 1, &sunColor[0]);
		glUniform3fv(program->getUniform("ambientColor"), 1, &ambientColor[0]);
		glUniform3fv(program->getUniform("fogColor"), 1, &fogColor[0]);
		glUniform1f(program->getUniform("fogStart"), 26.0f);
		glUniform1f(program->getUniform("fogEnd"), 62.0f);
	}

	void applyCharizardAnimation(const std::shared_ptr<Program> &program,
	                            const PokemonAnimationPose &pose, bool enabled)
	{
		glUniform1f(program->getUniform("animationMode"), enabled ? 1.0f : 0.0f);
		glUniform1f(program->getUniform("wingAngle"), pose.wingAngle);
		glUniform1f(program->getUniform("tailAngle"), pose.tailAngle);
		glUniform1f(program->getUniform("breathingScale"), pose.breathingScale);
	}

	GLuint createSolidTexture(unsigned char red, unsigned char green,
	                         unsigned char blue)
	{
		const unsigned char pixel[] = {red, green, blue, 255};
		GLuint texture = 0;
		glGenTextures(1, &texture);
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, texture);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 1, 1, 0, GL_RGBA,
		             GL_UNSIGNED_BYTE, pixel);
		return texture;
	}

	void resetGame()
	{
		resetRequested = false;
		if (!clearGameSaveStorage())
		{
			setStatus("Unable to clear autosave; the new run was cancelled.");
			notifySaveState("Save unavailable", false);
			return;
		}

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
		attackRequested = false;
		gameFinished = false;
		currentTarget = PokemonTargetSelection();
		nextTelemetryUpdate = 0.0;
		captureEffectStarted = -100.0;
		captureEffectSucceeded = true;
		captureSequenceActive = false;
		pendingCaptureTarget = nullptr;
		captureSequenceStarted = -100.0;
		lastCapturePhase = CapturePhase::Inactive;
		lastCaptureShake = 0;
		pendingCaptureSpecies.clear();
		playerHealth = battleStatsFor(PokemonSpecies::Charizard).maximumHealth;
		defeatedCount = 0;
		battleSequenceActive = false;
		pendingBattleTarget = nullptr;
		battleSequenceStarted = -100.0;
		lastBattlePhase = BattlePhase::Inactive;
		targetDamageApplied = false;
		playerDamageApplied = false;
		pendingBattleSpecies.clear();
		playerMoveLoadout.reset();
		researchProgress = ResearchMissionProgress();
		resetConfirmationExpires = -100.0;
		playerAnimationPhase = 0.0f;
		setStatus("New research run started. Explore the field and find a Pokemon.");
		saveGameProgress("New game saved");
	}

	glm::vec3 pokemonWorldPosition(const Pokemon &candidate) const
	{
		glm::vec3 position = candidate.getPos();
		if (!candidate.isFlying())
		{
			position.y = terrainHeightMap.heightAt(position.x, position.z);
		}
		return position;
	}

	CaptureActivity captureActivityFor(const Pokemon &candidate) const
	{
		switch (candidate.getBehaviorState())
		{
		case PokemonBehaviorState::Idle:
			return CaptureActivity::Idle;
		case PokemonBehaviorState::Flee:
			return CaptureActivity::Fleeing;
		case PokemonBehaviorState::Wander:
			return CaptureActivity::Moving;
		}
		return CaptureActivity::Moving;
	}

	CaptureSequenceSample currentCaptureSample(double now) const
	{
		return captureSequenceActive
		           ? sampleCaptureSequence(
			             pendingCaptureResult,
			             static_cast<float>(now - captureSequenceStarted))
		           : CaptureSequenceSample();
	}

	CaptureBallVisualPose captureBallVisualPose(
	    const CaptureSequenceSample &sample) const
	{
		CaptureBallVisualPose pose;
		pose.position = captureBallRestPosition;
		if (sample.phase == CapturePhase::Throwing)
		{
			const float progress = sample.phaseProgress;
			const float smoothProgress = progress * progress * (3.0f - 2.0f * progress);
			pose.position = glm::mix(captureThrowStart, captureHitPosition, smoothProgress);
			pose.position.y += std::sin(progress * 3.1415926f) * 1.25f;
			pose.pitch = progress * 12.5663706f;
			pose.roll = progress * 6.2831853f;
		}
		else if (sample.phase == CapturePhase::Absorbing)
		{
			pose.position = glm::mix(captureHitPosition, captureBallRestPosition,
			                         sample.phaseProgress);
			pose.pitch = 12.5663706f + sample.phaseProgress * 3.1415926f;
			pose.scale += std::sin(sample.phaseProgress * 3.1415926f) * 0.025f;
		}
		else if (sample.phase == CapturePhase::Shaking)
		{
			const float shake = std::sin(sample.phaseProgress * 6.2831853f);
			glm::vec2 toPlayer(mypos.x - pose.position.x,
			                   mypos.z - pose.position.z);
			glm::vec2 sideways(1.0f, 0.0f);
			if (glm::length(toPlayer) > 0.001f)
			{
				toPlayer = glm::normalize(toPlayer);
				sideways = glm::vec2(-toPlayer.y, toPlayer.x);
			}
			pose.position.x += sideways.x * shake * 0.16f;
			pose.position.z += sideways.y * shake * 0.16f;
			pose.roll = shake * 0.42f;
		}
		else if (sample.phase == CapturePhase::Succeeded)
		{
			pose.scale += std::sin(sample.phaseProgress * 3.1415926f) * 0.012f;
		}
		return pose;
	}

	bool isPendingCaptureTarget(const Pokemon &candidate) const
	{
		return captureSequenceActive && pendingCaptureTarget == &candidate;
	}

	BattleSequenceSample currentBattleSample(double now) const
	{
		return battleSequenceActive
		           ? sampleBattleSequence(
			             pendingBattlePlan,
			             static_cast<float>(now - battleSequenceStarted))
		           : BattleSequenceSample();
	}

	bool isPendingBattleTarget(const Pokemon &candidate) const
	{
		return battleSequenceActive && pendingBattleTarget == &candidate;
	}

	bool battlePhaseAtLeast(BattlePhase phase, BattlePhase threshold) const
	{
		return static_cast<int>(phase) >= static_cast<int>(threshold);
	}

	std::string effectivenessMessage(const BattleDamageResult &damage) const
	{
		if (damage.effectiveness > 1.01f)
		{
			return " It's super effective!";
		}
		if (damage.effectiveness < 0.99f)
		{
			return " It's not very effective.";
		}
		return std::string();
	}

	void finishCaptureSequence()
	{
		Pokemon *target = pendingCaptureTarget;
		const bool captured = pendingCaptureResult.captured && target;
		captureSequenceActive = false;
		pendingCaptureTarget = nullptr;
		lastCapturePhase = CapturePhase::Finished;

		if (captured)
		{
			target->setCaught(1);
			++caughtCount;
			if (caughtCount >= CAPTURE_GOAL)
			{
				gameFinished = true;
				setStatus("Research complete! Press R twice to play again.");
			}
			else if (pokeballs == 0)
			{
				gameFinished = true;
				setStatus("Out of Poke Balls before the goal. Press R twice to retry.");
			}
			else
			{
				std::ostringstream message;
				message << "Captured " << pendingCaptureSpecies << "! "
				        << caughtCount << "/" << CAPTURE_GOAL
				        << " research samples complete.";
				setStatus(message.str());
			}
		}
		else
		{
			if (target)
			{
				target->startle();
			}
			std::ostringstream message;
			message << pendingCaptureSpecies << " broke free";
			if (pendingCaptureResult.shakes > 0)
			{
				message << " after " << pendingCaptureResult.shakes
				        << (pendingCaptureResult.shakes == 1 ? " shake" : " shakes");
			}
			message << ". ";
			if (pokeballs == 0)
			{
				gameFinished = true;
				message << "Out of Poke Balls. Press R twice to retry.";
			}
			else
			{
				message << pokeballs << " Poke Balls left.";
			}
			setStatus(message.str());
		}
		saveGameProgress();
	}

	void updateCaptureSequence(double now)
	{
		if (!captureSequenceActive)
		{
			return;
		}
		const CaptureSequenceSample sample = currentCaptureSample(now);
		if (sample.phase != lastCapturePhase)
		{
			lastCapturePhase = sample.phase;
			if (sample.phase == CapturePhase::Absorbing)
			{
				emitGameCue("capture-hit");
				setStatus("Hit! " + pendingCaptureSpecies + " was pulled into the Poke Ball.");
			}
			else if (sample.phase == CapturePhase::Succeeded)
			{
				emitGameCue("capture-success");
				captureEffectPosition = captureBallRestPosition;
				captureEffectStarted = now;
				captureEffectSucceeded = true;
				setStatus("Click! " + pendingCaptureSpecies + " was caught.");
			}
			else if (sample.phase == CapturePhase::BrokeFree)
			{
				emitGameCue("capture-fail");
				captureEffectPosition = captureBallRestPosition;
				captureEffectStarted = now;
				captureEffectSucceeded = false;
				if (pendingCaptureTarget)
				{
					pendingCaptureTarget->startle();
				}
				setStatus(pendingCaptureSpecies + " broke free!");
			}
		}
		if (sample.phase == CapturePhase::Shaking &&
		    sample.shakeIndex != lastCaptureShake)
		{
			lastCaptureShake = sample.shakeIndex;
			emitGameCue("capture-shake");
			setStatus("Shake " + std::to_string(sample.shakeIndex) + "...");
		}
		if (sample.finished)
		{
			finishCaptureSequence();
		}
	}

	void finishBattleSequence()
	{
		Pokemon *target = pendingBattleTarget;
		const bool targetFainted = target && target->isFainted();
		battleSequenceActive = false;
		pendingBattleTarget = nullptr;
		lastBattlePhase = BattlePhase::Finished;

		if (playerHealth <= 0)
		{
			gameFinished = true;
			setStatus("Charizard can no longer battle. Press R twice to recover and retry.");
			return;
		}
		if (targetFainted)
		{
			std::ostringstream message;
			message << pendingBattleSpecies << " fainted. " << defeatedCount
			        << " wild Pokemon defeated.";
			setStatus(message.str());
			return;
		}
		if (target)
		{
			target->startle();
			std::ostringstream message;
			message << pendingBattleSpecies << " has " << target->getHealth()
			        << "/" << target->getMaximumHealth()
			        << " HP. Weaken it further or press C to throw.";
			setStatus(message.str());
		}
	}

	void updateBattleSequence(double now)
	{
		if (!battleSequenceActive)
		{
			return;
		}
		const BattleSequenceSample sample = currentBattleSample(now);
		if (!targetDamageApplied &&
		    battlePhaseAtLeast(sample.phase, BattlePhase::TargetImpact))
		{
			targetDamageApplied = true;
			if (pendingBattleTarget)
			{
				const int appliedDamage =
					pendingBattleTarget->applyDamage(pendingPlayerDamage.amount);
				emitGameCue("target-impact", pendingPlayerMove.type);
				if (pendingPlayerDamage.effectiveness > 1.01f)
				{
					recordSuperEffectiveHit(researchProgress);
				}
				if (pendingBattleTarget->isFainted())
				{
					++defeatedCount;
					setStatus(pendingBattleSpecies + " fainted from " +
					          pendingPlayerMove.name + "!");
				}
				else
				{
					std::ostringstream message;
					message << pendingBattleSpecies << " took " << appliedDamage
					        << " damage (" << pendingBattleTarget->getHealth()
					        << "/" << pendingBattleTarget->getMaximumHealth()
					        << " HP)." << effectivenessMessage(pendingPlayerDamage);
					setStatus(message.str());
				}
				saveGameProgress();
			}
		}
		if (pendingBattlePlan.counterEnabled && !playerDamageApplied &&
		    battlePhaseAtLeast(sample.phase, BattlePhase::PlayerImpact))
		{
			playerDamageApplied = true;
			const int appliedDamage = std::min(playerHealth, pendingWildDamage.amount);
			playerHealth = std::max(0, playerHealth - appliedDamage);
			emitGameCue("player-impact", pendingWildMove.type);
			std::ostringstream message;
			message << pendingBattleSpecies << " dealt " << appliedDamage
			        << " damage with " << pendingWildMove.name << " (Charizard "
			        << playerHealth << "/"
			        << battleStatsFor(PokemonSpecies::Charizard).maximumHealth
			        << " HP)." << effectivenessMessage(pendingWildDamage);
			setStatus(message.str());
			saveGameProgress();
		}

		if (sample.phase != lastBattlePhase)
		{
			lastBattlePhase = sample.phase;
			if (sample.phase == BattlePhase::PlayerProjectile)
			{
				emitGameCue("player-attack", pendingPlayerMove.type);
				setStatus(std::string("Charizard used ") + pendingPlayerMove.name + "!");
			}
			else if (sample.phase == BattlePhase::WildWindup &&
			         pendingBattlePlan.counterEnabled)
			{
				setStatus(pendingBattleSpecies + " prepares " + pendingWildMove.name + "!");
			}
			else if (sample.phase == BattlePhase::WildProjectile &&
			         pendingBattlePlan.counterEnabled)
			{
				emitGameCue("wild-attack", pendingWildMove.type);
			}
		}
		if (sample.finished)
		{
			finishBattleSequence();
		}
	}

	void selectPlayerMove(int slot)
	{
		if (!playerMoveLoadout.selectSlot(slot))
		{
			return;
		}
		const BattleMove &move = playerMoveLoadout.selectedMove();
		const double remaining = playerMoveLoadout.cooldownRemaining(
			slot, glfwGetTime());
		std::ostringstream message;
		message << "Selected " << move.name << ". ";
		if (remaining > 0.0)
		{
			message << std::fixed << std::setprecision(1) << remaining
			        << "s until ready.";
		}
		else
		{
			message << "Ready to use.";
		}
		setStatus(message.str());
		emitGameCue("move-select", move.type);
	}

	void attackTargetedPokemon()
	{
		if (captureSequenceActive || battleSequenceActive)
		{
			setStatus("Finish the current action before attacking again.");
			return;
		}
		if (gameFinished)
		{
			return;
		}
		const double now = glfwGetTime();
		const BattleMove &selectedMove = playerMoveLoadout.selectedMove();
		const double cooldownRemaining = playerMoveLoadout.cooldownRemaining(
			playerMoveLoadout.selectedSlot(), now);
		if (cooldownRemaining > 0.0)
		{
			std::ostringstream message;
			message << selectedMove.name << " is recharging for " << std::fixed
			        << std::setprecision(1) << cooldownRemaining << "s.";
			setStatus(message.str());
			return;
		}
		Pokemon *target = targetedPokemon();
		if (!target)
		{
			setStatus("No battle target. Face a Pokemon inside the lock-on range.");
			return;
		}
		if (!playerMoveLoadout.consumeSelected(now))
		{
			setStatus("The selected move is not ready yet.");
			return;
		}

		pendingBattleTarget = target;
		pendingBattleSpecies = pokemonSpeciesName(target->getSpecies());
		pendingPlayerMove = selectedMove;
		pendingWildMove = wildBattleMoveFor(target->getSpecies());
		pendingPlayerDamage = resolveBattleDamage(
			PokemonSpecies::Charizard, target->getSpecies(), pendingPlayerMove);
		pendingWildDamage = resolveBattleDamage(
			target->getSpecies(), PokemonSpecies::Charizard, pendingWildMove);
		pendingBattlePlan.counterEnabled =
			target->getHealth() > pendingPlayerDamage.amount;
		battleSequenceActive = true;
		battleSequenceStarted = now;
		lastBattlePhase = BattlePhase::PlayerWindup;
		targetDamageApplied = false;
		playerDamageApplied = false;
		const glm::vec3 forward(-std::sin(mycam.yaw()), 0.0f,
		                        -std::cos(mycam.yaw()));
		battlePlayerOrigin = mypos + glm::vec3(0.0f, 0.9f, 0.0f) +
		                     forward * 0.75f;
		battleTargetPosition = pokemonWorldPosition(*target);
		battleTargetPosition.y += target->isFlying() ? 0.0f : 0.52f;
		battlePlayerHitPosition = mypos + glm::vec3(0.0f, 0.82f, 0.0f);
		currentTarget = PokemonTargetSelection();
		setStatus("Charizard readies " + std::string(pendingPlayerMove.name) +
		          " against " + pendingBattleSpecies + ".");
	}

	void updatePokemonAgents(double deltaSeconds, double now)
	{
		if (gameFinished)
		{
			return;
		}
		const CaptureSequenceSample captureSample = currentCaptureSample(now);
		for (int i = 0; i < NUM_POKEMON; ++i)
		{
			if (isPendingBattleTarget(umbreons[i]) ||
			    (isPendingCaptureTarget(umbreons[i]) &&
			     captureSample.phase != CapturePhase::BrokeFree))
			{
				continue;
			}
			umbreons[i].update(deltaSeconds, mypos);
		}
		for (int i = 0; i < FLYING_POKEMON; ++i)
		{
			if (isPendingBattleTarget(charizards[i]) ||
			    (isPendingCaptureTarget(charizards[i]) &&
			     captureSample.phase != CapturePhase::BrokeFree))
			{
				continue;
			}
			charizards[i].update(deltaSeconds, mypos);
		}
	}

	void refreshTarget()
	{
		if (captureSequenceActive || battleSequenceActive)
		{
			currentTarget = PokemonTargetSelection();
			return;
		}
		std::vector<PokemonTargetCandidate> candidates;
		candidates.reserve(NUM_POKEMON + FLYING_POKEMON);
		for (int i = 0; i < NUM_POKEMON; ++i)
		{
			PokemonTargetCandidate candidate;
			candidate.index = i;
			candidate.caught = umbreons[i].getCaught() != 0 ||
			                   umbreons[i].isFainted();
			candidate.position = pokemonWorldPosition(umbreons[i]);
			candidates.push_back(candidate);
		}
		for (int i = 0; i < FLYING_POKEMON; ++i)
		{
			PokemonTargetCandidate candidate;
			candidate.index = i;
			candidate.flying = true;
			candidate.caught = charizards[i].getCaught() != 0 ||
			                   charizards[i].isFainted();
			candidate.position = pokemonWorldPosition(charizards[i]);
			candidates.push_back(candidate);
		}
		currentTarget = selectPokemonTarget(mypos, mycam.yaw(), candidates);
	}

	Pokemon *targetedPokemon()
	{
		if (!currentTarget.valid())
		{
			return nullptr;
		}
		if (currentTarget.flying)
		{
			return currentTarget.index < FLYING_POKEMON ? &charizards[currentTarget.index] : nullptr;
		}
		return currentTarget.index < NUM_POKEMON ? &umbreons[currentTarget.index] : nullptr;
	}

	void updateWebTelemetry()
	{
#ifdef __EMSCRIPTEN__
		const double now = glfwGetTime();
		if (now < nextTelemetryUpdate)
		{
			return;
		}
		nextTelemetryUpdate = now + 0.12;
		std::ostringstream telemetry;
		if (battleSequenceActive)
		{
			const BattleSequenceSample sample = currentBattleSample(now);
			telemetry << pendingBattleSpecies << " · ";
			switch (sample.phase)
			{
			case BattlePhase::PlayerWindup:
				telemetry << "Charging Ember";
				break;
			case BattlePhase::PlayerProjectile:
				telemetry << "Ember airborne";
				break;
			case BattlePhase::TargetImpact:
				telemetry << "Target hit";
				break;
			case BattlePhase::WildWindup:
				telemetry << "Countering";
				break;
			case BattlePhase::WildProjectile:
				telemetry << pendingWildMove.name;
				break;
			case BattlePhase::PlayerImpact:
				telemetry << "Charizard hit";
				break;
			case BattlePhase::Recovery:
				telemetry << "Recovering";
				break;
			case BattlePhase::Inactive:
			case BattlePhase::Finished:
				telemetry << "Resolving";
				break;
			}
			if (pendingBattleTarget)
			{
				telemetry << " · Enemy HP " << pendingBattleTarget->getHealth()
				          << "/" << pendingBattleTarget->getMaximumHealth();
			}
		}
		else if (captureSequenceActive)
		{
			const CaptureSequenceSample sample = currentCaptureSample(now);
			telemetry << pendingCaptureSpecies << " · ";
			switch (sample.phase)
			{
			case CapturePhase::Throwing:
				telemetry << "Ball airborne";
				break;
			case CapturePhase::Absorbing:
				telemetry << "Hit";
				break;
			case CapturePhase::Shaking:
				telemetry << "Shake " << sample.shakeIndex;
				break;
			case CapturePhase::Succeeded:
				telemetry << "Captured";
				break;
			case CapturePhase::BrokeFree:
				telemetry << "Broke free";
				break;
			case CapturePhase::Inactive:
			case CapturePhase::Finished:
				telemetry << "Resolving";
				break;
			}
		}
		else if (Pokemon *target = targetedPokemon())
		{
			telemetry << pokemonSpeciesName(target->getSpecies()) << " "
			          << std::fixed << std::setprecision(1)
			          << currentTarget.distance << "m · HP "
			          << target->getHealth() << "/" << target->getMaximumHealth();
		}
		else
		{
			telemetry << "No target";
		}
		telemetry << " · " << (mycam.gravityEnabled() ? "Gravity" : "Hover")
		          << " · " << (mycam.grounded() ? "Grounded" : "Airborne")
		          << " · Charizard HP " << playerHealth << "/"
		          << battleStatsFor(PokemonSpecies::Charizard).maximumHealth;
		const std::string text = telemetry.str();
		EM_ASM({
			if (Module.onGameTelemetry)
			{
				Module.onGameTelemetry(UTF8ToString($0));
			}
		}, text.c_str());

		Pokemon *hudTarget = nullptr;
		if (battleSequenceActive)
		{
			hudTarget = pendingBattleTarget;
		}
		else if (captureSequenceActive)
		{
			hudTarget = pendingCaptureTarget;
		}
		else
		{
			hudTarget = targetedPokemon();
		}
		const bool targetVisible = hudTarget != nullptr;
		const std::string targetName = targetVisible
		                                   ? pokemonSpeciesName(hudTarget->getSpecies())
		                                   : std::string();
		const int targetHealth = targetVisible ? hudTarget->getHealth() : 0;
		const int targetMaximum = targetVisible
		                                  ? hudTarget->getMaximumHealth()
		                                  : 1;
		const int playerMaximum =
			battleStatsFor(PokemonSpecies::Charizard).maximumHealth;
		EM_ASM({
			if (Module.onBattleHud)
			{
				Module.onBattleHud($0, $1, UTF8ToString($2), $3, $4, $5 !== 0);
			}
		}, playerHealth, playerMaximum, targetName.c_str(), targetHealth,
		   targetMaximum, targetVisible ? 1 : 0);

		const auto &moves = playerBattleMoves();
		const bool moveInputBusy =
			battleSequenceActive || captureSequenceActive || gameFinished;
		for (int slot = 0; slot < PLAYER_MOVE_SLOT_COUNT; ++slot)
		{
			const BattleMove &move = moves[static_cast<std::size_t>(slot)];
			const double remaining =
				playerMoveLoadout.cooldownRemaining(slot, now);
			const float fraction =
				playerMoveLoadout.cooldownFraction(slot, now);
			EM_ASM({
				if (Module.onMoveSlot)
				{
					Module.onMoveSlot(
						$0, UTF8ToString($1), $2, $3, $4, $5,
						$6 !== 0, $7 !== 0);
				}
			}, slot, move.name, static_cast<int>(move.type), move.power,
			   remaining, fraction,
			   slot == playerMoveLoadout.selectedSlot() ? 1 : 0,
			   moveInputBusy ? 1 : 0);
		}

		const ResearchMissionSnapshot mission = makeResearchMissionSnapshot(
			caughtCount, defeatedCount, researchProgress, CAPTURE_GOAL);
		EM_ASM({
			if (Module.onMissionProgress)
			{
				Module.onMissionProgress(
					$0, $1, $2, $3, $4, $5, $6, $7, $8);
			}
		}, mission.objectives[0].current, mission.objectives[0].target,
		   mission.objectives[1].current, mission.objectives[1].target,
		   mission.objectives[2].current, mission.objectives[2].target,
		   mission.objectives[3].current, mission.objectives[3].target,
		   mission.completedObjectives());
#endif
	}

	void captureNearestPokemon()
	{
		if (battleSequenceActive)
		{
			setStatus("Finish the battle exchange before throwing a Poke Ball.");
			return;
		}
		if (captureSequenceActive)
		{
			setStatus("A capture attempt is already in progress.");
			return;
		}
		if (gameFinished)
		{
			return;
		}
		if (pokeballs <= 0)
		{
			gameFinished = true;
			setStatus("Out of Poke Balls. Press R twice to try again.");
			return;
		}

		Pokemon *target = targetedPokemon();
		if (!target)
		{
			setStatus("No target. Face a Pokemon and move closer.");
			return;
		}
		const std::string speciesName = pokemonSpeciesName(target->getSpecies());
		const float captureRange = currentTarget.flying
		                               ? FLYING_TARGETING_RANGE
		                               : GROUND_TARGETING_RANGE;
		if (currentTarget.distance > captureRange)
		{
			std::ostringstream message;
			message << speciesName << " locked at " << std::fixed << std::setprecision(1)
			        << currentTarget.distance << "m. Move closer.";
			setStatus(message.str());
			return;
		}

		CaptureAttempt attempt;
		attempt.species = target->getSpecies();
		attempt.distance = currentTarget.distance;
		attempt.maximumDistance = captureRange;
		attempt.alignment = currentTarget.alignment;
		attempt.healthRatio = target->getHealthRatio();
		attempt.activity = captureActivityFor(*target);
		pendingCaptureResult = resolveCaptureAttempt(attempt, captureRandom.nextUnit());
		pendingCaptureTarget = target;
		pendingCaptureSpecies = speciesName;
		captureSequenceActive = true;
		captureSequenceStarted = glfwGetTime();
		lastCapturePhase = CapturePhase::Throwing;
		lastCaptureShake = 0;
		const glm::vec3 throwForward(-std::sin(mycam.yaw()), 0.0f,
		                             -std::cos(mycam.yaw()));
		captureThrowStart = mypos + glm::vec3(0.0f, 0.9f, 0.0f) +
		                    throwForward * 0.55f;
		captureHitPosition = pokemonWorldPosition(*target);
		captureHitPosition.y += target->isFlying() ? 0.0f : 0.45f;
		captureBallRestPosition = glm::vec3(
			captureHitPosition.x,
			terrainHeightMap.heightAt(captureHitPosition.x, captureHitPosition.z) + 0.18f,
			captureHitPosition.z);
		--pokeballs;
		currentTarget = PokemonTargetSelection();
		std::ostringstream message;
		message << "Poke Ball away at " << speciesName << " - "
		        << static_cast<int>(std::round(pendingCaptureResult.probability * 100.0f))
		        << "% capture chance.";
		setStatus(message.str());
		emitGameCue("capture-throw");
		saveGameProgress();
	}

	glm::vec3 articulatedPartPivot(const Shape::PartInfo &part) const
	{
		glm::vec3 pivot = (part.minimum + part.maximum) * 0.5f;
		if (part.name.find("leg-") != std::string::npos)
		{
			pivot.y = part.maximum.y;
		}
		else if (part.name.find("wing") != std::string::npos)
		{
			pivot.x = std::fabs(part.minimum.x) < std::fabs(part.maximum.x)
			              ? part.minimum.x
			              : part.maximum.x;
		}
		else if (part.name.find("tail") != std::string::npos)
		{
			pivot.z = std::fabs(part.minimum.z) < std::fabs(part.maximum.z)
			              ? part.minimum.z
			              : part.maximum.z;
		}
		return pivot;
	}

	void drawArticulatedShape(const std::shared_ptr<Shape> &creature,
	                         const glm::mat4 &rootTransform,
	                         const PokemonAnimationPose &pose,
	                         bool useExternalTextures)
	{
		for (int partIndex = 0; partIndex < creature->partCount(); ++partIndex)
		{
			const Shape::PartInfo &part = creature->partInfo(partIndex);
			const PokemonPartAnimation animation =
				samplePokemonPartAnimation(part.name, pose);
			const glm::vec3 pivot = articulatedPartPivot(part);
			glm::mat4 local = glm::translate(glm::mat4(1.0f), pivot);
			local = local * glm::rotate(glm::mat4(1.0f), animation.pitch,
			                           glm::vec3(1.0f, 0.0f, 0.0f));
			local = local * glm::rotate(glm::mat4(1.0f), animation.yaw,
			                           glm::vec3(0.0f, 1.0f, 0.0f));
			local = local * glm::rotate(glm::mat4(1.0f), animation.roll,
			                           glm::vec3(0.0f, 0.0f, 1.0f));
			local = local * glm::translate(glm::mat4(1.0f), -pivot);
			const glm::mat4 modelMatrix = rootTransform * local;
			glUniformMatrix4fv(pokemon2->getUniform("M"), 1, GL_FALSE,
			                   &modelMatrix[0][0]);
			creature->drawPart(pokemon2, partIndex, useExternalTextures);
		}
	}

	GLuint bulbasaurTextureForPart(const std::string &partName) const
	{
		if (partName.find("bulb-main") != std::string::npos)
		{
			return bulbasaurBulbTex;
		}
		if (partName.find("bulb-leaves") != std::string::npos)
		{
			return bulbasaurLeafTex;
		}
		if (partName.find("spots") != std::string::npos)
		{
			return bulbasaurSpotTex;
		}
		if (partName.find("eye-white") != std::string::npos ||
		    partName.find("eye-highlight") != std::string::npos)
		{
			return bulbasaurEyeWhiteTex;
		}
		if (partName.find("eye-iris") != std::string::npos)
		{
			return bulbasaurEyeRedTex;
		}
		if (partName.find("eye-dark") != std::string::npos)
		{
			return bulbasaurEyeDarkTex;
		}
		return bulbasaurBodyTex;
	}

	void drawBulbasaur(const glm::mat4 &rootTransform,
	                  const PokemonAnimationPose &pose)
	{
		for (int partIndex = 0; partIndex < bulbasaur->partCount(); ++partIndex)
		{
			const Shape::PartInfo &part = bulbasaur->partInfo(partIndex);
			PokemonPartAnimation animation =
				samplePokemonPartAnimation(part.name, pose);
			if (part.name.find("bulb-") != std::string::npos)
			{
				animation.yaw += pose.tailAngle * 0.45f;
				animation.roll += pose.strideAngle * 0.025f;
			}

			const glm::vec3 pivot = articulatedPartPivot(part);
			glm::mat4 local = glm::translate(glm::mat4(1.0f), pivot);
			local = local * glm::rotate(glm::mat4(1.0f), animation.pitch,
			                           glm::vec3(1.0f, 0.0f, 0.0f));
			local = local * glm::rotate(glm::mat4(1.0f), animation.yaw,
			                           glm::vec3(0.0f, 1.0f, 0.0f));
			local = local * glm::rotate(glm::mat4(1.0f), animation.roll,
			                           glm::vec3(0.0f, 0.0f, 1.0f));
			local = local * glm::translate(glm::mat4(1.0f), -pivot);
			const glm::mat4 modelMatrix = rootTransform * local;
			glUniformMatrix4fv(pokemon2->getUniform("M"), 1, GL_FALSE,
			                   &modelMatrix[0][0]);
			glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_2D, bulbasaurTextureForPart(part.name));
			bulbasaur->drawPart(pokemon2, partIndex, true);
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
		if (key == GLFW_KEY_X && action == GLFW_PRESS)
		{
			attackRequested = true;
		}
		if (key >= GLFW_KEY_1 && key <= GLFW_KEY_3 && action == GLFW_PRESS)
		{
			selectPlayerMove(key - GLFW_KEY_1);
		}
		if (key == GLFW_KEY_R && action == GLFW_PRESS)
		{
			const double now = glfwGetTime();
			if (now <= resetConfirmationExpires)
			{
				resetRequested = true;
				resetConfirmationExpires = -100.0;
			}
			else
			{
				resetConfirmationExpires = now + 3.0;
				setStatus("Press R again within 3 seconds to start a new run and replace autosave.");
			}
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

		bulbasaur = make_shared<Shape>();
		bulbasaur->loadMesh(resourceDirectory + "/pokemon/bulbasaur.obj");
		if (bulbasaur->partCount() < 8)
		{
			std::cerr << "Unable to load the articulated Bulbasaur model." << std::endl;
			exit(1);
		}
		bulbasaur->resize();
		bulbasaur->init();

		bulbasaurBodyTex = createSolidTexture(73, 177, 158);
		bulbasaurSpotTex = createSolidTexture(38, 119, 106);
		bulbasaurBulbTex = createSolidTexture(49, 126, 63);
		bulbasaurLeafTex = createSolidTexture(94, 173, 76);
		bulbasaurEyeWhiteTex = createSolidTexture(239, 244, 226);
		bulbasaurEyeRedTex = createSolidTexture(186, 48, 58);
		bulbasaurEyeDarkTex = createSolidTexture(42, 31, 43);

		string str1 = resourceDirectory + "/pokemon";
		charizard = make_shared<Shape>();
		charizard->loadMesh(resourceDirectory + "/pokemon/charizard.obj", &str1);
		charizard->resize();
		charizard->init();


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
		if (!data || !terrainHeightMap.setPixels(width, height, data, 4))
		{
			std::cerr << "Unable to load terrain height map: " << str << std::endl;
			exit(1);
		}
		glGenTextures(1, &HeightTex);
		glActiveTexture(GL_TEXTURE1);
		glBindTexture(GL_TEXTURE_2D, HeightTex);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
		glGenerateMipmap(GL_TEXTURE_2D);
		stbi_image_free(data);
		data = nullptr;
		mycam.setGroundHeightProvider([this](float worldX, float worldZ) {
			return terrainHeightMap.heightAt(worldX, worldZ);
		});
		std::vector<StaticCollisionCylinder> rockColliders;
		rockColliders.reserve(ROCK_PLACEMENTS.size());
		for (const RockPlacement &rock : ROCK_PLACEMENTS)
		{
			StaticCollisionCylinder collider;
			collider.center = rock.center;
			collider.radius = std::max(rock.scale.x, rock.scale.z) * 1.18f;
			collider.baseY = terrainHeightMap.heightAt(rock.center.x, rock.center.y);
			collider.height = rock.scale.y * 2.36f;
			rockColliders.push_back(collider);
		}
		mycam.setStaticObstacles(std::move(rockColliders));

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

		// texture 8: stone surface used by the visible collision boulders.
		str = resourceDirectory + "/Texture/gray.png";
		strcpy(filepath, str.c_str());
		data = stbi_load(filepath, &width, &height, &channels, 4);
		if (!data)
		{
			std::cerr << "Unable to load rock texture: " << str << std::endl;
			exit(1);
		}
		glGenTextures(1, &rockTex);
		glActiveTexture(GL_TEXTURE1);
		glBindTexture(GL_TEXTURE_2D, rockTex);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
		glGenerateMipmap(GL_TEXTURE_2D);
		stbi_image_free(data);
		data = nullptr;

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

		// texture 9: the original Umbreon UV atlas.
		str = resourceDirectory + "/Texture/umbreon.png";
		strcpy(filepath, str.c_str());
		data = stbi_load(filepath, &width, &height, &channels, 4);
		if (!data)
		{
			std::cerr << "Unable to load Umbreon texture: " << str << std::endl;
			exit(1);
		}
		glGenTextures(1, &umbreonTex);
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, umbreonTex);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA,
		             GL_UNSIGNED_BYTE, data);
		glGenerateMipmap(GL_TEXTURE_2D);
		stbi_image_free(data);
		data = nullptr;

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
		restoreGameProgress();
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
		prog->addUniform("time");
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
		addSceneLightingUniforms(heightshader);
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
		pokemon->addUniform("animationMode");
		pokemon->addUniform("wingAngle");
		pokemon->addUniform("tailAngle");
		pokemon->addUniform("breathingScale");
		addSceneLightingUniforms(pokemon);
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
		pokemon2->addUniform("surfaceDeform");
		pokemon2->addUniform("animationMode");
		pokemon2->addUniform("wingAngle");
		pokemon2->addUniform("tailAngle");
		pokemon2->addUniform("breathingScale");
		addSceneLightingUniforms(pokemon2);
		pokemon2->addAttribute("vertPos");
		pokemon2->addAttribute("vertTex");
		pokemon2->addAttribute("vertNor");

		targetshader = std::make_shared<Program>();
		targetshader->setVerbose(true);
		targetshader->setShaderNames(resourceDirectory + "/target_vertex.glsl",
		                             resourceDirectory + "/target_frag.glsl");
		if (!targetshader->init())
		{
			std::cerr << "Target indicator shaders failed to compile... exiting!" << std::endl;
			exit(1);
		}
		targetshader->addUniform("P");
		targetshader->addUniform("V");
		targetshader->addUniform("M");
		targetshader->addUniform("time");
		targetshader->addUniform("ringColor");
		targetshader->addUniform("opacity");
		targetshader->addUniform("fillAmount");
		targetshader->addAttribute("vertPos");
		targetshader->addAttribute("vertTex");

		pokeballShader = std::make_shared<Program>();
		pokeballShader->setVerbose(true);
		pokeballShader->setShaderNames(resourceDirectory + "/pokeball_vertex.glsl",
		                               resourceDirectory + "/pokeball_frag.glsl");
		if (!pokeballShader->init())
		{
			std::cerr << "Poke Ball shaders failed to compile... exiting!" << std::endl;
			exit(1);
		}
		pokeballShader->addUniform("P");
		pokeballShader->addUniform("V");
		pokeballShader->addUniform("M");
		addSceneLightingUniforms(pokeballShader);
		pokeballShader->addAttribute("vertPos");
		pokeballShader->addAttribute("vertNor");
		pokeballShader->addAttribute("vertTex");

		battleEffectShader = std::make_shared<Program>();
		battleEffectShader->setVerbose(true);
		battleEffectShader->setShaderNames(
			resourceDirectory + "/battle_effect_vertex.glsl",
			resourceDirectory + "/battle_effect_frag.glsl");
		if (!battleEffectShader->init())
		{
			std::cerr << "Battle effect shaders failed to compile... exiting!"
			          << std::endl;
			exit(1);
		}
		battleEffectShader->addUniform("P");
		battleEffectShader->addUniform("V");
		battleEffectShader->addUniform("M");
		battleEffectShader->addUniform("time");
		battleEffectShader->addUniform("effectColor");
		battleEffectShader->addUniform("coreColor");
		battleEffectShader->addUniform("opacity");
		battleEffectShader->addUniform("shellAmount");
		battleEffectShader->addAttribute("vertPos");
		battleEffectShader->addAttribute("vertNor");
		battleEffectShader->addAttribute("vertTex");
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
		if (captureSequenceActive || battleSequenceActive)
		{
			mycam.w = mycam.a = mycam.s = mycam.d = 0;
			mycam.q = mycam.e = mycam.space = 0;
		}
		glm::mat4 playerView = mycam.process(frametime);
		const glm::vec3 playerVelocity = mycam.velocity();
		const float playerSpeedRatio = glm::clamp(
			glm::length(glm::vec2(playerVelocity.x, playerVelocity.z)) / 7.0f,
			0.0f, 1.0f);
		playerAnimationPhase = advancePokemonAnimationPhase(
			playerAnimationPhase, static_cast<float>(frametime), true, false,
			playerSpeedRatio);
		PokemonAnimationInput playerAnimationInput;
		playerAnimationInput.flying = true;
		playerAnimationInput.speedRatio = playerSpeedRatio;
		playerAnimationInput.verticalSpeedRatio =
			glm::clamp(playerVelocity.y / 9.0f, -1.0f, 1.0f);
		playerAnimationInput.turnRatio = mycam.turnRatio();
		playerAnimationInput.phase = playerAnimationPhase;
		PokemonAnimationPose playerPose =
			samplePokemonAnimation(playerAnimationInput);
		const PlayerMotionEvents &motionEvents = mycam.motionEvents();
		if (motionEvents.hitCeiling)
		{
			setStatus("Maximum flight altitude reached.");
		}
		else if (motionEvents.landed)
		{
			recordSafeLanding(researchProgress);
			emitGameCue("land");
			setStatus("Landed safely.");
			saveGameProgress();
		}
		else if (motionEvents.hitBoundary)
		{
			setStatus("Field boundary reached.");
		}
		else if (motionEvents.hitObstacle)
		{
			setStatus("A boulder blocks the path.");
		}
		const double actionNow = glfwGetTime();
		updateBattleSequence(actionNow);
		updateCaptureSequence(actionNow);
		updatePokemonAgents(frametime, actionNow);
		refreshTarget();
		if (attackRequested)
		{
			attackTargetedPokemon();
			attackRequested = false;
		}
		if (captureRequested)
		{
			captureNearestPokemon();
			captureRequested = false;
		}
		updateWebTelemetry();
		const double captureRenderNow = glfwGetTime();
		const CaptureSequenceSample captureVisualSample =
			currentCaptureSample(captureRenderNow);
		const BattleSequenceSample battleVisualSample =
			currentBattleSample(captureRenderNow);
		if (battleSequenceActive)
		{
			applyPlayerBattlePose(playerPose, battleVisualSample,
			                      pendingPlayerMove);
		}

		// Keep the sky centered on the camera and let only its orientation follow
		// the player's view. Cloud motion is handled slowly in the sky shader.
		float trans = 0;
		float angle = -3.1415926 / 2.0;
		glm::mat4 RotateX = glm::rotate(glm::mat4(1.0f), angle, glm::vec3(1.0f, 0.0f, 0.0f));
		glm::mat4 TransZ = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, -3 + trans));
		glm::mat4 S = glm::scale(glm::mat4(1.0f), glm::vec3(0.8f, 0.8f, 0.8f));
		S = glm::scale(glm::mat4(1.0f), glm::vec3(100.0f, 100.0f, 100.0f));
		M = RotateX * S;

		// Draw the box using GLSL.

		prog->bind();

		V = glm::mat4(glm::mat3(playerView));
		glUniformMatrix4fv(prog->getUniform("P"), 1, GL_FALSE, &P[0][0]);
		glUniformMatrix4fv(prog->getUniform("V"), 1, GL_FALSE, &V[0][0]);
		glUniformMatrix4fv(prog->getUniform("M"), 1, GL_FALSE, &M[0][0]);
		glUniform1f(prog->getUniform("time"), static_cast<float>(glfwGetTime()));
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
		applySceneLighting(heightshader, V);

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

		targetshader->bind();
		V = playerView;
		glUniformMatrix4fv(targetshader->getUniform("P"), 1, GL_FALSE, &P[0][0]);
		glUniformMatrix4fv(targetshader->getUniform("V"), 1, GL_FALSE, &V[0][0]);
		const float indicatorTime = static_cast<float>(captureRenderNow);
		glUniform1f(targetshader->getUniform("time"), indicatorTime);
		glBindVertexArray(VertexArrayID2);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, IndexBufferIDBox2);
		glDepthMask(GL_FALSE);
		auto drawPlanarEffect = [&](const glm::vec3 &position,
		                            const glm::vec2 &dimensions,
		                            const glm::vec3 &effectColor, float opacity,
		                            float fillAmount) {
			const glm::mat4 translation =
				glm::translate(glm::mat4(1.0f), position);
			const glm::mat4 rotation = glm::rotate(
				glm::mat4(1.0f), -1.5707963f, glm::vec3(1.0f, 0.0f, 0.0f));
			const glm::mat4 scale = glm::scale(
				glm::mat4(1.0f), glm::vec3(dimensions.x, dimensions.y, 1.0f));
			const glm::mat4 centerQuad = glm::translate(
				glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, -0.5f));
			const glm::mat4 model = translation * rotation * scale * centerQuad;
			glUniformMatrix4fv(targetshader->getUniform("M"), 1, GL_FALSE,
			                   &model[0][0]);
			glUniform3fv(targetshader->getUniform("ringColor"), 1,
			             &effectColor[0]);
			glUniform1f(targetshader->getUniform("opacity"), opacity);
			glUniform1f(targetshader->getUniform("fillAmount"), fillAmount);
			glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, (void *)0);
		};
		auto drawTargetRing = [&](const glm::vec3 &position, float diameter,
		                          const glm::vec3 &ringColor, float opacity) {
			drawPlanarEffect(position, glm::vec2(diameter), ringColor, opacity, 0.0f);
		};
		auto drawGroundShadow = [&](const glm::vec3 &position,
		                           const glm::vec2 &dimensions, float opacity) {
			drawPlanarEffect(position, dimensions,
			                 glm::vec3(0.025f, 0.065f, 0.095f), opacity, 1.0f);
		};

		const float playerGroundHeight =
			terrainHeightMap.heightAt(mypos.x, mypos.z);
		const float playerAltitude = std::max(0.0f, mypos.y - playerGroundHeight);
		const float playerShadowGrowth =
			glm::clamp(1.0f + playerAltitude * 0.07f, 1.0f, 2.8f);
		const float playerShadowOpacity =
			glm::clamp(0.34f - playerAltitude * 0.012f, 0.07f, 0.34f);
		drawGroundShadow(
			glm::vec3(mypos.x, playerGroundHeight + 0.035f, mypos.z),
			glm::vec2(1.75f, 1.28f) * playerShadowGrowth, playerShadowOpacity);

		for (int i = 0; i < NUM_POKEMON; ++i)
		{
			if (umbreons[i].getCaught() == 1 || umbreons[i].isFainted() ||
			    (isPendingCaptureTarget(umbreons[i]) &&
			     !captureVisualSample.pokemonVisible))
			{
				continue;
			}
			const glm::vec3 position = umbreons[i].getPos();
			if (glm::length(glm::vec2(mypos.x - position.x,
			                          mypos.z - position.z)) > 50.0f)
			{
				continue;
			}
			const bool isUmbreon =
				umbreons[i].getSpecies() == PokemonSpecies::Umbreon;
			drawGroundShadow(
				glm::vec3(position.x,
				          terrainHeightMap.heightAt(position.x, position.z) + 0.032f,
				          position.z),
				isUmbreon ? glm::vec2(1.32f, 0.88f)
				           : glm::vec2(1.45f, 1.05f),
				0.25f);
		}

		for (int i = 0; i < FLYING_POKEMON; ++i)
		{
			if (charizards[i].getCaught() == 1 || charizards[i].isFainted() ||
			    (isPendingCaptureTarget(charizards[i]) &&
			     !captureVisualSample.pokemonVisible))
			{
				continue;
			}
			const glm::vec3 position = charizards[i].getPos();
			if (glm::length(glm::vec2(mypos.x - position.x,
			                          mypos.z - position.z)) > 100.0f)
			{
				continue;
			}
			const float groundHeight =
				terrainHeightMap.heightAt(position.x, position.z);
			const float altitude = std::max(0.0f, position.y - groundHeight);
			const float growth = glm::clamp(1.0f + altitude * 0.045f, 1.0f, 2.5f);
			const float opacity =
				glm::clamp(0.24f - altitude * 0.006f, 0.055f, 0.20f);
			drawGroundShadow(glm::vec3(position.x, groundHeight + 0.038f, position.z),
			                 glm::vec2(2.45f, 1.42f) * growth, opacity);
		}

		Pokemon *lockedPokemon = targetedPokemon();
		if (lockedPokemon && lockedPokemon->getCaught() == 0)
		{
			glm::vec3 targetPosition = pokemonWorldPosition(*lockedPokemon);
			targetPosition.y += currentTarget.flying ? -0.65f : 0.08f;
			const float targetDiameter = currentTarget.flying ? 3.2f : 2.35f;
			drawTargetRing(targetPosition, targetDiameter,
			               glm::vec3(0.18f, 0.82f, 1.0f), 0.88f);
		}
		if (battleSequenceActive)
		{
			const BattleEffectPalette playerPalette =
				battleEffectPalette(pendingPlayerMove.type);
			const BattleEffectPalette wildPalette =
				battleEffectPalette(pendingWildMove.type);
			const float playerVisualScale =
				battleMoveVisualScale(pendingPlayerMove.id);
			glm::vec3 playerRingPosition(
				mypos.x, terrainHeightMap.heightAt(mypos.x, mypos.z) + 0.055f,
				mypos.z);
			glm::vec3 targetRingPosition = battleTargetPosition;
			if (pendingBattleTarget && pendingBattleTarget->isFlying())
			{
				targetRingPosition.y -= 0.72f;
			}
			else
			{
				targetRingPosition.y = terrainHeightMap.heightAt(
					targetRingPosition.x, targetRingPosition.z) + 0.055f;
			}

			if (battleVisualSample.phase == BattlePhase::PlayerWindup)
			{
				const float progress = easedBattleProgress(
					battleVisualSample.phaseProgress);
				drawTargetRing(playerRingPosition,
				               (0.72f + progress * 0.64f) * playerVisualScale,
				               playerPalette.effectColor, 0.28f + progress * 0.32f);
			}
			else if (battleVisualSample.phase == BattlePhase::TargetImpact)
			{
				const float progress = battleVisualSample.phaseProgress;
				drawTargetRing(targetRingPosition,
				               (0.86f + progress * 2.65f) * playerVisualScale,
				               playerPalette.effectColor,
				               (1.0f - progress) * 0.86f);
			}
			else if (battleVisualSample.phase == BattlePhase::WildWindup)
			{
				const float progress = easedBattleProgress(
					battleVisualSample.phaseProgress);
				drawTargetRing(targetRingPosition, 0.82f + progress * 0.74f,
				               wildPalette.effectColor, 0.25f + progress * 0.38f);
			}
			else if (battleVisualSample.phase == BattlePhase::PlayerImpact)
			{
				const float progress = battleVisualSample.phaseProgress;
				drawTargetRing(playerRingPosition, 0.92f + progress * 2.90f,
				               wildPalette.effectColor,
				               (1.0f - progress) * 0.82f);
			}
		}
		if (captureSequenceActive && captureVisualSample.ballVisible)
		{
			const CaptureBallVisualPose ballPose =
				captureBallVisualPose(captureVisualSample);
			glm::vec3 ballIndicatorPosition = ballPose.position;
			glm::vec3 ballIndicatorColor(1.0f, 0.68f, 0.16f);
			float ballIndicatorDiameter = 0.58f;
			if (captureVisualSample.phase == CapturePhase::Throwing)
			{
				ballIndicatorColor = glm::vec3(1.0f, 0.34f, 0.22f);
				ballIndicatorDiameter = 0.46f;
			}
			else if (captureVisualSample.phase == CapturePhase::Absorbing)
			{
				ballIndicatorColor = glm::vec3(0.22f, 0.86f, 1.0f);
				ballIndicatorDiameter =
					0.58f + std::sin(captureVisualSample.phaseProgress * 3.1415926f) * 0.72f;
			}
			else if (captureVisualSample.phase == CapturePhase::Shaking)
			{
				ballIndicatorColor = glm::vec3(1.0f, 0.76f, 0.20f);
			}
			else if (captureVisualSample.phase == CapturePhase::Succeeded)
			{
				ballIndicatorColor = glm::vec3(0.34f, 1.0f, 0.48f);
				ballIndicatorDiameter += captureVisualSample.phaseProgress * 0.28f;
			}
			drawTargetRing(ballIndicatorPosition, ballIndicatorDiameter,
			               ballIndicatorColor, 0.95f);
		}
		const float captureEffectAge = indicatorTime - static_cast<float>(captureEffectStarted);
		if (captureEffectAge >= 0.0f && captureEffectAge < 0.9f)
		{
			glm::vec3 effectPosition = captureEffectPosition + glm::vec3(0.0f, 0.12f, 0.0f);
			const float effectProgress = captureEffectAge / 0.9f;
			const glm::vec3 effectColor = captureEffectSucceeded
			                                  ? glm::vec3(0.32f, 1.0f, 0.48f)
			                                  : glm::vec3(1.0f, 0.25f, 0.16f);
			drawTargetRing(effectPosition, 2.0f + effectProgress * 4.2f,
			               effectColor, 1.0f - effectProgress);
		}
		glDepthMask(GL_TRUE);
		targetshader->unbind();

		if (captureSequenceActive && captureVisualSample.ballVisible)
		{
			const CaptureBallVisualPose ballPose =
				captureBallVisualPose(captureVisualSample);
			const glm::vec3 toPlayer = mypos - ballPose.position;
			const float faceYaw = std::atan2(toPlayer.x, toPlayer.z);
			const glm::mat4 ballTranslation =
				glm::translate(glm::mat4(1.0f), ballPose.position);
			const glm::mat4 ballFacing = glm::rotate(
				glm::mat4(1.0f), faceYaw, glm::vec3(0.0f, 1.0f, 0.0f));
			const glm::mat4 ballPitch = glm::rotate(
				glm::mat4(1.0f), ballPose.pitch, glm::vec3(1.0f, 0.0f, 0.0f));
			const glm::mat4 ballRoll = glm::rotate(
				glm::mat4(1.0f), ballPose.roll, glm::vec3(0.0f, 0.0f, 1.0f));
			const glm::mat4 ballScale = glm::scale(
				glm::mat4(1.0f), glm::vec3(ballPose.scale));
			const glm::mat4 ballModel =
				ballTranslation * ballFacing * ballPitch * ballRoll * ballScale;

			pokeballShader->bind();
			glUniformMatrix4fv(pokeballShader->getUniform("P"), 1, GL_FALSE, &P[0][0]);
			glUniformMatrix4fv(pokeballShader->getUniform("V"), 1, GL_FALSE, &playerView[0][0]);
			glUniformMatrix4fv(pokeballShader->getUniform("M"), 1, GL_FALSE,
			                   &ballModel[0][0]);
			applySceneLighting(pokeballShader, playerView);
			shape->draw(pokeballShader, false);
			pokeballShader->unbind();
		}

		prog2->bind();
		V = mat4(1);
		glUniformMatrix4fv(prog2->getUniform("P"), 1, GL_FALSE, &P[0][0]);
		glUniformMatrix4fv(prog2->getUniform("V"), 1, GL_FALSE, &V[0][0]);
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
		V = playerView;
		applySceneLighting(pokemon, V);
		S = glm::scale(glm::mat4(1.0f), glm::vec3(0.85f));
		mat4 T = glm::translate(glm::mat4(1.0f),
		                        mypos + glm::vec3(0.0f, 0.46f + playerPose.bodyBob, 0.0f));

		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, fireTex);
		glUniformMatrix4fv(pokemon->getUniform("P"), 1, GL_FALSE, &P[0][0]);
		glUniformMatrix4fv(pokemon->getUniform("V"), 1, GL_FALSE, &V[0][0]);
		mat4 R = glm::rotate(glm::mat4(1.0f), mycam.yaw() + 3.1415926f,
		                     glm::vec3(0.0f, 1.0f, 0.0f));
		mat4 PlayerPitch = glm::rotate(glm::mat4(1.0f), playerPose.bodyPitch,
		                               glm::vec3(1.0f, 0.0f, 0.0f));
		mat4 PlayerRoll = glm::rotate(glm::mat4(1.0f), playerPose.bodyRoll,
		                              glm::vec3(0.0f, 0.0f, 1.0f));
		M = T * R * PlayerPitch * PlayerRoll * S;
		glUniformMatrix4fv(pokemon->getUniform("M"), 1, GL_FALSE, &M[0][0]);
		applyCharizardAnimation(pokemon, playerPose, true);
		charizard->draw(pokemon, false);
		pokemon->unbind();
		/***main character***/

		pokemon2->bind();
		glUniformMatrix4fv(pokemon2->getUniform("P"), 1, GL_FALSE, &P[0][0]);
		V = playerView;
		glUniformMatrix4fv(pokemon2->getUniform("V"), 1, GL_FALSE, &V[0][0]);
		applySceneLighting(pokemon2, V);

		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, rockTex);
		glUniform1f(pokemon2->getUniform("surfaceDeform"), 0.18f);
		applyCharizardAnimation(pokemon2, PokemonAnimationPose(), false);
		for (const RockPlacement &rock : ROCK_PLACEMENTS)
		{
			const float groundHeight = terrainHeightMap.heightAt(rock.center.x, rock.center.y);
			T = glm::translate(glm::mat4(1.0f),
			                   glm::vec3(rock.center.x, groundHeight + rock.scale.y * 1.08f, rock.center.y));
			R = glm::rotate(glm::mat4(1.0f), rock.yaw, glm::vec3(0.0f, 1.0f, 0.0f));
			S = glm::scale(glm::mat4(1.0f), rock.scale);
			M = T * R * S;
			glUniformMatrix4fv(pokemon2->getUniform("M"), 1, GL_FALSE, &M[0][0]);
			shape->draw(pokemon2, false);
		}
		glUniform1f(pokemon2->getUniform("surfaceDeform"), 0.0f);

		// Ground Pokemon
		for (int i = 0; i < NUM_POKEMON; i++)
		{
			// if flag been caught, then don't draw, if too far, don't draw
			if (umbreons[i].getCaught() == 1 || umbreons[i].isFainted() ||
			    (isPendingCaptureTarget(umbreons[i]) &&
			     !captureVisualSample.pokemonVisible))
			{
				continue;
			}

			float distance = glm::length(glm::vec2(mypos.x - umbreons[i].getPos().x,
			                                      mypos.z - umbreons[i].getPos().z));

			if (distance > 50)
			{
				continue;
			}
			const float speedRatio = umbreons[i].getSpeedRatio();
			const bool fleeing = umbreons[i].getBehaviorState() == PokemonBehaviorState::Flee;
			PokemonAnimationInput animationInput;
			animationInput.fleeing = fleeing;
			animationInput.speedRatio = speedRatio;
			animationInput.phase = umbreons[i].getMotionPhase();
			PokemonAnimationPose pose = samplePokemonAnimation(animationInput);
			if (isPendingBattleTarget(umbreons[i]))
			{
				applyWildBattlePose(pose, battleVisualSample);
			}
			const bool renderUmbreon =
				umbreons[i].getSpecies() == PokemonSpecies::Umbreon;
			const float creatureScale = renderUmbreon ? 0.55f : 0.65f;
			const float groundOffset = renderUmbreon ? 0.34f : 0.47f;
			vec3 wildPosition = umbreons[i].getPos();
			wildPosition.y = terrainHeightMap.heightAt(wildPosition.x, wildPosition.z) +
			                 groundOffset + pose.bodyBob;
			T = glm::translate(glm::mat4(1.0f), wildPosition);
			R = glm::rotate(glm::mat4(1.0f), umbreons[i].getHeading(), glm::vec3(0.0f, 1.0f, 0.0f));
			mat4 Lean = glm::rotate(glm::mat4(1.0f), pose.bodyPitch,
			                        glm::vec3(1.0f, 0.0f, 0.0f));
			mat4 Breathing = glm::scale(glm::mat4(1.0f),
			                            glm::vec3(1.0f, pose.breathingScale, 1.0f));
			mat4 CreatureScale = glm::scale(glm::mat4(1.0f),
			                                glm::vec3(creatureScale));
			const mat4 creatureRoot = T * R * Lean * CreatureScale * Breathing;
			if (renderUmbreon)
			{
				glActiveTexture(GL_TEXTURE0);
				glBindTexture(GL_TEXTURE_2D, umbreonTex);
				drawArticulatedShape(umbreon, creatureRoot, pose, true);
			}
			else
			{
				drawBulbasaur(creatureRoot, pose);
			}
		}

		S = glm::scale(glm::mat4(1.0f), glm::vec3(1.6f, 1.6f, 1.6f));
		// charizard
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, fireTex);
		for (int i = 0; i < FLYING_POKEMON; i++)
		{
			if (charizards[i].getCaught() == 1 || charizards[i].isFainted() ||
			    (isPendingCaptureTarget(charizards[i]) &&
			     !captureVisualSample.pokemonVisible))
			{
				continue;
			}
			float distance = glm::length(glm::vec2(mypos.x - charizards[i].getPos().x,
			                                      mypos.z - charizards[i].getPos().z));
			if (distance > 100)
			{
				continue;
			}
			const float flightSpeedRatio = charizards[i].getSpeedRatio();
			PokemonAnimationInput flightAnimationInput;
			flightAnimationInput.flying = true;
			flightAnimationInput.fleeing =
				charizards[i].getBehaviorState() == PokemonBehaviorState::Flee;
			flightAnimationInput.speedRatio = flightSpeedRatio;
			flightAnimationInput.verticalSpeedRatio =
				glm::clamp(charizards[i].getVelocity().y / 4.8f, -1.0f, 1.0f);
			flightAnimationInput.phase = charizards[i].getMotionPhase();
			PokemonAnimationPose flightPose =
				samplePokemonAnimation(flightAnimationInput);
			if (isPendingBattleTarget(charizards[i]))
			{
				applyWildBattlePose(flightPose, battleVisualSample);
			}
			vec3 flightPosition = charizards[i].getPos();
			flightPosition.y += flightPose.bodyBob;
			T = glm::translate(glm::mat4(1.0f), flightPosition);
			R = glm::rotate(glm::mat4(1.0f), charizards[i].getHeading(), glm::vec3(0.0f, 1.0f, 0.0f));
			mat4 FlightPitch = glm::rotate(glm::mat4(1.0f), flightPose.bodyPitch,
			                               glm::vec3(1.0f, 0.0f, 0.0f));
			mat4 FlightRoll = glm::rotate(glm::mat4(1.0f), flightPose.bodyRoll,
			                              glm::vec3(0.0f, 0.0f, 1.0f));
			M = T * R * FlightPitch * FlightRoll * S;
			glUniformMatrix4fv(pokemon2->getUniform("M"), 1, GL_FALSE, &M[0][0]);
			applyCharizardAnimation(pokemon2, flightPose, true);
			charizard->draw(pokemon2, false);
		}

		pokemon2->unbind();

		if (battleSequenceActive)
		{
			const BattleEffectPalette playerPalette =
				battleEffectPalette(pendingPlayerMove.type);
			const BattleEffectPalette wildPalette =
				battleEffectPalette(pendingWildMove.type);
			const float playerVisualScale =
				battleMoveVisualScale(pendingPlayerMove.id);
			const float visualTime = static_cast<float>(captureRenderNow);
			const float pulse = 0.94f + std::sin(visualTime * 18.0f) * 0.06f;

			battleEffectShader->bind();
			glUniformMatrix4fv(battleEffectShader->getUniform("P"), 1, GL_FALSE,
			                   &P[0][0]);
			glUniformMatrix4fv(battleEffectShader->getUniform("V"), 1, GL_FALSE,
			                   &playerView[0][0]);
			glUniform1f(battleEffectShader->getUniform("time"), visualTime);
			glDepthMask(GL_FALSE);
			glBlendFunc(GL_SRC_ALPHA, GL_ONE);

			auto drawBattleOrb = [&](const glm::vec3 &position, float orbScale,
			                         const BattleEffectPalette &palette,
			                         float opacity, float shellAmount) {
				const glm::mat4 translation =
					glm::translate(glm::mat4(1.0f), position);
				const glm::mat4 scale = glm::scale(
					glm::mat4(1.0f), glm::vec3(std::max(0.01f, orbScale)));
				const glm::mat4 model = translation * scale;
				glUniformMatrix4fv(battleEffectShader->getUniform("M"), 1,
				                   GL_FALSE, &model[0][0]);
				glUniform3fv(battleEffectShader->getUniform("effectColor"), 1,
				             &palette.effectColor[0]);
				glUniform3fv(battleEffectShader->getUniform("coreColor"), 1,
				             &palette.coreColor[0]);
				glUniform1f(battleEffectShader->getUniform("opacity"),
				            glm::clamp(opacity, 0.0f, 1.0f));
				glUniform1f(battleEffectShader->getUniform("shellAmount"),
				            glm::clamp(shellAmount, 0.0f, 1.0f));
				shape->draw(battleEffectShader, false);
			};

			if (battleVisualSample.phase == BattlePhase::PlayerWindup)
			{
				const float progress = easedBattleProgress(
					battleVisualSample.phaseProgress);
				drawBattleOrb(battlePlayerOrigin,
				              (0.11f + progress * 0.13f) * pulse * playerVisualScale,
				              playerPalette, 0.78f, 0.06f);
			}
			else if (battleVisualSample.phase == BattlePhase::PlayerProjectile)
			{
				const int trailCount = battleMoveTrailCount(pendingPlayerMove.id);
				for (int trailIndex = trailCount - 1; trailIndex >= 0; --trailIndex)
				{
					const float delayedProgress = glm::clamp(
						battleVisualSample.phaseProgress -
						    static_cast<float>(trailIndex) * 0.055f,
						0.0f, 1.0f);
					if (trailIndex > 0 && delayedProgress <= 0.0f)
					{
						continue;
					}
					const glm::vec3 position = battleProjectilePosition(
						battlePlayerOrigin, battleTargetPosition, delayedProgress);
					const float trailScale =
						1.0f - static_cast<float>(trailIndex) * 0.09f;
					const float trailOpacity =
						0.92f - static_cast<float>(trailIndex) * 0.10f;
					drawBattleOrb(position,
					              0.245f * pulse * playerVisualScale * trailScale,
					              playerPalette, trailOpacity, 0.05f);
				}
			}
			else if (battleVisualSample.phase == BattlePhase::TargetImpact)
			{
				const float progress = easedBattleProgress(
					battleVisualSample.phaseProgress);
				drawBattleOrb(battleTargetPosition,
				              (0.30f + progress * 0.74f) * playerVisualScale,
				              playerPalette, (1.0f - progress) * 0.82f, 0.92f);
			}
			else if (battleVisualSample.phase == BattlePhase::WildWindup)
			{
				const float progress = easedBattleProgress(
					battleVisualSample.phaseProgress);
				drawBattleOrb(battleTargetPosition,
				              (0.10f + progress * 0.14f) * pulse, wildPalette,
				              0.76f, 0.08f);
			}
			else if (battleVisualSample.phase == BattlePhase::WildProjectile)
			{
				const glm::vec3 position = battleProjectilePosition(
					battleTargetPosition, battlePlayerHitPosition,
					battleVisualSample.phaseProgress);
				drawBattleOrb(position, 0.225f * pulse, wildPalette, 0.90f, 0.08f);
			}
			else if (battleVisualSample.phase == BattlePhase::PlayerImpact)
			{
				const float progress = easedBattleProgress(
					battleVisualSample.phaseProgress);
				drawBattleOrb(battlePlayerHitPosition, 0.28f + progress * 0.68f,
				              wildPalette, (1.0f - progress) * 0.78f, 0.94f);
			}

			glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
			glDepthMask(GL_TRUE);
			battleEffectShader->unbind();
		}
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
