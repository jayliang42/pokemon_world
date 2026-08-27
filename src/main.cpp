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
#include <limits>
#include <sstream>
#include <utility>
#include <vector>
#include "GLCompat.h"
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include "GLSL.h"
#include "AlphaNest.h"
#include "BattleMechanics.h"
#include "BattleMoveLoadout.h"
#include "BattleMoveVolume.h"
#include "BattleSequence.h"
#include "CaptureMechanics.h"
#include "CaptureProjectile.h"
#include "CaptureSequence.h"
#include "FieldRadar.h"
#include "FieldCamp.h"
#include "FieldLure.h"
#include "FrameCapture.h"
#include "GameSession.h"
#include "GameSave.h"
#include "GameSaveStorage.h"
#include "HudTelemetry.h"
#include "Program.h"
#include "MatrixStack.h"
#include "Pokemon.h"
#include "PokemonAnimation.h"
#include "PokemonGroupAlert.h"
#include "PokemonSightline.h"
#include "PokemonTargeting.h"
#include "PlayerController.h"
#include "ResearchMission.h"
#include "ResearchProgression.h"
#include "ResearchRunState.h"
#include "RegionalResearch.h"
#include "ResourceLocator.h"
#include "TerrainHeightMap.h"
#include "ThirdPersonCamera.h"
#include "WorldLighting.h"
#include "WorldLayout.h"
#include "WorldRegion.h"

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
shared_ptr<Shape> eevee;
shared_ptr<Shape> charizard;
shared_ptr<Shape> fieldCampMesh;
shared_ptr<Shape> fieldLandmarkMesh;

constexpr int NUM_POKEMON = 48;
constexpr int FLYING_POKEMON = 8;
constexpr int ALPHA_TARGET_INDEX = FLYING_POKEMON;
constexpr int ALPHA_POKEMON_ID = 1000;

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

glm::vec3 wildBattleLungeOffset(const glm::vec3 &start,
	                            const glm::vec3 &end,
	                            const BattleSequenceSample &sample,
	                            const BattleMove &move)
{
	if (move.id != BattleMoveId::Bite &&
	    move.id != BattleMoveId::Tackle)
	{
		return glm::vec3(0.0f);
	}
	glm::vec3 direction = end - start;
	direction.y = 0.0f;
	const float distance = glm::length(direction);
	if (distance <= 0.001f)
	{
		return glm::vec3(0.0f);
	}
	direction /= distance;
	const float lungeDistance = move.id == BattleMoveId::Tackle
	                                ? std::min(2.0f, distance * 0.45f)
	                                : std::min(1.55f, distance * 0.34f);
	const float eased = easedBattleProgress(sample.phaseProgress);
	if (sample.phase == BattlePhase::WildWindup)
	{
		return direction * (-0.12f * eased);
	}
	if (sample.phase == BattlePhase::WildProjectile)
	{
		return direction * (-0.12f + (lungeDistance + 0.12f) * eased);
	}
	if (sample.phase == BattlePhase::PlayerImpact)
	{
		return direction * (lungeDistance * (1.0f - eased));
	}
	return glm::vec3(0.0f);
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
		return 0.90f;
	case BattleMoveId::Bite:
		return 1.0f;
	case BattleMoveId::Tackle:
		return 1.12f;
	case BattleMoveId::WingAttack:
		return 1.35f;
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
	case BattleMoveId::Tackle:
	case BattleMoveId::WingAttack:
		return 1;
	}
	return 1;
}

void applyPlayerBattlePose(PokemonAnimationPose &pose,
	                       const BattleSequenceSample &sample,
	                       const BattleMove &move, bool playerEvaded)
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
	else if (sample.phase == BattlePhase::PlayerRecovery)
	{
		pose.bodyPitch -= (1.0f - eased) * (flamethrower ? 0.14f : 0.08f);
		pose.wingAngle *= 0.65f + eased * 0.35f;
	}
	else if (sample.phase == BattlePhase::PlayerImpact && !playerEvaded)
	{
		const float recoil = std::sin(sample.phaseProgress * 3.1415926f);
		pose.bodyPitch += recoil * 0.11f;
		pose.bodyRoll += recoil * 0.16f;
	}
}

void applyPlayerDodgePose(PokemonAnimationPose &pose, bool dodging,
	                      bool invulnerable)
{
	if (!dodging && !invulnerable)
	{
		return;
	}
	const float strength = dodging ? 1.0f : 0.48f;
	pose.bodyPitch -= 0.22f * strength;
	pose.bodyBob += 0.035f * strength;
	pose.wingAngle *= dodging ? 0.18f : 0.52f;
	pose.tailAngle *= 0.58f;
	pose.breathingScale += 0.016f * strength;
}

void applyWildBattlePose(PokemonAnimationPose &pose,
	                     const BattleSequenceSample &sample,
	                     const BattleMove &move)
{
	const float eased = easedBattleProgress(sample.phaseProgress);
	if (move.id == BattleMoveId::Bite)
	{
		if (sample.phase == BattlePhase::WildWindup)
		{
			pose.bodyPitch -= eased * 0.18f;
			pose.bodyBob -= eased * 0.055f;
			pose.breathingScale += eased * 0.026f;
		}
		else if (sample.phase == BattlePhase::WildProjectile)
		{
			const float burst = std::sin(sample.phaseProgress * 3.1415926f);
			pose.bodyPitch -= 0.18f + burst * 0.12f;
			pose.bodyBob += burst * 0.11f;
			pose.strideAngle += burst * 0.48f;
		}
		else if (sample.phase == BattlePhase::PlayerImpact)
		{
			pose.bodyPitch -= (1.0f - eased) * 0.14f;
		}
		else if (sample.phase == BattlePhase::TargetImpact)
		{
			const float recoil = std::sin(sample.phaseProgress * 3.1415926f);
			pose.bodyPitch += recoil * 0.13f;
			pose.bodyRoll -= recoil * 0.19f;
			pose.bodyBob += recoil * 0.08f;
		}
		return;
	}
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

const std::array<WorldRockPlacement, 10> &ROCK_PLACEMENTS =
	worldRockPlacements();
const std::array<WorldLandmarkPlacement, 9> &LANDMARK_PLACEMENTS =
	worldLandmarkPlacements();
const std::array<WorldTrailSegment, 9> &TRAIL_SEGMENTS =
	worldTrailSegments();
const std::array<WorldInterestPointPlacement, 4> &INTEREST_POINT_PLACEMENTS =
	worldInterestPointPlacements();
vec3 mypos;
Pokemon umbreons[NUM_POKEMON];
Pokemon charizards[FLYING_POKEMON];

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

	glm::mat4 process(double ftime, float movementScale = 1.0f)
	{
		movementScale = glm::clamp(movementScale, 0.0f, 1.0f);
		PlayerInput input;
		input.forward = static_cast<float>(w - s) * movementScale;
		input.turn = playerTurnAxis(a != 0, d != 0) * movementScale;
		input.vertical =
			static_cast<float>(((q == 1 || space == 1) ? 1 : 0) - e) *
			movementScale;
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

	void resetAt(const glm::vec3 &position, float yaw = 0.0f)
	{
		w = a = s = d = q = e = space = 0;
		controller_.reset(position, yaw);
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

	bool requestDodge()
	{
		return controller_.requestDodge();
	}

	bool isDodging() const
	{
		return controller_.isDodging();
	}

	bool isInvulnerable() const
	{
		return controller_.isInvulnerable();
	}

	float dodgeCooldownRemaining() const
	{
		return controller_.dodgeCooldownRemaining();
	}

	float dodgeCooldownFraction() const
	{
		return controller_.dodgeCooldownFraction();
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

	void setDynamicObstacles(std::vector<StaticCollisionCylinder> obstacles)
	{
		controller_.setDynamicObstacles(std::move(obstacles));
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
		return playerTurnAxis(a != 0, d != 0);
	}

	glm::mat4 viewMatrix() const
	{
		return glm::lookAt(cameraPose_.position, cameraPose_.target,
		                   glm::vec3(0.0f, 1.0f, 0.0f));
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
	GLuint eeveeBodyTex = 0;
	GLuint eeveeManeTex = 0;
	GLuint eeveeInnerEarTex = 0;
	GLuint eeveeEyeWhiteTex = 0;
	GLuint eeveeEyeIrisTex = 0;
	GLuint eeveeEyeDarkTex = 0;
	GLuint campTentTex = 0;
	GLuint campEntranceTex = 0;
	GLuint campWorkbenchTex = 0;
	GLuint campSupplyTex = 0;
	GLuint campPoleTex = 0;
	GLuint campFlagTex = 0;
	GLuint moonTreeTrunkTex = 0;
	GLuint moonTreeCanopyLowTex = 0;
	GLuint moonTreeCanopyHighTex = 0;
	GLuint redSpireRockTex = 0;
	GLuint redSpireCrystalTex = 0;
	TerrainHeightMap terrainHeightMap;
	std::string resourceDirectory;
	WorldLighting sceneLighting = sampleWorldLighting(0.25f);
	int caughtCount = 0;
	int pokeballs = RESEARCH_STARTING_POKEBALLS;
	bool attackRequested = false;
	bool resetRequested = false;
	bool gameFinished = false;
	std::string statusMessage =
		"Field camp ready · Press F for the survey briefing, then explore the field.";
	PokemonTargetSelection currentTarget;
	double nextTelemetryUpdate = 0.0;
	glm::vec3 captureEffectPosition = glm::vec3(0.0f);
	double captureEffectStarted = -100.0;
	bool captureEffectSucceeded = true;
	CaptureRandom captureRandom{static_cast<std::uint32_t>(std::time(nullptr))};
	bool captureSequenceActive = false;
	CaptureResult pendingCaptureResult;
	bool pendingCaptureLureBonus = false;
	Pokemon *pendingCaptureTarget = nullptr;
	double captureSequenceStarted = -100.0;
	CapturePhase lastCapturePhase = CapturePhase::Inactive;
	int lastCaptureShake = 0;
	std::string pendingCaptureSpecies;
	glm::vec3 captureThrowStart = glm::vec3(0.0f);
	glm::vec3 captureHitPosition = glm::vec3(0.0f);
	glm::vec3 captureBallRestPosition = glm::vec3(0.0f);
	CaptureProjectileConfig captureProjectileConfig;
	CaptureProjectileState captureProjectile;
	bool captureAiming = false;
	double captureAimStarted = -100.0;
	glm::vec3 captureProjectileOrigin = glm::vec3(0.0f);
	glm::vec3 captureMissBallPosition = glm::vec3(0.0f);
	double captureMissBallVisibleUntil = -100.0;
	int playerHealth = 118;
	int defeatedCount = 0;
	bool researchSubmitted = false;
	int lastCampSettlementScore = 0;
	int researchLevel = RESEARCH_LEVEL_TRAINEE;
	int luresRemaining = 0;
	FieldLureState fieldLure;
	FieldCampLayout fieldCamp = defaultFieldCampLayout();
	bool battleSequenceActive = false;
	BattleSequencePlan pendingBattlePlan;
	BattleDamageResult pendingPlayerDamage;
	BattleDamageResult pendingWildDamage;
	BattleMove pendingPlayerMove;
	BattleMove pendingWildMove;
	BattleMoveVolumeResult pendingPlayerMoveVolume;
	BattleMoveVolumeResult pendingWildMoveVolume;
	BattleMoveLoadout playerMoveLoadout;
	Pokemon *pendingBattleTarget = nullptr;
	double battleSequenceStarted = -100.0;
	BattlePhase lastBattlePhase = BattlePhase::Inactive;
	bool targetDamageApplied = false;
	bool playerDamageApplied = false;
	bool playerMoveReleased = false;
	bool wildMoveReleased = false;
	bool playerEvadedCurrentCounter = false;
	bool battleInitiatedByWild = false;
	std::string pendingBattleSpecies;
	glm::vec3 battlePlayerOrigin = glm::vec3(0.0f);
	glm::vec3 battleTargetPosition = glm::vec3(0.0f);
	glm::vec3 battlePlayerHitPosition = glm::vec3(0.0f);
	glm::vec3 dodgeEffectOrigin = glm::vec3(0.0f);
	double dodgeEffectStarted = -100.0;
	bool perfectDodgePending = false;
	Pokemon *perfectCounterTarget = nullptr;
	double perfectCounterWindowExpires = -100.0;
	Pokemon *overworldThreat = nullptr;
	float overworldThreatDistance = 0.0f;
	double nextWildEncounterTime = 1.25;
	PokemonGroupAlertState groupAlertState;
	ResearchMissionProgress researchProgress;
	AlphaNestProgress alphaNestProgress;
	Pokemon alphaCharizard = Pokemon(1, ALPHA_POKEMON_ID, 0xA17FA123u);
	double resetConfirmationExpires = -100.0;
	float playerAnimationPhase = 0.0f;
	GameSession gameSession;
	double lastFrameTime = -1.0;
	double nextWindowTitleUpdate = 0.0;
	float qaLightingCyclePhaseOverride = -1.0f;

	double gameplayTime() const
	{
		return gameSession.simulationTimeSeconds();
	}

	WorldLighting worldLightingAt(double now) const
	{
		const float phase = qaLightingCyclePhaseOverride >= 0.0f
		                        ? qaLightingCyclePhaseOverride
		                        : worldLightingCyclePhase(now);
		return sampleWorldLighting(phase);
	}

	bool perfectCounterWindowActive(double now) const
	{
		return perfectCounterTarget && now < perfectCounterWindowExpires &&
		       perfectCounterTarget->getCaught() == 0 &&
		       !perfectCounterTarget->isFainted() &&
		       perfectCounterTarget->isEcologicallyPresent();
	}

	double perfectCounterWindowRemaining(double now) const
	{
		return perfectCounterWindowActive(now)
		           ? std::max(0.0, perfectCounterWindowExpires - now)
		           : 0.0;
	}

	void clearPerfectCounterWindow()
	{
		perfectDodgePending = false;
		perfectCounterTarget = nullptr;
		perfectCounterWindowExpires = -100.0;
	}

	bool captureInteractionActive() const
	{
		return captureAiming || captureProjectile.active || captureSequenceActive;
	}

	float currentCaptureCharge(double now) const
	{
		return captureAiming
		           ? captureThrowChargeFraction(now - captureAimStarted,
		                                        captureProjectileConfig)
		           : 0.0f;
	}

	glm::vec3 captureAimDirection() const
	{
		return glm::vec3(-std::sin(mycam.yaw()), 0.0f,
		                 -std::cos(mycam.yaw()));
	}

	glm::vec3 captureLaunchPosition() const
	{
		return mypos + glm::vec3(0.0f, 0.9f, 0.0f) +
		       captureAimDirection() * 0.55f;
	}

	void resetCaptureInteraction()
	{
		captureAiming = false;
		captureAimStarted = -100.0;
		captureProjectile = CaptureProjectileState();
		captureMissBallVisibleUntil = -100.0;
		captureSequenceActive = false;
		pendingCaptureLureBonus = false;
		pendingCaptureTarget = nullptr;
		captureSequenceStarted = -100.0;
		lastCapturePhase = CapturePhase::Inactive;
		lastCaptureShake = 0;
		pendingCaptureSpecies.clear();
	}

	bool isAlphaPokemon(const Pokemon &candidate) const
	{
		return &candidate == &alphaCharizard;
	}

	std::string displayPokemonName(const Pokemon &candidate) const
	{
		return isAlphaPokemon(candidate)
		           ? "Alpha Charizard"
		           : pokemonSpeciesName(candidate.getSpecies());
	}

	const WorldInterestPointPlacement *alphaNestPoint() const
	{
		for (const WorldInterestPointPlacement &point : INTEREST_POINT_PLACEMENTS)
		{
			if (point.kind == WorldInterestPointKind::AlphaNest)
			{
				return &point;
			}
		}
		return nullptr;
	}

	bool alphaNestPrerequisitesMet() const
	{
		return researchProgress.moonshadowTrackSurveys > 0 &&
		       researchProgress.redrockLookoutSurveys > 0;
	}

	float distanceToAlphaNest() const
	{
		const WorldInterestPointPlacement *point = alphaNestPoint();
		return point
		           ? glm::distance(glm::vec2(mypos.x, mypos.z), point->center)
		           : std::numeric_limits<float>::infinity();
	}

	AlphaNestInteractionInput alphaNestInteractionInput(float distance) const
	{
		AlphaNestInteractionInput input;
		const WorldInterestPointPlacement *point = alphaNestPoint();
		input.distance = distance;
		input.interactionRadius = point ? point->interactionRadius : 0.0f;
		input.prerequisitesMet = alphaNestPrerequisitesMet();
		const ResearchRunOutcome outcome = currentRunOutcome();
		input.surveyActive = outcome == ResearchRunOutcome::Active ||
		                     outcome == ResearchRunOutcome::ResearchComplete;
		input.interactionBusy = battleSequenceActive || captureInteractionActive();
		input.grounded = mycam.grounded();
		return input;
	}

	std::string alphaNestPrompt(AlphaNestInteractionStatus status) const
	{
		switch (status)
		{
		case AlphaNestInteractionStatus::Available: return "F · DISTURB ALPHA NEST";
		case AlphaNestInteractionStatus::Locked: return "COMPLETE BOTH REGIONAL SURVEYS";
		case AlphaNestInteractionStatus::SurveyUnavailable: return "SURVEY CLOSED";
		case AlphaNestInteractionStatus::InteractionBusy: return "FINISH CURRENT ACTION";
		case AlphaNestInteractionStatus::TooFar: return "MOVE INTO THE NEST";
		case AlphaNestInteractionStatus::Airborne: return "LAND TO ENTER";
		case AlphaNestInteractionStatus::AlreadyActive: return "ALPHA CHARIZARD ACTIVE";
		case AlphaNestInteractionStatus::Resolved: return "ALPHA ENCOUNTER COMPLETE";
		case AlphaNestInteractionStatus::InvalidInput: return std::string();
		}
		return std::string();
	}

	void hideAlphaPokemon()
	{
		alphaCharizard.setEcologicallyPresent(false);
	}

	bool spawnAlphaPokemon()
	{
		const WorldInterestPointPlacement *point = alphaNestPoint();
		if (!point)
		{
			return false;
		}
		alphaCharizard = Pokemon(1, ALPHA_POKEMON_ID, 0xA17FA123u);
		const float nestGround = terrainHeightMap.heightAt(
			point->center.x, point->center.y);
		alphaCharizard.setPosition(glm::vec3(
			point->center.x, nestGround + 7.5f, point->center.y));
		alphaCharizard.setEcologicallyPresent(true);
		alphaCharizard.receiveCompanionAlert();
		return true;
	}

	const char *regionalResearchSiteName(WorldInterestPointKind kind) const
	{
		return kind == WorldInterestPointKind::MoonshadowTracks
		           ? "Moonshadow Tracks"
		           : "Redrock Lookout";
	}

	bool regionalResearchRecorded(WorldInterestPointKind kind) const
	{
		if (kind == WorldInterestPointKind::MoonshadowTracks)
		{
			return researchProgress.moonshadowTrackSurveys > 0;
		}
		if (kind == WorldInterestPointKind::RedrockLookout)
		{
			return researchProgress.redrockLookoutSurveys > 0;
		}
		return false;
	}

	const WorldInterestPointPlacement *nearestRegionalResearchPoint(
		float &distance) const
	{
		distance = std::numeric_limits<float>::infinity();
		const WorldInterestPointPlacement *nearest = nullptr;
		for (const WorldInterestPointPlacement &point : INTEREST_POINT_PLACEMENTS)
		{
			if (point.kind != WorldInterestPointKind::MoonshadowTracks &&
			    point.kind != WorldInterestPointKind::RedrockLookout)
			{
				continue;
			}
			const float candidateDistance = glm::distance(
				glm::vec2(mypos.x, mypos.z), point.center);
			if (candidateDistance < distance)
			{
				distance = candidateDistance;
				nearest = &point;
			}
		}
		return nearest;
	}

	RegionalObservationInput regionalObservationInput(
		const WorldInterestPointPlacement &point, float distance,
		double now) const
	{
		const ResearchRunOutcome outcome = currentRunOutcome();
		RegionalObservationInput input;
		input.kind = point.kind;
		input.distance = distance;
		input.interactionRadius = point.interactionRadius;
		input.daylight = worldLightingAt(now).daylight;
		input.grounded = mycam.grounded();
		input.surveyActive = outcome == ResearchRunOutcome::Active ||
		                     outcome == ResearchRunOutcome::ResearchComplete;
		input.interactionBusy = battleSequenceActive || captureInteractionActive();
		input.alreadyRecorded = regionalResearchRecorded(point.kind);
		return input;
	}

	std::string regionalObservationPrompt(
		const WorldInterestPointPlacement &point,
		RegionalObservationStatus status) const
	{
		switch (status)
		{
		case RegionalObservationStatus::Available:
			return point.kind == WorldInterestPointKind::MoonshadowTracks
			           ? "F · RECORD TRACKS"
			           : "F · SURVEY LOOKOUT";
		case RegionalObservationStatus::AlreadyRecorded: return "RESEARCH RECORDED";
		case RegionalObservationStatus::InteractionBusy: return "FINISH CURRENT ACTION";
		case RegionalObservationStatus::TooFar: return "MOVE INTO THE MARKER";
		case RegionalObservationStatus::Airborne: return "LAND TO RECORD";
		case RegionalObservationStatus::RequiresNight: return "RETURN AT TWILIGHT OR NIGHT";
		case RegionalObservationStatus::SurveyUnavailable: return "SURVEY CLOSED";
		case RegionalObservationStatus::InvalidInput:
		case RegionalObservationStatus::NotResearchSite: return std::string();
		}
		return std::string();
	}

	void updateWindowTitle()
	{
		if (!windowManager || !windowManager->getHandle())
		{
			return;
		}

		std::ostringstream title;
		const float daylight = worldLightingAt(gameplayTime()).daylight;
		title << "Pokemon World | W/S move  A/D turn  Q/E/Space fly  Shift dodge  Z gravity  1/2/3 moves  X attack  Hold/release C throw  L lure  F interact  R reset"
		      << " | Caught " << caughtCount << "/" << RESEARCH_CAPTURE_GOAL
		      << " | Defeated " << defeatedCount
		      << " | Poke Balls " << pokeballs
		      << " | " << researchLevelName(researchLevel)
		      << " | Lures " << luresRemaining
		      << " | HP " << playerHealth << "/"
		      << battleStatsFor(PokemonSpecies::Charizard).maximumHealth
		      << " | Move " << playerMoveLoadout.selectedMove().name
		      << " | " << (mycam.gravityEnabled() ? "Gravity ON" : "Hover mode")
		      << " | " << (mycam.grounded() ? "Grounded" : "Airborne")
		      << " | " << pokemonEcologyPhaseName(daylight);
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

	ResearchRunOutcome currentRunOutcome() const
	{
		return evaluateResearchRunOutcome(caughtCount, RESEARCH_CAPTURE_GOAL, pokeballs,
		                                 playerHealth, researchSubmitted);
	}

	bool playerInsideCamp() const
	{
		return isInsideCampInteractionRange(mypos, fieldCamp);
	}

	void resetPlayerAtFieldCamp()
	{
		const float groundHeight = terrainHeightMap.heightAt(
			fieldCamp.spawnPosition.x, fieldCamp.spawnPosition.y);
		mycam.resetAt(
			glm::vec3(fieldCamp.spawnPosition.x, groundHeight,
			          fieldCamp.spawnPosition.y),
			fieldCamp.spawnYaw);
	}

	bool playerReadyAtCamp() const
	{
		return playerInsideCamp() && mycam.grounded();
	}

	bool researchReadyToSubmit() const
	{
		return caughtCount >= RESEARCH_CAPTURE_GOAL && !researchSubmitted;
	}

	int currentResearchScore() const
	{
		const ResearchMissionSnapshot mission = makeResearchMissionSnapshot(
			caughtCount, defeatedCount, researchProgress, RESEARCH_CAPTURE_GOAL);
		CampSettlementInput input;
		input.atCamp = true;
		input.caughtCount = caughtCount;
		input.captureGoal = RESEARCH_CAPTURE_GOAL;
		input.defeatedCount = defeatedCount;
		input.completedObjectives = mission.completedObjectives();
		const CampSettlementSummary preview = makeCampSettlement(input);
		return preview.eligible ? preview.researchScore : 0;
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
		limits.captureGoal = RESEARCH_CAPTURE_GOAL;
		limits.startingPokeballs = RESEARCH_STARTING_POKEBALLS;
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
		data.researchSubmitted = researchSubmitted;
		data.researchLevel = researchLevel;
		data.luresRemaining = luresRemaining;
		data.alphaNestResolved = alphaNestProgress.resolved;
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

	bool applySavedProgress(const GameSaveData &data, int sourceVersion)
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
		alphaNestProgress = AlphaNestProgress();
		alphaNestProgress.resolved = data.alphaNestResolved;
		hideAlphaPokemon();

		resetPlayerAtFieldCamp();
		refreshPokemonCollisionObstacles();
		caughtCount = data.caughtCount;
		pokeballs = data.pokeballs;
		defeatedCount = data.defeatedCount;
		playerHealth = data.playerHealth;
		researchSubmitted = data.researchSubmitted;
		researchLevel = data.researchLevel;
		luresRemaining = data.luresRemaining;
		researchProgress = data.missionProgress;
		lastCampSettlementScore =
			researchSubmitted ? currentResearchScore() : 0;
		if (sourceVersion < 4 && researchSubmitted)
		{
			const ResearchProgressionResult migrated =
				evaluateResearchProgression(researchLevel,
				                            lastCampSettlementScore);
			researchLevel = migrated.level;
			luresRemaining = migrated.lureCapacity;
		}
		fieldLure = FieldLureState();
		groupAlertState = PokemonGroupAlertState();
		resetCaptureInteraction();
		attackRequested = false;
		resetRequested = false;
		currentTarget = PokemonTargetSelection();
		nextTelemetryUpdate = 0.0;
		nextWindowTitleUpdate = 0.0;
		dodgeEffectStarted = -100.0;
		clearPerfectCounterWindow();
		battleSequenceActive = false;
		pendingBattleTarget = nullptr;
		pendingPlayerMoveVolume = BattleMoveVolumeResult();
		playerEvadedCurrentCounter = false;
		battleInitiatedByWild = false;
		overworldThreat = nullptr;
		overworldThreatDistance = 0.0f;
		nextWildEncounterTime = gameplayTime() + 1.0;
		playerMoveLoadout.reset();
		resetConfirmationExpires = -100.0;
		const ResearchRunOutcome outcome = currentRunOutcome();
		gameFinished = outcome == ResearchRunOutcome::ResearchSubmitted ||
		               outcome == ResearchRunOutcome::PlayerFainted ||
		               outcome == ResearchRunOutcome::OutOfPokeBalls;

		std::ostringstream message;
		if (outcome == ResearchRunOutcome::ResearchComplete)
		{
			message << "Research ready to submit. Autosave restored at camp; land and press F.";
		}
		else if (outcome == ResearchRunOutcome::ResearchSubmitted)
		{
			message << "Submitted research restored. Press R twice to begin a new survey.";
		}
		else if (outcome == ResearchRunOutcome::PlayerFainted)
		{
			message << "Charizard needs recovery. Autosave restored; "
			        << (canRecoverAtCamp(outcome, pokeballs)
			                ? "press F to return to camp."
			                : "no Poke Balls remain, so press R twice for a new run.");
		}
		else if (outcome == ResearchRunOutcome::OutOfPokeBalls)
		{
			message << "Out of Poke Balls. Autosave restored; press R twice to retry.";
		}
		else
		{
			message << "Autosave restored: " << caughtCount << "/" << RESEARCH_CAPTURE_GOAL
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
			setStatus("Field camp ready · Press F for the survey briefing, then explore the field.");
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
		if (!parsed.valid ||
		    !applySavedProgress(parsed.data, parsed.sourceVersion))
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

	void addSkyLightingUniforms(const std::shared_ptr<Program> &program)
	{
		program->addUniform("horizonColor");
		program->addUniform("zenithColor");
		program->addUniform("sunDirection");
		program->addUniform("sunColor");
		program->addUniform("daylight");
	}

	void applySceneLighting(const std::shared_ptr<Program> &program, const glm::mat4 &view)
	{
		const glm::vec3 sunDirectionView = glm::normalize(
			glm::mat3(view) * sceneLighting.sunDirection);
		glUniform3fv(program->getUniform("sunDirection"), 1, &sunDirectionView[0]);
		glUniform3fv(program->getUniform("sunColor"), 1, &sceneLighting.sunColor[0]);
		glUniform3fv(program->getUniform("ambientColor"), 1, &sceneLighting.ambientColor[0]);
		glUniform3fv(program->getUniform("fogColor"), 1, &sceneLighting.fogColor[0]);
		glUniform1f(program->getUniform("fogStart"), sceneLighting.fogStart);
		glUniform1f(program->getUniform("fogEnd"), sceneLighting.fogEnd);
	}

	void applySkyLighting(const std::shared_ptr<Program> &program)
	{
		glUniform3fv(program->getUniform("horizonColor"), 1,
		             &sceneLighting.skyHorizonColor[0]);
		glUniform3fv(program->getUniform("zenithColor"), 1,
		             &sceneLighting.skyZenithColor[0]);
		glUniform3fv(program->getUniform("sunDirection"), 1,
		             &sceneLighting.sunDirection[0]);
		glUniform3fv(program->getUniform("sunColor"), 1,
		             &sceneLighting.sunColor[0]);
		glUniform1f(program->getUniform("daylight"), sceneLighting.daylight);
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
		gameSession.reset();

		for (int i = 0; i < NUM_POKEMON; ++i)
		{
			umbreons[i] = Pokemon(0, i);
		}
		for (int i = 0; i < FLYING_POKEMON; ++i)
		{
			charizards[i] = Pokemon(1, i);
		}
		alphaNestProgress = AlphaNestProgress();
		alphaCharizard = Pokemon(1, ALPHA_POKEMON_ID, 0xA17FA123u);
		hideAlphaPokemon();

		resetPlayerAtFieldCamp();
		refreshPokemonCollisionObstacles();
		caughtCount = 0;
		pokeballs = RESEARCH_STARTING_POKEBALLS;
		luresRemaining = lureCapacityForResearchLevel(researchLevel);
		fieldLure = FieldLureState();
		groupAlertState = PokemonGroupAlertState();
		researchSubmitted = false;
		lastCampSettlementScore = 0;
		resetCaptureInteraction();
		attackRequested = false;
		gameFinished = false;
		currentTarget = PokemonTargetSelection();
		nextTelemetryUpdate = 0.0;
		nextWindowTitleUpdate = 0.0;
		captureEffectStarted = -100.0;
		dodgeEffectStarted = -100.0;
		clearPerfectCounterWindow();
		captureEffectSucceeded = true;
		playerHealth = battleStatsFor(PokemonSpecies::Charizard).maximumHealth;
		defeatedCount = 0;
		battleSequenceActive = false;
		pendingBattleTarget = nullptr;
		pendingPlayerMoveVolume = BattleMoveVolumeResult();
		battleSequenceStarted = -100.0;
		lastBattlePhase = BattlePhase::Inactive;
		targetDamageApplied = false;
		playerDamageApplied = false;
		playerEvadedCurrentCounter = false;
		battleInitiatedByWild = false;
		pendingBattleSpecies.clear();
		overworldThreat = nullptr;
		overworldThreatDistance = 0.0f;
		nextWildEncounterTime = gameplayTime() + 1.0;
		playerMoveLoadout.reset();
		researchProgress = ResearchMissionProgress();
		resetConfirmationExpires = -100.0;
		playerAnimationPhase = 0.0f;
		setStatus("Field camp ready. Review the mission, then launch into the meadow.");
		saveGameProgress("New game saved");
	}

	bool recoverAtCamp()
	{
		const ResearchRunOutcome outcome = currentRunOutcome();
		if (!canRecoverAtCamp(outcome, pokeballs))
		{
			if (outcome == ResearchRunOutcome::Active)
			{
				setStatus("Camp recovery is only available after Charizard faints.");
			}
			else if (outcome == ResearchRunOutcome::ResearchComplete)
			{
				setStatus("Research is complete. Press R twice to start a new run.");
			}
			else
			{
				setStatus("No Poke Balls remain. Press R twice to start a new run.");
			}
			return false;
		}

		resetPlayerAtFieldCamp();
		playerHealth = campRecoveryHealth(
			battleStatsFor(PokemonSpecies::Charizard).maximumHealth);
		resetCaptureInteraction();
		attackRequested = false;
		currentTarget = PokemonTargetSelection();
		captureEffectStarted = -100.0;
		captureEffectSucceeded = true;
		clearPerfectCounterWindow();
		battleSequenceActive = false;
		pendingBattleTarget = nullptr;
		pendingPlayerMoveVolume = BattleMoveVolumeResult();
		battleSequenceStarted = -100.0;
		lastBattlePhase = BattlePhase::Inactive;
		targetDamageApplied = false;
		playerDamageApplied = false;
		playerEvadedCurrentCounter = false;
		battleInitiatedByWild = false;
		pendingBattleSpecies.clear();
		overworldThreat = nullptr;
		overworldThreatDistance = 0.0f;
		nextWildEncounterTime = gameplayTime() + 2.0;
		playerMoveLoadout.reset();
		dodgeEffectStarted = -100.0;
		resetConfirmationExpires = -100.0;
		gameFinished = false;
		setStatus("Charizard was retrieved to the field camp. Research progress was preserved.");
		return saveGameProgress("Camp recovery saved");
	}

	bool settleResearchAtCamp()
	{
		const ResearchMissionSnapshot mission = makeResearchMissionSnapshot(
			caughtCount, defeatedCount, researchProgress, RESEARCH_CAPTURE_GOAL);
		CampSettlementInput input;
		input.atCamp = playerReadyAtCamp();
		input.alreadySubmitted = researchSubmitted;
		input.caughtCount = caughtCount;
		input.captureGoal = RESEARCH_CAPTURE_GOAL;
		input.defeatedCount = defeatedCount;
		input.completedObjectives = mission.completedObjectives();
		input.playerMaximumHealth =
			battleStatsFor(PokemonSpecies::Charizard).maximumHealth;
		input.startingPokeballs = RESEARCH_STARTING_POKEBALLS;
		const CampSettlementSummary settlement = makeCampSettlement(input);
		if (!settlement.eligible)
		{
			return false;
		}

		researchSubmitted = true;
		lastCampSettlementScore = settlement.researchScore;
		const ResearchProgressionResult progression =
			evaluateResearchProgression(researchLevel,
			                            settlement.researchScore);
		researchLevel = progression.level;
		luresRemaining = progression.lureCapacity;
		fieldLure = FieldLureState();
		playerHealth = settlement.restoredHealth;
		pokeballs = settlement.replenishedPokeballs;
		resetCaptureInteraction();
		attackRequested = false;
		clearPerfectCounterWindow();
		battleSequenceActive = false;
		pendingBattleTarget = nullptr;
		overworldThreat = nullptr;
		overworldThreatDistance = 0.0f;
		gameFinished = true;
		emitGameCue("camp-settlement");
		std::ostringstream message;
		message << "Research submitted at camp · Score "
		        << settlement.researchScore << " · Charizard restored · "
		        << settlement.replenishedPokeballs
		        << " Poke Balls prepared";
		if (progression.observerUnlocked)
		{
			message << " · Observer rank unlocked · 2 lures prepared";
		}
		else if (researchLevelAllowsLures(researchLevel))
		{
			message << " · " << luresRemaining << " lures prepared";
		}
		message << ". Press R twice for a new survey.";
		setStatus(message.str());
		return saveGameProgress("Research submitted at camp");
	}

	bool interactWithCamp()
	{
		const ResearchRunOutcome outcome = currentRunOutcome();
		if (outcome == ResearchRunOutcome::PlayerFainted)
		{
			return recoverAtCamp();
		}
		if (outcome == ResearchRunOutcome::ResearchSubmitted)
		{
			setStatus("This survey is already submitted. Press R twice to prepare a new one.");
			return false;
		}
		if (!playerInsideCamp())
		{
			std::ostringstream message;
			message << "Field camp is " << std::fixed << std::setprecision(1)
			        << horizontalDistanceToCamp(mypos, fieldCamp)
			        << "m away. Return to the marked landing circle.";
			setStatus(message.str());
			return false;
		}
		if (!mycam.grounded())
		{
			setStatus("Land inside the camp circle before using the field station.");
			return false;
		}
		if (outcome == ResearchRunOutcome::ResearchComplete)
		{
			return settleResearchAtCamp();
		}
		if (outcome == ResearchRunOutcome::OutOfPokeBalls)
		{
			setStatus("The survey ended before submission. Press R twice to prepare a new run.");
			return false;
		}

		setStatus("Field camp ready · Catch 5 research samples, then return here to submit them.");
		return true;
	}

	bool interactWithRegionalResearch(
		const WorldInterestPointPlacement &point, float distance,
		bool persistProgress = true)
	{
		const RegionalObservationStatus status = evaluateRegionalObservation(
			regionalObservationInput(point, distance, gameplayTime()));
		switch (status)
		{
		case RegionalObservationStatus::Available:
			if (point.kind == WorldInterestPointKind::MoonshadowTracks)
			{
				recordMoonshadowTrackSurvey(researchProgress);
				setStatus(
					"Moonshadow tracks recorded · Fresh Umbreon movement confirmed after dusk · Regional objective complete.");
			}
			else
			{
				recordRedrockLookoutSurvey(researchProgress);
				setStatus(
					"Redrock lookout surveyed · Highland flight route mapped · Regional objective complete.");
			}
			emitGameCue("regional-observation");
			if (persistProgress)
			{
				saveGameProgress("Regional research saved");
			}
			return true;
		case RegionalObservationStatus::AlreadyRecorded:
			setStatus(std::string(regionalResearchSiteName(point.kind)) +
			          " is already recorded in this survey.");
			break;
		case RegionalObservationStatus::InteractionBusy:
			setStatus("Finish the current battle or capture before recording field notes.");
			break;
		case RegionalObservationStatus::TooFar:
		{
			std::ostringstream message;
			message << regionalResearchSiteName(point.kind) << " is " << std::fixed
			        << std::setprecision(1) << distance
			        << "m away. Move into the glowing observation marker.";
			setStatus(message.str());
			break;
		}
		case RegionalObservationStatus::Airborne:
			setStatus("Land inside the observation marker before recording field notes.");
			break;
		case RegionalObservationStatus::RequiresNight:
			setStatus(
				"The Moonshadow tracks are indistinct in daylight. Return at Twilight or Night when Umbreon is active.");
			break;
		case RegionalObservationStatus::SurveyUnavailable:
			setStatus("Regional research is unavailable after this survey ends. Start a new survey at camp.");
			break;
		case RegionalObservationStatus::InvalidInput:
			setStatus("This observation marker is unavailable because its field data is invalid.");
			break;
		case RegionalObservationStatus::NotResearchSite:
			return false;
		}
		return false;
	}

	bool interactWithAlphaNest(float distance)
	{
		const AlphaNestInteractionInput input = alphaNestInteractionInput(distance);
		const AlphaNestInteractionStatus status =
			evaluateAlphaNestInteraction(alphaNestProgress, input);
		switch (status)
		{
		case AlphaNestInteractionStatus::Available:
			if (!activateAlphaNest(alphaNestProgress, input) ||
			    !spawnAlphaPokemon())
			{
				alphaNestProgress.active = false;
				setStatus("The Alpha nest could not start because its world anchor is unavailable.");
				return false;
			}
			nextWildEncounterTime = gameplayTime() + 0.45;
			setStatus(
				"The nest erupts · Alpha Charizard has arrived! Risk a direct throw, or weaken it first for better odds.");
			emitGameCue("alpha-awaken", PokemonType::Fire);
			return true;
		case AlphaNestInteractionStatus::Locked:
			setStatus(
				"The nest is dormant. Record both Moonshadow Tracks and Redrock Lookout to identify the Alpha route.");
			break;
		case AlphaNestInteractionStatus::SurveyUnavailable:
			setStatus("The Alpha commission is unavailable after this survey closes.");
			break;
		case AlphaNestInteractionStatus::InteractionBusy:
			setStatus("Finish the current battle or capture before entering the Alpha nest.");
			break;
		case AlphaNestInteractionStatus::TooFar:
		{
			std::ostringstream message;
			message << "Alpha Charizard Nest is " << std::fixed
			        << std::setprecision(1) << distance
			        << "m away. Move inside the crimson ring.";
			setStatus(message.str());
			break;
		}
		case AlphaNestInteractionStatus::Airborne:
			setStatus("Land inside the crimson nest ring before disturbing it.");
			break;
		case AlphaNestInteractionStatus::AlreadyActive:
			setStatus("Alpha Charizard is already active. Track it with the field radar.");
			break;
		case AlphaNestInteractionStatus::Resolved:
			setStatus("Alpha Charizard encounter complete · The empty nest remains as a field record.");
			break;
		case AlphaNestInteractionStatus::InvalidInput:
			setStatus("The Alpha nest is unavailable because its field data is invalid.");
			break;
		}
		return false;
	}

	bool interactWithWorld(bool persistProgress = true)
	{
		const ResearchRunOutcome outcome = currentRunOutcome();
		if (playerInsideCamp() || outcome == ResearchRunOutcome::PlayerFainted ||
		    outcome == ResearchRunOutcome::ResearchSubmitted ||
		    outcome == ResearchRunOutcome::OutOfPokeBalls)
		{
			return interactWithCamp();
		}
		const float alphaDistance = distanceToAlphaNest();
		const WorldInterestPointPlacement *alphaPoint = alphaNestPoint();
		if (alphaPoint &&
		    alphaDistance <= alphaPoint->interactionRadius + 4.0f)
		{
			return interactWithAlphaNest(alphaDistance);
		}

		float distance = 0.0f;
		const WorldInterestPointPlacement *point =
			nearestRegionalResearchPoint(distance);
		if (point && distance <= point->interactionRadius + 4.0f)
		{
			return interactWithRegionalResearch(*point, distance, persistProgress);
		}
		return interactWithCamp();
	}

	bool deployLure()
	{
		glm::vec3 lurePosition = mypos + captureAimDirection() * 3.2f;
		lurePosition.y = terrainHeightMap.heightAt(
			lurePosition.x, lurePosition.z) + 0.06f;
		const bool gameplayAvailable =
			currentRunOutcome() == ResearchRunOutcome::Active &&
			!playerInsideCamp() && !battleSequenceActive &&
			!captureInteractionActive();
		const FieldLureDeployResult deployment = deployFieldLure(
			researchLevelAllowsLures(researchLevel), luresRemaining,
			mycam.grounded(), gameplayAvailable, lurePosition, fieldLure.active);
		switch (deployment.status)
		{
		case FieldLureDeployStatus::Deployed:
			luresRemaining = deployment.remainingInventory;
			fieldLure = deployment.lure;
			emitGameCue("lure-deploy");
			setStatus("Field lure deployed · Calm Eevee within 18m will investigate · " +
			          std::to_string(luresRemaining) + " remaining.");
			return saveGameProgress("Lure inventory saved");
		case FieldLureDeployStatus::Locked:
			setStatus("Lures unlock at Observer rank after a 700-point survey.");
			break;
		case FieldLureDeployStatus::Empty:
			setStatus("No lures remain in this survey. Start a new survey to restock.");
			break;
		case FieldLureDeployStatus::Airborne:
			setStatus("Land before placing a field lure.");
			break;
		case FieldLureDeployStatus::AlreadyActive:
			setStatus("A field lure is already active. Wait for its scent to fade.");
			break;
		case FieldLureDeployStatus::Unavailable:
			setStatus("Field lures can only be deployed during an active survey away from camp.");
			break;
		}
		return false;
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
		case PokemonBehaviorState::Alert:
			return CaptureActivity::Idle;
		case PokemonBehaviorState::Flee:
			return CaptureActivity::Fleeing;
		case PokemonBehaviorState::Wander:
		case PokemonBehaviorState::Pursue:
			return CaptureActivity::Moving;
		}
		return CaptureActivity::Moving;
	}

	bool hasBackHitOpportunity(const Pokemon &candidate,
	                           const glm::vec3 &throwerPosition) const
	{
		const glm::vec3 targetToThrower = throwerPosition - candidate.getPos();
		return isCaptureBackHit(candidate.getHeading(), targetToThrower.x,
		                        targetToThrower.z);
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

	void finishCaptureSequence(bool persistProgress = true)
	{
		Pokemon *target = pendingCaptureTarget;
		const bool captured = pendingCaptureResult.captured && target;
		captureSequenceActive = false;
		pendingCaptureTarget = nullptr;
		lastCapturePhase = CapturePhase::Finished;

		if (captured)
		{
			const bool capturedAlpha = isAlphaPokemon(*target);
			const bool completedEeveeTask =
				!capturedAlpha && target->getSpecies() == PokemonSpecies::Eevee &&
				target->getHealth() == target->getMaximumHealth() &&
				researchProgress.healthyEeveeCaptures == 0;
			if (completedEeveeTask)
			{
				recordHealthyEeveeCapture(researchProgress);
			}
			target->setCaught(1);
			if (capturedAlpha)
			{
				if (resolveAlphaNest(alphaNestProgress))
				{
					hideAlphaPokemon();
				}
				if (pokeballs == 0 && !researchReadyToSubmit())
				{
					gameFinished = true;
					setStatus(
						"Captured Alpha Charizard! Alpha encounter complete, but supplies are empty. Press R twice to prepare a new survey.");
				}
				else
				{
					setStatus(
						"Captured Alpha Charizard! Alpha encounter complete. Return to camp when ready.");
				}
			}
			else if (++caughtCount >= RESEARCH_CAPTURE_GOAL)
			{
				gameFinished = false;
				setStatus(
					"Research quota complete! Return to the field camp, land, and press F to submit." +
					(completedEeveeTask
					     ? std::string(" Eevee research task complete.")
					     : std::string()));
			}
			else if (pokeballs == 0)
			{
				gameFinished = true;
				setStatus(
					"Out of Poke Balls before the goal. Press R twice to retry." +
					(completedEeveeTask
					     ? std::string(" Eevee research task complete.")
					     : std::string()));
			}
			else
			{
				std::ostringstream message;
				message << "Captured " << pendingCaptureSpecies << "! "
				        << caughtCount << "/" << RESEARCH_CAPTURE_GOAL
				        << " research samples complete.";
				if (completedEeveeTask)
				{
					message << " Eevee research task complete.";
				}
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
		if (persistProgress)
		{
			saveGameProgress();
		}
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
				std::ostringstream message;
					message << "Hit! " << pendingCaptureSpecies
					        << " was pulled into the Poke Ball · "
					        << static_cast<int>(std::round(
					               pendingCaptureResult.probability * 100.0f))
					        << "% capture chance";
					if (pendingCaptureLureBonus)
					{
						message << " · Lure bonus";
					}
					message << ".";
				setStatus(message.str());
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

	void finishBattleSequence(double now)
	{
		Pokemon *target = pendingBattleTarget;
		const bool targetFainted = target && target->isFainted();
		const bool wildInitiated = battleInitiatedByWild;
		const bool openPerfectCounter =
			perfectDodgePending && target && !targetFainted;
		perfectDodgePending = false;
		battleSequenceActive = false;
		pendingBattleTarget = nullptr;
		lastBattlePhase = BattlePhase::Finished;
		battleInitiatedByWild = false;
		nextWildEncounterTime = now + 0.9;
		if (wildInitiated && target)
		{
			target->coolDownAfterAttack();
		}

		if (playerHealth <= 0)
		{
			clearPerfectCounterWindow();
			gameFinished = true;
			setStatus(canRecoverAtCamp(currentRunOutcome(), pokeballs)
			              ? "Charizard fainted. Press F to return to camp and keep your research."
			              : "Charizard fainted with no Poke Balls left. Press R twice for a new run.");
			return;
		}
		if (targetFainted)
		{
			clearPerfectCounterWindow();
			if (target && isAlphaPokemon(*target))
			{
				hideAlphaPokemon();
				setStatus(
					"Alpha Charizard defeated. Alpha encounter complete; return to camp when ready.");
				return;
			}
			std::ostringstream message;
			message << pendingBattleSpecies << " fainted. " << defeatedCount
			        << " wild Pokemon defeated.";
			setStatus(message.str());
			return;
		}
		if (openPerfectCounter)
		{
			perfectCounterTarget = target;
			perfectCounterWindowExpires =
				now + static_cast<double>(PERFECT_COUNTER_WINDOW_SECONDS);
			target->startle();
			std::ostringstream message;
			message << "Perfect dodge! Counter " << pendingBattleSpecies
			        << " within " << std::fixed << std::setprecision(1)
			        << PERFECT_COUNTER_WINDOW_SECONDS
			        << "s for +35% damage and faster startup.";
			setStatus(message.str());
			return;
		}
		if (pendingBattlePlan.counterEnabled && wildMoveReleased &&
		    !pendingWildMoveVolume.hitTarget)
		{
			std::ostringstream message;
			message << pendingBattleSpecies << "'s " << pendingWildMove.name
			        << " hit the world before reaching Charizard. Reposition around cover.";
			setStatus(message.str());
			return;
		}
		if (wildInitiated)
		{
			if (pendingBattleTarget && pendingBattleTarget->isFlying())
			{
				setStatus(playerEvadedCurrentCounter
				              ? "Clean dodge! Wild Charizard widens its turn; counterattack or change altitude."
				              : "Wild Charizard is circling for another pass. Change altitude or counterattack.");
			}
			else
			{
				setStatus(playerEvadedCurrentCounter
				              ? "Clean dodge! Umbreon pauses; counterattack or create distance."
				              : "Umbreon is guarding its territory. Create distance or counterattack.");
			}
			return;
		}
		if (target)
		{
			target->startle();
			std::ostringstream message;
			message << pendingBattleSpecies << " has " << target->getHealth()
			        << "/" << target->getMaximumHealth()
			        << " HP. Weaken it further or hold C to aim a throw.";
			setStatus(message.str());
		}
	}

	void updateBattleSequence(double now)
	{
		if (!battleSequenceActive)
		{
			return;
		}
		BattleSequenceSample sample = currentBattleSample(now);
		if (pendingBattlePlan.playerAttackEnabled && !playerMoveReleased &&
		    battlePhaseAtLeast(sample.phase, BattlePhase::PlayerProjectile))
		{
			playerMoveReleased = true;
			const glm::vec3 forward(-std::sin(mycam.yaw()), 0.0f,
			                        -std::cos(mycam.yaw()));
			battlePlayerOrigin = mypos +
			                     glm::vec3(
				                     0.0f,
				                     playerBattleMoveReleaseHeight(pendingPlayerMove.id),
				                     0.0f) +
			                     forward * 0.75f;
			if (pendingBattleTarget)
			{
				battleTargetPosition = pokemonWorldPosition(*pendingBattleTarget);
				battleTargetPosition.y +=
					pendingBattleTarget->isFlying() ? 0.0f : 0.52f;
				pendingPlayerMoveVolume = resolveSelectedBattleMove(
					pendingPlayerMove, *pendingBattleTarget,
					battlePlayerOrigin, forward);
			}
			pendingBattlePlan.playerAttackHit =
				pendingPlayerMoveVolume.hitTarget;
			pendingBattlePlan.counterEnabled =
				!pendingPlayerMoveVolume.hitTarget || !pendingBattleTarget ||
				pendingBattleTarget->getHealth() > pendingPlayerDamage.amount;
			sample = currentBattleSample(now);
		}
		if (pendingBattlePlan.counterEnabled && !wildMoveReleased &&
		    battlePhaseAtLeast(sample.phase, BattlePhase::WildProjectile))
		{
			wildMoveReleased = true;
			battlePlayerHitPosition = mypos + glm::vec3(0.0f, 0.82f, 0.0f);
			if (pendingBattleTarget)
			{
				battleTargetPosition = captureCollisionCenter(*pendingBattleTarget);
				pendingWildMoveVolume = resolveWildBattleMove(
					pendingWildMove, *pendingBattleTarget,
					battlePlayerHitPosition);
			}
		}
		if (pendingBattlePlan.playerAttackEnabled && !targetDamageApplied &&
		    battlePhaseAtLeast(sample.phase, BattlePhase::TargetImpact))
		{
			targetDamageApplied = true;
			if (pendingBattleTarget && pendingPlayerMoveVolume.hitTarget)
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
					if (isAlphaPokemon(*pendingBattleTarget))
					{
						if (resolveAlphaNest(alphaNestProgress))
						{
							hideAlphaPokemon();
						}
					}
					else
					{
						++defeatedCount;
					}
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
			else if (pendingBattleTarget)
			{
				std::ostringstream message;
				message << pendingPlayerMove.name;
				switch (pendingPlayerMoveVolume.impact)
				{
				case BattleMoveImpactKind::Terrain:
					message << " struck the terrain before reaching ";
					emitGameCue("move-blocked", pendingPlayerMove.type);
					break;
				case BattleMoveImpactKind::Obstacle:
					message << " was blocked by cover before reaching ";
					emitGameCue("move-blocked", pendingPlayerMove.type);
					break;
				case BattleMoveImpactKind::Miss:
				case BattleMoveImpactKind::Target:
					message << " missed ";
					emitGameCue("move-miss", pendingPlayerMove.type);
					break;
				}
				message << pendingBattleSpecies << ". It can counterattack.";
				setStatus(message.str());
			}
		}
		if (pendingBattlePlan.counterEnabled && !playerDamageApplied &&
		    battlePhaseAtLeast(sample.phase, BattlePhase::PlayerImpact))
		{
			playerDamageApplied = true;
			if (!pendingWildMoveVolume.hitTarget)
			{
				playerEvadedCurrentCounter = false;
				perfectDodgePending = false;
				emitGameCue("wild-blocked", pendingWildMove.type);
				std::ostringstream message;
				message << pendingBattleSpecies << "'s " << pendingWildMove.name;
				if (pendingWildMoveVolume.impact == BattleMoveImpactKind::Terrain)
				{
					message << " struck the terrain.";
				}
				else if (pendingWildMoveVolume.impact ==
				         BattleMoveImpactKind::Obstacle)
				{
					message << " was stopped by cover.";
				}
				else
				{
					message << " fell short.";
				}
				setStatus(message.str());
			}
			else
			{
				const BattleMoveGeometry geometry =
					battleMoveGeometryFor(pendingWildMove.id);
				const glm::vec2 impactOffset(
					mypos.x - battlePlayerHitPosition.x,
					mypos.z - battlePlayerHitPosition.z);
				const bool clearedImpactZone =
					glm::length(impactOffset) >= geometry.dangerRadius;
				const PlayerHitResult hit = resolvePlayerHit(
					playerHealth, pendingWildDamage.amount,
					mycam.isInvulnerable() || clearedImpactZone);
				playerHealth = hit.remainingHealth;
				playerEvadedCurrentCounter = hit.evaded;
				perfectDodgePending = isPerfectDodge(
					hit.evaded, static_cast<float>(now - dodgeEffectStarted));
				if (hit.evaded)
				{
					emitGameCue(
						perfectDodgePending ? "perfect-dodge" : "dodge-success",
						pendingWildMove.type);
					setStatus(perfectDodgePending
					              ? "Perfect dodge! Counter window opening against " +
					                    pendingBattleSpecies + "."
					              : "Charizard evaded " + pendingBattleSpecies + "'s " +
					                    pendingWildMove.name + "!");
				}
				else
				{
					emitGameCue("player-impact", pendingWildMove.type);
					std::ostringstream message;
					message << pendingBattleSpecies << " dealt " << hit.appliedDamage
					        << " damage with " << pendingWildMove.name << " (Charizard "
					        << playerHealth << "/"
					        << battleStatsFor(PokemonSpecies::Charizard).maximumHealth
					        << " HP)." << effectivenessMessage(pendingWildDamage);
					setStatus(message.str());
					saveGameProgress();
				}
			}
		}

		if (sample.phase != lastBattlePhase)
		{
			lastBattlePhase = sample.phase;
			if (sample.phase == BattlePhase::PlayerProjectile &&
			    pendingBattlePlan.playerAttackEnabled)
			{
				emitGameCue("player-attack", pendingPlayerMove.type);
				setStatus(std::string("Charizard used ") + pendingPlayerMove.name + "!");
			}
			else if (sample.phase == BattlePhase::WildWindup &&
			         pendingBattlePlan.counterEnabled)
			{
				setStatus(pendingBattleSpecies + " prepares " + pendingWildMove.name +
				          (battleInitiatedByWild ? "! Press Shift to dodge." : "!"));
			}
			else if (sample.phase == BattlePhase::WildProjectile &&
			         pendingBattlePlan.counterEnabled)
			{
				emitGameCue("wild-attack", pendingWildMove.type);
			}
		}
		if (sample.finished)
		{
			finishBattleSequence(now);
		}
	}

	void selectPlayerMove(int slot)
	{
		if (!playerMoveLoadout.selectSlot(slot))
		{
			return;
		}
		const BattleMove &move = playerMoveLoadout.selectedMove();
		const BattleMoveGeometry geometry = battleMoveGeometryFor(move.id);
		const double remaining = playerMoveLoadout.cooldownRemaining(
			slot, gameplayTime());
		std::ostringstream message;
		message << "Selected " << move.name << " · "
		        << battleMoveShapeName(geometry.shape) << " · " << std::fixed
		        << std::setprecision(1) << geometry.range << "m. ";
		if (remaining > 0.0)
		{
			message << remaining << "s until ready.";
		}
		else
		{
			message << "Ready to use.";
		}
		message << " Startup " << std::fixed << std::setprecision(2)
		        << move.timing.startupSeconds << "s · mobility "
		        << static_cast<int>(std::round(
		               (1.0f - move.timing.movementLock) * 100.0f))
		        << "%.";
		setStatus(message.str());
		emitGameCue("move-select", move.type);
	}

	void attackTargetedPokemon()
	{
		if (playerInsideCamp())
		{
			setStatus("The field camp is a safe zone. Leave camp before using battle moves.");
			return;
		}
		if (researchReadyToSubmit() && !alphaNestProgress.active)
		{
			setStatus("Research quota complete. Return to camp and press F to submit.");
			return;
		}
		if (captureInteractionActive() || battleSequenceActive)
		{
			setStatus("Finish the current action before attacking again.");
			return;
		}
		if (gameFinished)
		{
			return;
		}
		const double now = gameplayTime();
		const BattleMove &selectedMove = playerMoveLoadout.selectedMove();
		const bool perfectCounterReady = perfectCounterWindowActive(now);
		Pokemon *target = perfectCounterReady
		                      ? perfectCounterTarget
		                      : targetedPokemon();
		const double cooldownRemaining = playerMoveLoadout.cooldownRemaining(
			playerMoveLoadout.selectedSlot(), now);
		if (cooldownRemaining > 0.0)
		{
			std::ostringstream message;
			message << selectedMove.name << " is recharging for " << std::fixed
			        << std::setprecision(1) << cooldownRemaining << "s."
			        << (perfectCounterReady
			                ? " Select another ready move before the counter window closes."
			                : "");
			setStatus(message.str());
			return;
		}
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
		const bool perfectCounter =
			perfectCounterReady && target == perfectCounterTarget;
		clearPerfectCounterWindow();

		pendingBattleTarget = target;
		pendingBattleSpecies = displayPokemonName(*target);
		pendingPlayerMove = selectedMove;
		pendingWildMove = wildBattleMoveFor(target->getSpecies());
		pendingPlayerDamage = resolveBattleDamage(
			PokemonSpecies::Charizard, target->getSpecies(), pendingPlayerMove);
		BattleMoveTiming playerTiming = pendingPlayerMove.timing;
		if (perfectCounter)
		{
			pendingPlayerDamage.amount = perfectCounterDamage(
				pendingPlayerDamage.amount);
			playerTiming.startupSeconds *=
				PERFECT_COUNTER_STARTUP_MULTIPLIER;
		}
		pendingWildDamage = resolveBattleDamage(
			target->getSpecies(), PokemonSpecies::Charizard, pendingWildMove);
		const glm::vec3 forward(-std::sin(mycam.yaw()), 0.0f,
		                        -std::cos(mycam.yaw()));
		battlePlayerOrigin = mypos +
		                     glm::vec3(
			                     0.0f,
			                     playerBattleMoveReleaseHeight(pendingPlayerMove.id),
			                     0.0f) +
		                     forward * 0.75f;
		battleTargetPosition = pokemonWorldPosition(*target);
		battleTargetPosition.y += target->isFlying() ? 0.0f : 0.52f;
		pendingPlayerMoveVolume = BattleMoveVolumeResult();
		pendingPlayerMoveVolume.impactPosition = battleTargetPosition;
		pendingBattlePlan.playerAttackEnabled = true;
		pendingBattlePlan.counterEnabled = true;
		pendingBattlePlan.playerAttackHit = false;
		pendingBattlePlan.playerTiming = playerTiming;
		pendingBattlePlan.counterTiming = pendingWildMove.timing;
		battleSequenceActive = true;
		battleInitiatedByWild = false;
		battleSequenceStarted = now;
		lastBattlePhase = BattlePhase::PlayerWindup;
		targetDamageApplied = false;
		playerDamageApplied = false;
		playerMoveReleased = false;
		wildMoveReleased = false;
		playerEvadedCurrentCounter = false;
		battlePlayerHitPosition = mypos + glm::vec3(0.0f, 0.82f, 0.0f);
		pendingWildMoveVolume = BattleMoveVolumeResult();
		pendingWildMoveVolume.impactPosition = battlePlayerHitPosition;
		currentTarget = PokemonTargetSelection();
		const BattleMoveGeometry geometry =
			battleMoveGeometryFor(pendingPlayerMove.id);
		std::ostringstream message;
		message << (perfectCounter ? "Perfect counter · " : "Charizard readies ")
		        << pendingPlayerMove.name << " · "
		        << battleMoveShapeName(geometry.shape) << " · " << std::fixed
		        << std::setprecision(1) << geometry.range << "m against "
		        << pendingBattleSpecies << ".";
		setStatus(message.str());
	}

	void startWildEncounter(Pokemon &attacker, double now)
	{
		if (battleSequenceActive || captureProjectile.active ||
		    captureSequenceActive || gameFinished ||
		    attacker.getCaught() != 0 || attacker.isFainted() ||
		    !attacker.isEcologicallyPresent() ||
		    now < nextWildEncounterTime)
		{
			return;
		}
		clearPerfectCounterWindow();
		captureAiming = false;

		pendingBattleTarget = &attacker;
		pendingBattleSpecies = displayPokemonName(attacker);
		pendingPlayerMove = playerMoveLoadout.selectedMove();
		pendingWildMove = wildBattleMoveFor(attacker.getSpecies());
		pendingPlayerDamage = BattleDamageResult();
		pendingPlayerMoveVolume = BattleMoveVolumeResult();
		pendingWildDamage = resolveBattleDamage(
			attacker.getSpecies(), PokemonSpecies::Charizard, pendingWildMove);
		pendingBattlePlan.playerAttackEnabled = false;
		pendingBattlePlan.counterEnabled = true;
		pendingBattlePlan.playerAttackHit = false;
		pendingBattlePlan.playerTiming = pendingPlayerMove.timing;
		pendingBattlePlan.counterTiming = pendingWildMove.timing;
		battleSequenceActive = true;
		battleInitiatedByWild = true;
		battleSequenceStarted = now;
		lastBattlePhase = BattlePhase::Inactive;
		targetDamageApplied = true;
		playerDamageApplied = false;
		playerMoveReleased = true;
		wildMoveReleased = false;
		playerEvadedCurrentCounter = false;
		battlePlayerOrigin = mypos + glm::vec3(0.0f, 0.9f, 0.0f);
		battleTargetPosition = pokemonWorldPosition(attacker);
		battleTargetPosition.y += attacker.isFlying() ? 0.0f : 0.52f;
		battlePlayerHitPosition = mypos + glm::vec3(0.0f, 0.82f, 0.0f);
		pendingWildMoveVolume = BattleMoveVolumeResult();
		pendingWildMoveVolume.impactPosition = battlePlayerHitPosition;
		currentTarget = PokemonTargetSelection();
		nextWildEncounterTime =
		    now + battleSequenceDuration(pendingBattlePlan) + 0.9;
		emitGameCue("wild-alert", pendingWildMove.type);
		setStatus(attacker.isFlying()
		              ? pendingBattleSpecies + " lines up " +
		                    pendingWildMove.name +
		                    "! Change altitude or press Shift to dodge."
		              : pendingBattleSpecies + " lunges with " +
		                    pendingWildMove.name + "! Press Shift to dodge.");
	}

	void updatePokemonAgents(double deltaSeconds, double now)
	{
		overworldThreat = nullptr;
		overworldThreatDistance = 0.0f;
		if (gameFinished)
		{
			return;
		}
		Pokemon *nearestAttack = nullptr;
		Pokemon *nearestAlert = nullptr;
		float nearestAttackDistance = 100000.0f;
		float nearestAlertDistance = 100000.0f;
		float nearestThreatDistance = 100000.0f;
		int groupAlertedCount = 0;
		PokemonSpecies groupAlertSpecies = PokemonSpecies::Umbreon;
		const glm::vec3 playerVelocity = mycam.velocity();
		const float horizontalNoise = glm::length(glm::vec2(
			playerVelocity.x, playerVelocity.z)) / 7.0f;
		const float verticalNoise = std::fabs(playerVelocity.y) / 10.0f;
		const float playerNoise = glm::clamp(
			0.10f + horizontalNoise * 0.60f + verticalNoise * 0.25f +
				(mycam.isDodging() ? 0.25f : 0.0f),
			0.0f, 1.0f);
		const bool campSafe = playerInsideCamp();
		const float wildlifePlayerNoise = campSafe ? 0.0f : playerNoise;
		const float daylight = worldLightingAt(now).daylight;
		auto collectBehavior = [&](Pokemon &candidate,
		                           const PokemonBehaviorEvents &events) {
			if (candidate.getCaught() != 0 || candidate.isFainted() ||
			    !candidate.isEcologicallyPresent())
			{
				return;
			}
			if (campSafe)
			{
				return;
			}
			const glm::vec3 position = candidate.getPos();
			const float distance = glm::length(glm::vec2(
				mypos.x - position.x, mypos.z - position.z));
			if (candidate.isThreatening() && distance < nearestThreatDistance)
			{
				overworldThreat = &candidate;
				overworldThreatDistance = distance;
				nearestThreatDistance = distance;
			}
			if (events.alertStarted && distance < nearestAlertDistance)
			{
				nearestAlert = &candidate;
				nearestAlertDistance = distance;
			}
			if (events.attackReady && distance < nearestAttackDistance)
			{
				nearestAttack = &candidate;
				nearestAttackDistance = distance;
			}
		};
		auto recordSpeciesBehavior =
			[&](Pokemon &candidate, PokemonBehaviorState previousState,
			    const PokemonBehaviorEvents &events) {
				const PokemonSpecies species = candidate.getSpecies();
				if (species == PokemonSpecies::Bulbasaur &&
				    previousState != PokemonBehaviorState::Flee &&
				    candidate.getBehaviorState() == PokemonBehaviorState::Flee &&
				    researchProgress.bulbasaurFleeObservations == 0)
				{
					recordBulbasaurFleeObservation(researchProgress);
					saveGameProgress();
				}
				if (species == PokemonSpecies::Umbreon && events.alertStarted &&
				    researchProgress.umbreonWarningObservations == 0)
				{
					recordUmbreonWarningObservation(researchProgress);
					saveGameProgress();
				}
			};
		const CaptureSequenceSample captureSample = currentCaptureSample(now);
		const std::vector<PokemonNavigationBlocker> navigationBlockers =
			makeGroundPokemonNavigationBlockers();
		const std::vector<PokemonSightlineCylinder> sightlineBlockers =
			makePokemonSightlineBlockers();
		for (int i = 0; i < NUM_POKEMON; ++i)
		{
			const bool interactionPinned = isPendingBattleTarget(umbreons[i]) ||
			    (isPendingCaptureTarget(umbreons[i]) &&
			     captureSample.phase != CapturePhase::BrokeFree);
			if (!interactionPinned)
			{
				umbreons[i].setEcologicallyPresent(pokemonEcologySlotPresent(
					umbreons[i].getSpecies(), umbreons[i].getID(), daylight));
			}
			if (interactionPinned || !umbreons[i].isEcologicallyPresent())
			{
				continue;
			}
			const PokemonBehaviorState previousState =
				umbreons[i].getBehaviorState();
			if ((previousState == PokemonBehaviorState::Idle ||
			     previousState == PokemonBehaviorState::Wander) &&
			    fieldLureAttracts(umbreons[i].getSpecies(), umbreons[i].getPos(),
			                     umbreons[i].getAlertness(), fieldLure))
			{
				umbreons[i].investigateAt(
					fieldLure.position.x, fieldLure.position.y,
					fieldLure.position.z);
			}
			const PokemonBehaviorEvents events =
			    umbreons[i].update(
					deltaSeconds, mypos, navigationBlockers, wildlifePlayerNoise,
					!campSafe && pokemonCanSeePlayerThroughWorld(
					                 umbreons[i], sightlineBlockers),
					daylight);
			if (events.alertStarted && !campSafe)
			{
				const int recipients =
					propagateGroundPokemonAlert(i, now, daylight, sightlineBlockers);
				if (recipients > 0)
				{
					groupAlertedCount += recipients;
					groupAlertSpecies = umbreons[i].getSpecies();
				}
			}
			recordSpeciesBehavior(umbreons[i], previousState, events);
			collectBehavior(umbreons[i], events);
		}
		for (int i = 0; i < FLYING_POKEMON; ++i)
		{
			const bool interactionPinned = isPendingBattleTarget(charizards[i]) ||
			    (isPendingCaptureTarget(charizards[i]) &&
			     captureSample.phase != CapturePhase::BrokeFree);
			if (!interactionPinned)
			{
				charizards[i].setEcologicallyPresent(pokemonEcologySlotPresent(
					charizards[i].getSpecies(), charizards[i].getID(), daylight));
			}
			if (interactionPinned || !charizards[i].isEcologicallyPresent())
			{
				continue;
			}
			const PokemonBehaviorEvents events =
			    charizards[i].update(
					deltaSeconds, mypos, {}, wildlifePlayerNoise,
					!campSafe && pokemonCanSeePlayerThroughWorld(
					                 charizards[i], sightlineBlockers),
					daylight);
			collectBehavior(charizards[i], events);
		}
		const bool alphaInteractionPinned = isPendingBattleTarget(alphaCharizard) ||
			(isPendingCaptureTarget(alphaCharizard) &&
			 captureSample.phase != CapturePhase::BrokeFree);
		if (!alphaNestProgress.active || alphaNestProgress.resolved ||
		    alphaCharizard.getCaught() != 0 || alphaCharizard.isFainted())
		{
			hideAlphaPokemon();
		}
		else if (!alphaInteractionPinned)
		{
			alphaCharizard.setEcologicallyPresent(true);
			const PokemonBehaviorEvents events = alphaCharizard.update(
				deltaSeconds, mypos, {}, wildlifePlayerNoise,
				!campSafe && pokemonCanSeePlayerThroughWorld(
				                 alphaCharizard, sightlineBlockers),
				daylight);
			collectBehavior(alphaCharizard, events);
		}
		refreshPokemonCollisionObstacles();

		if (nearestAttack && !battleSequenceActive && !captureProjectile.active &&
		    !captureSequenceActive &&
		    now >= nextWildEncounterTime)
		{
			startWildEncounter(*nearestAttack, now);
			return;
		}
		if (nearestAlert && !battleSequenceActive && !captureProjectile.active &&
		    !captureSequenceActive)
		{
			emitGameCue(
				groupAlertedCount > 0 ? "group-alert" : "wild-alert",
				wildBattleMoveFor(nearestAlert->getSpecies()).type);
			const std::string species = displayPokemonName(*nearestAlert);
			std::string message;
			if (nearestAlert->getSpecies() == PokemonSpecies::Umbreon)
			{
				message = "Wild " + species +
				          " noticed you. Leave its territory or prepare to dodge.";
			}
			else if (nearestAlert->isFlying())
			{
				message = "Wild " + species +
				          " spotted you. Change altitude or break line of sight before its attack run.";
			}
			else
			{
				message = "Wild " + species +
				          " is watching you. Break line of sight or circle behind it.";
			}
			if (groupAlertedCount > 0)
			{
				message += " Its warning alerted " +
				           std::to_string(groupAlertedCount) + " nearby " +
				           pokemonSpeciesName(groupAlertSpecies) + ".";
			}
			setStatus(message);
		}
	}

	glm::vec3 pokemonSightPoint(const Pokemon &candidate) const
	{
		glm::vec3 point = pokemonWorldPosition(candidate);
		if (candidate.isFlying())
		{
			point.y += 0.45f;
		}
		else
		{
			switch (candidate.getSpecies())
			{
			case PokemonSpecies::Umbreon: point.y += 0.72f; break;
			case PokemonSpecies::Eevee: point.y += 0.62f; break;
			case PokemonSpecies::Bulbasaur: point.y += 0.58f; break;
			case PokemonSpecies::Charizard: point.y += 0.75f; break;
			}
		}
		return point;
	}

	std::vector<PokemonSightlineCylinder> makePokemonSightlineBlockers() const
	{
		std::vector<PokemonSightlineCylinder> blockers;
		blockers.reserve(ROCK_PLACEMENTS.size() + LANDMARK_PLACEMENTS.size() + 3);
		for (const WorldRockPlacement &rock : ROCK_PLACEMENTS)
		{
			PokemonSightlineCylinder blocker;
			blocker.center = rock.center;
			blocker.radius = std::max(rock.scale.x, rock.scale.z) * 1.18f;
			blocker.baseY =
				terrainHeightMap.heightAt(rock.center.x, rock.center.y);
			blocker.height = rock.scale.y * 2.36f;
			blockers.push_back(blocker);
		}
		for (const WorldLandmarkPlacement &landmark : LANDMARK_PLACEMENTS)
		{
			PokemonSightlineCylinder blocker;
			blocker.center = landmark.center;
			blocker.radius = landmark.occlusionRadius;
			blocker.baseY = terrainHeightMap.heightAt(
				landmark.center.x, landmark.center.y);
			blocker.height = landmark.height;
			blockers.push_back(blocker);
		}
		auto addCampBlocker = [&](const glm::vec2 &center, float radius,
		                          float height) {
			PokemonSightlineCylinder blocker;
			blocker.center = center;
			blocker.radius = radius;
			blocker.baseY = terrainHeightMap.heightAt(center.x, center.y);
			blocker.height = height;
			blockers.push_back(blocker);
		};
		addCampBlocker(fieldCamp.tentCenter, 2.35f, 3.0f);
		addCampBlocker(fieldCamp.workbenchCenter, 1.65f, 1.2f);
		addCampBlocker(fieldCamp.supplyCrateCenter, 1.05f, 1.5f);
		return blockers;
	}

	std::vector<BattleMoveBlockerCylinder> makeBattleMoveBlockers() const
	{
		const std::vector<PokemonSightlineCylinder> sightlineBlockers =
			makePokemonSightlineBlockers();
		std::vector<BattleMoveBlockerCylinder> blockers;
		blockers.reserve(sightlineBlockers.size());
		for (const PokemonSightlineCylinder &sightlineBlocker : sightlineBlockers)
		{
			BattleMoveBlockerCylinder blocker;
			blocker.center = sightlineBlocker.center;
			blocker.radius = sightlineBlocker.radius;
			blocker.baseY = sightlineBlocker.baseY;
			blocker.height = sightlineBlocker.height;
			blockers.push_back(blocker);
		}
		return blockers;
	}

	BattleMoveVolumeResult resolveSelectedBattleMove(
		const BattleMove &move, const Pokemon &target,
		const glm::vec3 &origin, const glm::vec3 &horizontalForward) const
	{
		BattleMoveVolumeInput input;
		input.origin = origin;
		input.targetCenter = captureCollisionCenter(target);
		input.targetRadius = captureCollisionRadius(target);
		const glm::vec3 toTarget = input.targetCenter - origin;
		const float targetDistance = glm::length(toTarget);
		if (targetDistance > 0.000001f)
		{
			const glm::vec3 targetDirection = toTarget / targetDistance;
			input.aimDirection = glm::vec3(
				horizontalForward.x, targetDirection.y, horizontalForward.z);
		}
		else
		{
			input.aimDirection = horizontalForward;
		}
		input.blockers = makeBattleMoveBlockers();
		input.groundHeightProvider = [this](float x, float z) {
			return terrainHeightMap.heightAt(x, z);
		};
		return resolveBattleMoveVolume(move.id, input);
	}

	BattleMoveVolumeResult resolveWildBattleMove(
		const BattleMove &move, const Pokemon &attacker,
		const glm::vec3 &targetCenter) const
	{
		BattleMoveVolumeInput input;
		input.origin = captureCollisionCenter(attacker);
		input.targetCenter = targetCenter;
		input.targetRadius = 0.78f;
		input.aimDirection = targetCenter - input.origin;
		input.blockers = makeBattleMoveBlockers();
		input.groundHeightProvider = [this](float x, float z) {
			return terrainHeightMap.heightAt(x, z);
		};
		return resolveBattleMoveVolume(move.id, input);
	}

	bool clearPokemonSightline(
		const glm::vec3 &observer, const glm::vec3 &subject,
		const std::vector<PokemonSightlineCylinder> &blockers) const
	{
		return pokemonSightlineClear(
			observer, subject, blockers,
			[this](float x, float z) { return terrainHeightMap.heightAt(x, z); });
	}

	bool pokemonCanSeePlayerThroughWorld(
		const Pokemon &candidate,
		const std::vector<PokemonSightlineCylinder> &blockers) const
	{
		const glm::vec3 observer = pokemonSightPoint(candidate);
		const glm::vec3 subject = mypos + glm::vec3(0.0f, 0.82f, 0.0f);
		const float horizontalDistance = glm::length(glm::vec2(
			subject.x - observer.x, subject.z - observer.z));
		return horizontalDistance > 20.0f ||
		       clearPokemonSightline(observer, subject, blockers);
	}

	int propagateGroundPokemonAlert(
		int sourceIndex, double now, float daylight,
		const std::vector<PokemonSightlineCylinder> &blockers)
	{
		if (sourceIndex < 0 || sourceIndex >= NUM_POKEMON)
		{
			return 0;
		}
		Pokemon &source = umbreons[sourceIndex];
		const glm::vec3 sourcePoint = pokemonSightPoint(source);
		std::vector<PokemonGroupAlertCandidate> candidates;
		candidates.reserve(NUM_POKEMON);
		for (int index = 0; index < NUM_POKEMON; ++index)
		{
			Pokemon &candidate = umbreons[index];
			PokemonGroupAlertCandidate alertCandidate;
			alertCandidate.id = index;
			alertCandidate.species = candidate.getSpecies();
			alertCandidate.position = candidate.getPos();
			alertCandidate.alertness = candidate.getAlertness();
			const PokemonBehaviorState state = candidate.getBehaviorState();
			alertCandidate.eligible =
				candidate.getCaught() == 0 && !candidate.isFainted() &&
				candidate.isEcologicallyPresent() &&
				pokemonEcologySlotPresent(candidate.getSpecies(), candidate.getID(),
				                          daylight) &&
				!isPendingBattleTarget(candidate) &&
				!isPendingCaptureTarget(candidate) &&
				(state == PokemonBehaviorState::Idle ||
				 state == PokemonBehaviorState::Wander);
			if (alertCandidate.eligible && index != sourceIndex &&
			    alertCandidate.species == source.getSpecies())
			{
				alertCandidate.sightlineClear = clearPokemonSightline(
					sourcePoint, pokemonSightPoint(candidate), blockers);
			}
			candidates.push_back(alertCandidate);
		}
		const PokemonGroupAlertResult result = propagatePokemonGroupAlert(
			groupAlertState, now, sourceIndex, source.getSpecies(), source.getPos(),
			candidates);
		for (int recipientId : result.recipientIds)
		{
			umbreons[recipientId].receiveCompanionAlert();
		}
		return static_cast<int>(result.recipientIds.size());
	}

	std::vector<PokemonNavigationBlocker> makeGroundPokemonNavigationBlockers() const
	{
		std::vector<PokemonNavigationBlocker> blockers;
		blockers.reserve(ROCK_PLACEMENTS.size() + LANDMARK_PLACEMENTS.size() +
		                 NUM_POKEMON + 4);
		for (std::size_t index = 0; index < ROCK_PLACEMENTS.size(); ++index)
		{
			const WorldRockPlacement &rock = ROCK_PLACEMENTS[index];
			PokemonNavigationBlocker blocker;
			blocker.id = -1000 - static_cast<int>(index);
			blocker.center = rock.center;
			blocker.radius = std::max(rock.scale.x, rock.scale.z) * 1.18f;
			blockers.push_back(blocker);
		}
		for (std::size_t index = 0; index < LANDMARK_PLACEMENTS.size(); ++index)
		{
			const WorldLandmarkPlacement &landmark = LANDMARK_PLACEMENTS[index];
			PokemonNavigationBlocker blocker;
			blocker.id = -3000 - static_cast<int>(index);
			blocker.center = landmark.center;
			blocker.radius = landmark.collisionRadius;
			blockers.push_back(blocker);
		}
		const std::array<std::pair<glm::vec2, float>, 3> campBlockers = {{
			{fieldCamp.tentCenter, 2.35f},
			{fieldCamp.workbenchCenter, 1.65f},
			{fieldCamp.supplyCrateCenter, 1.05f},
		}};
		for (std::size_t index = 0; index < campBlockers.size(); ++index)
		{
			PokemonNavigationBlocker blocker;
			blocker.id = -2000 - static_cast<int>(index);
			blocker.center = campBlockers[index].first;
			blocker.radius = campBlockers[index].second;
			blockers.push_back(blocker);
		}
		PokemonNavigationBlocker campExclusion;
		campExclusion.id = -2003;
		campExclusion.center = fieldCamp.center;
		campExclusion.radius = fieldCamp.wildExclusionRadius;
		blockers.push_back(campExclusion);
		for (int index = 0; index < NUM_POKEMON; ++index)
		{
			const Pokemon &candidate = umbreons[index];
			if (candidate.getCaught() != 0 || candidate.isFainted() ||
			    !candidate.isEcologicallyPresent())
			{
				continue;
			}
			PokemonNavigationBlocker blocker;
			blocker.id = index;
			const glm::vec3 position = candidate.getPos();
			blocker.center = glm::vec2(position.x, position.z);
			blocker.radius = candidate.getSpecies() == PokemonSpecies::Umbreon
			                     ? 0.60f
			                     : (candidate.getSpecies() == PokemonSpecies::Eevee
			                            ? 0.56f
			                            : 0.54f);
			blockers.push_back(blocker);
		}
		return blockers;
	}

	void refreshPokemonCollisionObstacles()
	{
		std::vector<StaticCollisionCylinder> colliders;
		colliders.reserve(NUM_POKEMON);
		for (int i = 0; i < NUM_POKEMON; ++i)
		{
			const Pokemon &candidate = umbreons[i];
			const glm::vec3 position = candidate.getPos();
			StaticCollisionCylinder collider;
			collider.center = glm::vec2(position.x, position.z);
			collider.baseY = terrainHeightMap.heightAt(position.x, position.z);
			if (candidate.getCaught() == 0 && !candidate.isFainted() &&
			    candidate.isEcologicallyPresent())
			{
				const PokemonSpecies species = candidate.getSpecies();
				const bool umbreon = species == PokemonSpecies::Umbreon;
				const bool eevee = species == PokemonSpecies::Eevee;
				collider.radius = umbreon ? 0.78f : (eevee ? 0.68f : 0.64f);
				collider.height = umbreon ? 1.22f : (eevee ? 1.05f : 0.96f);
			}
			else
			{
				collider.radius = 0.0f;
				collider.height = 0.0f;
			}
			colliders.push_back(collider);
		}
		mycam.setDynamicObstacles(std::move(colliders));
	}

	void refreshTarget()
	{
		if (captureProjectile.active || captureSequenceActive || battleSequenceActive)
		{
			currentTarget = PokemonTargetSelection();
			return;
		}
		std::vector<PokemonTargetCandidate> candidates;
		candidates.reserve(NUM_POKEMON + FLYING_POKEMON + 1);
		for (int i = 0; i < NUM_POKEMON; ++i)
		{
			PokemonTargetCandidate candidate;
			candidate.index = i;
			candidate.caught = umbreons[i].getCaught() != 0 ||
			                   umbreons[i].isFainted() ||
			                   !umbreons[i].isEcologicallyPresent();
			candidate.position = pokemonWorldPosition(umbreons[i]);
			candidates.push_back(candidate);
		}
		for (int i = 0; i < FLYING_POKEMON; ++i)
		{
			PokemonTargetCandidate candidate;
			candidate.index = i;
			candidate.flying = true;
			candidate.caught = charizards[i].getCaught() != 0 ||
			                   charizards[i].isFainted() ||
			                   !charizards[i].isEcologicallyPresent();
			candidate.position = pokemonWorldPosition(charizards[i]);
			candidates.push_back(candidate);
		}
		PokemonTargetCandidate alphaCandidate;
		alphaCandidate.index = ALPHA_TARGET_INDEX;
		alphaCandidate.flying = true;
		alphaCandidate.caught = alphaCharizard.getCaught() != 0 ||
		                         alphaCharizard.isFainted() ||
		                         !alphaCharizard.isEcologicallyPresent();
		alphaCandidate.position = pokemonWorldPosition(alphaCharizard);
		candidates.push_back(alphaCandidate);
		PokemonTargetingConfig targetingConfig;
		const BattleMoveGeometry selectedGeometry = battleMoveGeometryFor(
			playerMoveLoadout.selectedMove().id);
		targetingConfig.groundRange = std::max(
			targetingConfig.groundRange, selectedGeometry.range);
		targetingConfig.flyingRange = std::max(
			targetingConfig.flyingRange, selectedGeometry.range);
		currentTarget = selectPokemonTarget(
			mypos, mycam.yaw(), candidates, targetingConfig);
	}

	Pokemon *targetedPokemon()
	{
		if (!currentTarget.valid())
		{
			return nullptr;
		}
		Pokemon *candidate = nullptr;
		if (currentTarget.flying)
		{
			candidate = currentTarget.index == ALPHA_TARGET_INDEX
			                ? &alphaCharizard
			                : (currentTarget.index < FLYING_POKEMON
			                       ? &charizards[currentTarget.index]
			                       : nullptr);
		}
		else
		{
			candidate = currentTarget.index < NUM_POKEMON
			                ? &umbreons[currentTarget.index]
			                : nullptr;
		}
		return candidate && candidate->getCaught() == 0 &&
		               !candidate->isFainted() &&
		               candidate->isEcologicallyPresent()
		           ? candidate
		           : nullptr;
	}

	FieldRadarContact nearestResearchRadarContact() const
	{
		std::vector<FieldRadarCandidate> candidates;
		candidates.reserve(NUM_POKEMON + FLYING_POKEMON + 1);
		for (int index = 0; index < NUM_POKEMON; ++index)
		{
			FieldRadarCandidate candidate;
			candidate.id = index;
			candidate.available = umbreons[index].getCaught() == 0 &&
			                      !umbreons[index].isFainted() &&
			                      umbreons[index].isEcologicallyPresent();
			candidate.position = pokemonWorldPosition(umbreons[index]);
			candidates.push_back(candidate);
		}
		for (int index = 0; index < FLYING_POKEMON; ++index)
		{
			FieldRadarCandidate candidate;
			candidate.id = NUM_POKEMON + index;
			candidate.available = charizards[index].getCaught() == 0 &&
			                      !charizards[index].isFainted() &&
			                      charizards[index].isEcologicallyPresent();
			candidate.position = pokemonWorldPosition(charizards[index]);
			candidates.push_back(candidate);
		}
		FieldRadarCandidate alphaCandidate;
		alphaCandidate.id = NUM_POKEMON + ALPHA_TARGET_INDEX;
		alphaCandidate.available = alphaCharizard.getCaught() == 0 &&
		                           !alphaCharizard.isFainted() &&
		                           alphaCharizard.isEcologicallyPresent();
		alphaCandidate.position = pokemonWorldPosition(alphaCharizard);
		if (alphaNestProgress.active && alphaCandidate.available)
		{
			return selectNearestFieldRadarContact(
				mypos, mycam.yaw(), {alphaCandidate});
		}
		candidates.push_back(alphaCandidate);
		return selectNearestFieldRadarContact(mypos, mycam.yaw(), candidates);
	}

	Pokemon *pokemonForRadarContact(const FieldRadarContact &contact)
	{
		if (!contact.valid())
		{
			return nullptr;
		}
		if (contact.id < NUM_POKEMON)
		{
			return &umbreons[contact.id];
		}
		const int flyingIndex = contact.id - NUM_POKEMON;
		if (flyingIndex == ALPHA_TARGET_INDEX)
		{
			return &alphaCharizard;
		}
		return flyingIndex < FLYING_POKEMON ? &charizards[flyingIndex] : nullptr;
	}

	HudTelemetry collectHudTelemetry(double now)
	{
		static_assert(HUD_MOVE_SLOT_COUNT == PLAYER_MOVE_SLOT_COUNT,
		              "HUD move slots must match the battle loadout");
		HudTelemetry telemetry;
		std::ostringstream summary;
		float regionalDistance = 0.0f;
		const WorldInterestPointPlacement *regionalPoint =
			nearestRegionalResearchPoint(regionalDistance);
		const bool nearRegionalPoint =
			regionalPoint &&
			regionalDistance <= regionalPoint->interactionRadius + 4.0f;
		const RegionalObservationStatus regionalStatus = nearRegionalPoint
			? evaluateRegionalObservation(
			      regionalObservationInput(*regionalPoint, regionalDistance, now))
			: RegionalObservationStatus::InvalidInput;
		const WorldInterestPointPlacement *alphaPoint = alphaNestPoint();
		const float alphaDistance = distanceToAlphaNest();
		const bool nearAlphaPoint =
			alphaPoint && alphaDistance <= alphaPoint->interactionRadius + 4.0f;
		const AlphaNestInteractionStatus alphaStatus = nearAlphaPoint
			? evaluateAlphaNestInteraction(
			      alphaNestProgress, alphaNestInteractionInput(alphaDistance))
			: AlphaNestInteractionStatus::InvalidInput;
		if (captureAiming)
		{
			summary << "Aiming Poke Ball · Charge "
			        << static_cast<int>(std::round(currentCaptureCharge(now) * 100.0f))
			        << "%";
		}
		else if (captureProjectile.active)
		{
			summary << "Poke Ball airborne";
		}
		else if (battleSequenceActive)
		{
			const BattleSequenceSample sample = currentBattleSample(now);
			summary << pendingBattleSpecies << " · ";
			switch (sample.phase)
			{
			case BattlePhase::PlayerWindup:
				summary << "Charging " << pendingPlayerMove.name;
				break;
			case BattlePhase::PlayerProjectile:
				summary << pendingPlayerMove.name << " active";
				break;
			case BattlePhase::TargetImpact:
				summary << (pendingPlayerMoveVolume.hitTarget
				                ? "Target hit"
				                : "Attack blocked or missed");
				break;
			case BattlePhase::PlayerRecovery:
				summary << pendingPlayerMove.name << " recovery";
				break;
			case BattlePhase::WildWindup:
				summary << (battleInitiatedByWild ? "Attacking" : "Countering");
				break;
			case BattlePhase::WildProjectile: summary << pendingWildMove.name; break;
			case BattlePhase::PlayerImpact:
				summary << (!pendingWildMoveVolume.hitTarget
				                ? "Wild attack blocked"
				                : (playerEvadedCurrentCounter
				                       ? "Charizard evaded"
				                       : "Charizard hit"));
				break;
			case BattlePhase::Recovery: summary << "Recovering"; break;
			case BattlePhase::Inactive:
			case BattlePhase::Finished: summary << "Resolving"; break;
			}
			if (pendingBattleTarget)
			{
				summary << " · Enemy HP " << pendingBattleTarget->getHealth()
				        << "/" << pendingBattleTarget->getMaximumHealth();
			}
		}
		else if (captureSequenceActive)
		{
			const CaptureSequenceSample sample = currentCaptureSample(now);
			summary << pendingCaptureSpecies << " · ";
			switch (sample.phase)
			{
			case CapturePhase::Throwing: summary << "Ball airborne"; break;
			case CapturePhase::Absorbing: summary << "Hit"; break;
			case CapturePhase::Shaking: summary << "Shake " << sample.shakeIndex; break;
			case CapturePhase::Succeeded: summary << "Captured"; break;
			case CapturePhase::BrokeFree: summary << "Broke free"; break;
			case CapturePhase::Inactive:
			case CapturePhase::Finished: summary << "Resolving"; break;
			}
		}
		else if (researchSubmitted)
		{
			summary << "Research submitted · Score "
			        << lastCampSettlementScore << " · Supplies ready";
		}
		else if (nearAlphaPoint)
		{
			summary << "Alpha Charizard Nest · " << std::fixed
			        << std::setprecision(1) << alphaDistance << "m · "
			        << alphaNestPrompt(alphaStatus);
		}
		else if (nearRegionalPoint)
		{
			summary << regionalResearchSiteName(regionalPoint->kind) << " · "
			        << std::fixed << std::setprecision(1) << regionalDistance
			        << "m · "
			        << regionalObservationPrompt(*regionalPoint, regionalStatus);
		}
		else if (researchReadyToSubmit())
		{
			summary << "Return to camp · " << std::fixed << std::setprecision(1)
			        << horizontalDistanceToCamp(mypos, fieldCamp) << "m · "
			        << (playerReadyAtCamp() ? "Press F to submit"
			                                : (playerInsideCamp()
			                                       ? "Land to submit"
			                                       : "Follow camp beacon"));
		}
		else if (playerInsideCamp())
		{
			summary << "Field camp · Safe zone · F mission briefing";
		}
		else if (perfectCounterWindowActive(now))
		{
			summary << "Perfect counter · "
			        << displayPokemonName(*perfectCounterTarget)
			        << " · " << std::fixed << std::setprecision(1)
			        << perfectCounterWindowRemaining(now)
			        << "s · X for +35% damage";
		}
		else if (overworldThreat)
		{
			summary << "Wild alert · "
			        << displayPokemonName(*overworldThreat) << " "
			        << std::fixed << std::setprecision(1)
			        << overworldThreatDistance << "m · "
			        << (overworldThreat->getBehaviorState() ==
			                    PokemonBehaviorState::Pursue
			                ? "Pursuing"
			                : "Watching");
		}
		else if (Pokemon *target = targetedPokemon())
		{
			summary << displayPokemonName(*target) << " "
			        << std::fixed << std::setprecision(1) << currentTarget.distance
			        << "m · HP " << target->getHealth() << "/"
			        << target->getMaximumHealth() << " · Alert "
			        << static_cast<int>(std::round(target->getAlertness() * 100.0f))
			        << "%";
			if (hasBackHitOpportunity(*target, mypos))
			{
				summary << " · Back bonus";
			}
		}
		else
		{
			summary << "No target";
		}
		summary << " · " << (mycam.gravityEnabled() ? "Gravity" : "Hover")
		        << " · " << (mycam.grounded() ? "Grounded" : "Airborne")
		        << " · Charizard HP " << playerHealth << "/"
		        << battleStatsFor(PokemonSpecies::Charizard).maximumHealth;
		const float daylight = worldLightingAt(now).daylight;
		summary << " · " << pokemonEcologyPhaseName(daylight) << " · "
		        << pokemonEcologyFieldHint(daylight);
		telemetry.summary = summary.str();

		telemetry.player.health = playerHealth;
		telemetry.player.maximumHealth =
			battleStatsFor(PokemonSpecies::Charizard).maximumHealth;
		Pokemon *hudTarget = battleSequenceActive
		                         ? pendingBattleTarget
		                         : (captureSequenceActive
		                                ? pendingCaptureTarget
		                                : (perfectCounterWindowActive(now)
		                                       ? perfectCounterTarget
		                                       : targetedPokemon()));
		telemetry.target.visible = hudTarget != nullptr;
		if (hudTarget)
		{
			telemetry.target.name = displayPokemonName(*hudTarget);
			telemetry.target.health = hudTarget->getHealth();
			telemetry.target.maximumHealth = hudTarget->getMaximumHealth();
			telemetry.target.alertness = hudTarget->getAlertness();
			telemetry.target.backHitOpportunity =
				hasBackHitOpportunity(*hudTarget, mypos);
		}

		telemetry.dodge.remainingSeconds = mycam.dodgeCooldownRemaining();
		telemetry.dodge.counterWindowRemainingSeconds =
			perfectCounterWindowRemaining(now);
		telemetry.dodge.cooldownFraction = mycam.dodgeCooldownFraction();
		telemetry.dodge.dodging = mycam.isDodging();
		telemetry.dodge.invulnerable = mycam.isInvulnerable();

		telemetry.threat.visible = overworldThreat && !battleSequenceActive &&
		                           !captureSequenceActive && !gameFinished;
		if (telemetry.threat.visible)
		{
			telemetry.threat.name = displayPokemonName(*overworldThreat);
			telemetry.threat.distance = overworldThreatDistance;
			telemetry.threat.pursuing =
				overworldThreat->getBehaviorState() == PokemonBehaviorState::Pursue;
		}

		const FieldRadarContact radarContact = nearestResearchRadarContact();
		Pokemon *radarPokemon = pokemonForRadarContact(radarContact);
		telemetry.radar.visible = radarPokemon != nullptr && !gameFinished &&
		                           (!researchReadyToSubmit() || alphaNestProgress.active) &&
		                           !playerInsideCamp();
		if (telemetry.radar.visible)
		{
			telemetry.radar.name = displayPokemonName(*radarPokemon);
			telemetry.radar.distance = radarContact.distance;
			telemetry.radar.bearingRadians = radarContact.bearingRadians;
		}

		FieldRadarCandidate campCandidate;
		campCandidate.id = 0;
		campCandidate.position = glm::vec3(
			fieldCamp.center.x, 0.0f, fieldCamp.center.y);
		const FieldRadarContact campContact = selectNearestFieldRadarContact(
			mypos, mycam.yaw(), {campCandidate});
		telemetry.camp.distance = horizontalDistanceToCamp(mypos, fieldCamp);
		telemetry.camp.bearingRadians = campContact.bearingRadians;
		telemetry.camp.inside = playerInsideCamp();
		telemetry.camp.grounded = mycam.grounded();
		telemetry.camp.readyToSubmit = researchReadyToSubmit();
		telemetry.camp.submitted = researchSubmitted;
		telemetry.camp.settlementScore = lastCampSettlementScore;
		const bool showAlphaPoint = nearAlphaPoint && !alphaNestPrompt(alphaStatus).empty();
		const std::string regionalPrompt = showAlphaPoint
			? alphaNestPrompt(alphaStatus)
			: (nearRegionalPoint
			       ? regionalObservationPrompt(*regionalPoint, regionalStatus)
			       : std::string());
		telemetry.regional.visible =
			(showAlphaPoint || nearRegionalPoint) && !regionalPrompt.empty();
		if (telemetry.regional.visible)
		{
			const WorldInterestPointPlacement *hudPoint =
				showAlphaPoint ? alphaPoint : regionalPoint;
			FieldRadarCandidate regionalCandidate;
			regionalCandidate.id = 0;
			regionalCandidate.position = glm::vec3(
				hudPoint->center.x, 0.0f, hudPoint->center.y);
			const FieldRadarContact regionalContact = selectNearestFieldRadarContact(
				mypos, mycam.yaw(), {regionalCandidate});
			telemetry.regional.name = showAlphaPoint
				                          ? "Alpha Charizard Nest"
				                          : regionalResearchSiteName(regionalPoint->kind);
			telemetry.regional.prompt = regionalPrompt;
			telemetry.regional.distance =
				showAlphaPoint ? alphaDistance : regionalDistance;
			telemetry.regional.bearingRadians = regionalContact.bearingRadians;
			telemetry.regional.ready = showAlphaPoint
				                           ? alphaStatus == AlphaNestInteractionStatus::Available
				                           : regionalStatus == RegionalObservationStatus::Available;
			telemetry.regional.recorded = showAlphaPoint
				                              ? alphaStatus == AlphaNestInteractionStatus::Resolved
				                              : regionalStatus == RegionalObservationStatus::AlreadyRecorded;
			telemetry.regional.alpha = showAlphaPoint;
		}
		telemetry.research.level = researchLevel;
		telemetry.research.levelName = researchLevelName(researchLevel);
		telemetry.research.luresRemaining = luresRemaining;
		telemetry.research.lureActive = fieldLure.active;
		telemetry.research.lureRemainingSeconds =
			fieldLure.active ? fieldLure.remainingSeconds : 0.0f;

		telemetry.moveInputBusy =
			battleSequenceActive || captureInteractionActive() || gameFinished ||
			playerInsideCamp() ||
			(researchReadyToSubmit() && !alphaNestProgress.active);
		const auto &moves = playerBattleMoves();
		for (std::size_t index = 0; index < telemetry.moves.size(); ++index)
		{
			const int slot = static_cast<int>(index);
			const BattleMove &move = moves[index];
			HudMoveTelemetry &hudMove = telemetry.moves[index];
			const BattleMoveGeometry geometry = battleMoveGeometryFor(move.id);
			hudMove.name = move.name;
			hudMove.shape = battleMoveShapeName(geometry.shape);
			hudMove.type = static_cast<int>(move.type);
			hudMove.power = move.power;
			hudMove.range = geometry.range;
			hudMove.remainingSeconds =
				playerMoveLoadout.cooldownRemaining(slot, now);
			hudMove.cooldownFraction =
				playerMoveLoadout.cooldownFraction(slot, now);
			hudMove.selected = slot == playerMoveLoadout.selectedSlot();
		}

		const ResearchMissionSnapshot mission = makeResearchMissionSnapshot(
			caughtCount, defeatedCount, researchProgress, RESEARCH_CAPTURE_GOAL);
		for (std::size_t index = 0; index < telemetry.missionObjectives.size();
		     ++index)
		{
			telemetry.missionObjectives[index].current =
				mission.objectives[index].current;
			telemetry.missionObjectives[index].target =
				mission.objectives[index].target;
		}
		telemetry.completedMissionObjectives = mission.completedObjectives();
		return telemetry;
	}

	void updateWebTelemetry(double now)
	{
#ifdef __EMSCRIPTEN__
		if (now < nextTelemetryUpdate)
		{
			return;
		}
		nextTelemetryUpdate = now + 0.12;
		const HudTelemetry telemetry = collectHudTelemetry(now);
		std::string validationError;
		if (!validateHudTelemetry(telemetry, &validationError))
		{
			std::cerr << "HUD telemetry rejected: " << validationError << std::endl;
			return;
		}
		const std::string payload = encodeHudTelemetryJson(telemetry);
		EM_ASM({
			if (Module.onHudTelemetry)
			{
				Module.onHudTelemetry(JSON.parse(UTF8ToString($0)));
			}
		}, payload.c_str());
#endif
		(void)now;
	}

	void beginCaptureAim()
	{
		if (playerInsideCamp())
		{
			setStatus("The field camp is a safe zone. Launch before aiming a Poke Ball.");
			return;
		}
		if (researchReadyToSubmit() && !alphaNestProgress.active)
		{
			setStatus("Research quota complete. Return to camp and press F to submit.");
			return;
		}
		if (battleSequenceActive)
		{
			setStatus("Finish the battle exchange before aiming a Poke Ball.");
			return;
		}
		if (captureInteractionActive())
		{
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
		captureAiming = true;
		captureAimStarted = gameplayTime();
		setStatus("Aiming Poke Ball: turn to line up the arc, then release C.");
	}

	void releaseCaptureThrow(bool persistProgress = true)
	{
		if (!captureAiming)
		{
			return;
		}
		const double now = gameplayTime();
		const float charge = currentCaptureCharge(now);
		captureAiming = false;
		if (battleSequenceActive || captureSequenceActive || gameFinished ||
		    pokeballs <= 0)
		{
			setStatus("The throw was cancelled because the encounter state changed.");
			return;
		}

		captureProjectileOrigin = captureLaunchPosition();
		captureProjectile = launchCaptureProjectile(
			captureProjectileOrigin, captureAimDirection(), charge,
			captureProjectileConfig);
		if (!captureProjectile.active)
		{
			setStatus("Unable to launch the Poke Ball from this aim direction.");
			return;
		}
		--pokeballs;
		currentTarget = PokemonTargetSelection();
		std::ostringstream message;
		message << "Poke Ball away at "
		        << static_cast<int>(std::round(charge * 100.0f))
		        << "% charge. A real hit is required.";
		setStatus(message.str());
		emitGameCue("capture-throw");
		if (persistProgress)
		{
			saveGameProgress();
		}
	}

	glm::vec3 captureCollisionCenter(const Pokemon &candidate) const
	{
		glm::vec3 center = pokemonWorldPosition(candidate);
		if (!candidate.isFlying())
		{
			center.y += candidate.getSpecies() == PokemonSpecies::Umbreon
			                ? 0.68f
			                : 0.58f;
		}
		return center;
	}

	float captureCollisionRadius(const Pokemon &candidate) const
	{
		if (isAlphaPokemon(candidate))
		{
			return 1.75f;
		}
		if (candidate.isFlying())
		{
			return 1.25f;
		}
		switch (candidate.getSpecies())
		{
		case PokemonSpecies::Umbreon: return 0.72f;
		case PokemonSpecies::Eevee: return 0.64f;
		case PokemonSpecies::Bulbasaur: return 0.62f;
		case PokemonSpecies::Charizard: return 1.25f;
		}
		return 0.65f;
	}

	void finishCaptureProjectileMiss(const glm::vec3 &position,
	                               const std::string &obstacle, double now)
	{
		captureProjectile.active = false;
		captureProjectile.position = position;
		captureMissBallPosition = position;
		captureMissBallVisibleUntil = now + 0.75;
		captureEffectPosition = position;
		captureEffectStarted = now;
		captureEffectSucceeded = false;
		nextWildEncounterTime = now + 0.45;
		emitGameCue("capture-fail");
		std::ostringstream message;
		message << "The Poke Ball hit " << obstacle << " before a Pokemon. ";
		if (pokeballs <= 0)
		{
			gameFinished = true;
			message << "Out of Poke Balls; press R twice to retry.";
		}
		else
		{
			message << pokeballs << " Poke Balls left.";
		}
		setStatus(message.str());
	}

	void beginCaptureSequenceFromHit(Pokemon &target,
	                                const CaptureSweepHit &hit, double now)
	{
		CaptureAttempt attempt;
		attempt.species = target.getSpecies();
		attempt.distance = glm::length(hit.position - captureProjectileOrigin);
		attempt.maximumDistance = target.isFlying() ? FLYING_TARGETING_RANGE
		                                           : GROUND_TARGETING_RANGE;
		attempt.alignment = hit.quality;
		attempt.healthRatio = target.getHealthRatio();
		attempt.alertness = target.getAlertness();
		attempt.backHit =
			hasBackHitOpportunity(target, captureProjectileOrigin);
		attempt.lured = fieldLureCaptureBonusApplies(target.getPos(), fieldLure);
		attempt.activity = captureActivityFor(target);
		attempt.difficultyMultiplier = isAlphaPokemon(target)
		                                   ? ALPHA_CAPTURE_DIFFICULTY_MULTIPLIER
		                                   : 1.0f;
		pendingCaptureLureBonus = attempt.lured;
		pendingCaptureResult =
			resolveCaptureAttempt(attempt, captureRandom.nextUnit());
		pendingCaptureTarget = &target;
		pendingCaptureSpecies = displayPokemonName(target);
		captureProjectile.active = false;
		captureProjectile.position = hit.position;
		captureSequenceActive = true;
		captureSequenceStarted = now - captureThrowFlightPhaseDuration();
		lastCapturePhase = CapturePhase::Throwing;
		lastCaptureShake = 0;
		captureThrowStart = captureProjectileOrigin;
		captureHitPosition = hit.position;
		captureBallRestPosition = glm::vec3(
			hit.position.x,
			terrainHeightMap.heightAt(hit.position.x, hit.position.z) +
				captureProjectileConfig.radius,
			hit.position.z);
		currentTarget = PokemonTargetSelection();
		nextWildEncounterTime =
			now + captureSequenceDuration(pendingCaptureResult) + 0.45;
		std::ostringstream message;
		message << "Hit " << pendingCaptureSpecies << " with "
		        << static_cast<int>(std::round(hit.quality * 100.0f))
		        << "% throw precision · Alert "
		        << static_cast<int>(std::round(attempt.alertness * 100.0f))
		        << "%";
		if (attempt.backHit)
		{
			message << " · Back hit";
		}
		if (attempt.lured)
		{
			message << " · Lure bonus";
		}
		message << " · "
		        << static_cast<int>(std::round(
		               pendingCaptureResult.probability * 100.0f))
		        << "% capture chance.";
		setStatus(message.str());
	}

	void updateCaptureProjectile(float deltaSeconds, double now)
	{
		if (!captureProjectile.active)
		{
			return;
		}
		const CaptureProjectileSegment segment = advanceCaptureProjectile(
			captureProjectile, deltaSeconds, captureProjectileConfig);
		CaptureSweepHit environmentHit = sweepCaptureSphereAgainstTerrain(
			segment.start, segment.end, captureProjectileConfig.radius,
			[this](float x, float z) { return terrainHeightMap.heightAt(x, z); });
		std::string environmentName = "the ground";
		for (const WorldRockPlacement &rock : ROCK_PLACEMENTS)
		{
			CaptureCollisionCylinder collider;
			collider.center = rock.center;
			collider.radius = std::max(rock.scale.x, rock.scale.z) * 1.18f;
			collider.baseY =
				terrainHeightMap.heightAt(rock.center.x, rock.center.y);
			collider.height = rock.scale.y * 2.36f;
			const CaptureSweepHit rockHit = sweepCaptureSphereAgainstCylinder(
				segment.start, segment.end, captureProjectileConfig.radius,
				collider);
			if (rockHit.hit &&
			    (!environmentHit.hit || rockHit.fraction < environmentHit.fraction))
			{
				environmentHit = rockHit;
				environmentName = "a boulder";
			}
		}
		for (const WorldLandmarkPlacement &landmark : LANDMARK_PLACEMENTS)
		{
			CaptureCollisionCylinder collider;
			collider.center = landmark.center;
			collider.radius = landmark.occlusionRadius;
			collider.baseY = terrainHeightMap.heightAt(
				landmark.center.x, landmark.center.y);
			collider.height = landmark.height;
			const CaptureSweepHit landmarkHit = sweepCaptureSphereAgainstCylinder(
				segment.start, segment.end, captureProjectileConfig.radius, collider);
			if (landmarkHit.hit &&
			    (!environmentHit.hit || landmarkHit.fraction < environmentHit.fraction))
			{
				environmentHit = landmarkHit;
				environmentName = landmark.kind == WorldLandmarkKind::MoonTree
				                      ? "a moon tree"
				                      : "a redrock spire";
			}
		}

		Pokemon *hitPokemon = nullptr;
		CaptureSweepHit pokemonHit;
		auto considerPokemon = [&](Pokemon &candidate) {
			if (candidate.getCaught() != 0 || candidate.isFainted() ||
			    !candidate.isEcologicallyPresent())
			{
				return;
			}
			const CaptureSweepHit hit = sweepCaptureSphereAgainstSphere(
				segment.start, segment.end, captureProjectileConfig.radius,
				captureCollisionCenter(candidate), captureCollisionRadius(candidate));
			if (hit.hit && (!pokemonHit.hit || hit.fraction < pokemonHit.fraction))
			{
				pokemonHit = hit;
				hitPokemon = &candidate;
			}
		};
		for (int index = 0; index < NUM_POKEMON; ++index)
		{
			considerPokemon(umbreons[index]);
		}
		for (int index = 0; index < FLYING_POKEMON; ++index)
		{
			considerPokemon(charizards[index]);
		}
		considerPokemon(alphaCharizard);

		if (hitPokemon &&
		    (!environmentHit.hit || pokemonHit.fraction < environmentHit.fraction))
		{
			beginCaptureSequenceFromHit(*hitPokemon, pokemonHit, now);
			return;
		}
		if (environmentHit.hit)
		{
			finishCaptureProjectileMiss(
				environmentHit.position, environmentName, now);
			return;
		}
		if (!captureProjectile.active)
		{
			finishCaptureProjectileMiss(
				captureProjectile.position, "the edge of the field", now);
		}
	}

	std::vector<glm::vec3> capturePredictionPath(double now) const
	{
		std::vector<glm::vec3> points;
		if (!captureAiming)
		{
			return points;
		}
		CaptureProjectileState prediction = launchCaptureProjectile(
			captureLaunchPosition(), captureAimDirection(), currentCaptureCharge(now),
			captureProjectileConfig);
		if (!prediction.active)
		{
			return points;
		}
		points.push_back(prediction.position);
		while (prediction.active && points.size() < 36)
		{
			const CaptureProjectileSegment segment = advanceCaptureProjectile(
				prediction, 0.12f, captureProjectileConfig);
			CaptureSweepHit firstHit = sweepCaptureSphereAgainstTerrain(
				segment.start, segment.end, captureProjectileConfig.radius,
				[this](float x, float z) {
					return terrainHeightMap.heightAt(x, z);
				});
			for (const WorldRockPlacement &rock : ROCK_PLACEMENTS)
			{
				CaptureCollisionCylinder collider;
				collider.center = rock.center;
				collider.radius =
					std::max(rock.scale.x, rock.scale.z) * 1.18f;
				collider.baseY =
					terrainHeightMap.heightAt(rock.center.x, rock.center.y);
				collider.height = rock.scale.y * 2.36f;
				const CaptureSweepHit rockHit = sweepCaptureSphereAgainstCylinder(
					segment.start, segment.end, captureProjectileConfig.radius,
					collider);
				if (rockHit.hit &&
				    (!firstHit.hit || rockHit.fraction < firstHit.fraction))
				{
					firstHit = rockHit;
				}
			}
			for (const WorldLandmarkPlacement &landmark : LANDMARK_PLACEMENTS)
			{
				CaptureCollisionCylinder collider;
				collider.center = landmark.center;
				collider.radius = landmark.occlusionRadius;
				collider.baseY = terrainHeightMap.heightAt(
					landmark.center.x, landmark.center.y);
				collider.height = landmark.height;
				const CaptureSweepHit landmarkHit = sweepCaptureSphereAgainstCylinder(
					segment.start, segment.end, captureProjectileConfig.radius, collider);
				if (landmarkHit.hit &&
				    (!firstHit.hit || landmarkHit.fraction < firstHit.fraction))
				{
					firstHit = landmarkHit;
				}
			}
			points.push_back(firstHit.hit ? firstHit.position : segment.end);
			if (firstHit.hit)
			{
				break;
			}
		}
		return points;
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

	GLuint campTextureForPart(const std::string &partName) const
	{
		if (partName == "tent-fabric") return campTentTex;
		if (partName == "tent-entrance") return campEntranceTex;
		if (partName == "workbench") return campWorkbenchTex;
		if (partName == "supply-crate") return campSupplyTex;
		if (partName == "flagpole") return campPoleTex;
		return campFlagTex;
	}

	bool landmarkPartMatches(WorldLandmarkKind kind,
	                         const std::string &partName) const
	{
		const std::string prefix = kind == WorldLandmarkKind::MoonTree
		                               ? "moon-tree-"
		                               : "red-spire-";
		return partName.compare(0, prefix.size(), prefix) == 0;
	}

	GLuint landmarkTextureForPart(const std::string &partName) const
	{
		if (partName == "moon-tree-trunk") return moonTreeTrunkTex;
		if (partName == "moon-tree-canopy-low") return moonTreeCanopyLowTex;
		if (partName == "moon-tree-canopy-high") return moonTreeCanopyHighTex;
		if (partName == "red-spire-rock") return redSpireRockTex;
		return redSpireCrystalTex;
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

	GLuint eeveeTextureForPart(const std::string &partName) const
	{
		if (partName.find("mane") != std::string::npos ||
		    partName.find("tail-tip") != std::string::npos)
		{
			return eeveeManeTex;
		}
		if (partName.find("ear-inner") != std::string::npos)
		{
			return eeveeInnerEarTex;
		}
		if (partName.find("eye-white") != std::string::npos ||
		    partName.find("eye-highlight") != std::string::npos)
		{
			return eeveeEyeWhiteTex;
		}
		if (partName.find("eye-iris") != std::string::npos)
		{
			return eeveeEyeIrisTex;
		}
		if (partName.find("eye-dark") != std::string::npos)
		{
			return eeveeEyeDarkTex;
		}
		return eeveeBodyTex;
	}

	void drawEevee(const glm::mat4 &rootTransform,
	               const PokemonAnimationPose &pose)
	{
		for (int partIndex = 0; partIndex < eevee->partCount(); ++partIndex)
		{
			const Shape::PartInfo &part = eevee->partInfo(partIndex);
			PokemonPartAnimation animation =
				samplePokemonPartAnimation(part.name, pose);
			if (part.name.find("mane") != std::string::npos)
			{
				animation.roll += pose.strideAngle * 0.035f;
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
			glBindTexture(GL_TEXTURE_2D, eeveeTextureForPart(part.name));
			eevee->drawPart(pokemon2, partIndex, true);
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
		if ((key == GLFW_KEY_LEFT_SHIFT || key == GLFW_KEY_RIGHT_SHIFT) &&
		    action == GLFW_PRESS)
		{
			if (captureSequenceActive)
			{
				setStatus("Finish the capture sequence before dodging.");
			}
			else if (!gameFinished && !mycam.requestDodge())
			{
				std::ostringstream message;
				message << "Dodge recharging for " << std::fixed
				        << std::setprecision(1) << mycam.dodgeCooldownRemaining()
				        << "s.";
				setStatus(message.str());
			}
		}
		if (key == GLFW_KEY_C && action == GLFW_PRESS)
		{
			beginCaptureAim();
		}
		if (key == GLFW_KEY_C && action == GLFW_RELEASE)
		{
			releaseCaptureThrow();
		}
		if (key == GLFW_KEY_X && action == GLFW_PRESS)
		{
			attackRequested = true;
		}
		if (key == GLFW_KEY_F && action == GLFW_PRESS)
		{
			interactWithWorld();
		}
		if (key == GLFW_KEY_L && action == GLFW_PRESS)
		{
			deployLure();
		}
		if (key >= GLFW_KEY_1 && key <= GLFW_KEY_3 && action == GLFW_PRESS)
		{
			selectPlayerMove(key - GLFW_KEY_1);
		}
		if (key == GLFW_KEY_R && action == GLFW_PRESS)
		{
			const double now = gameplayTime();
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
		alphaCharizard = Pokemon(1, ALPHA_POKEMON_ID, 0xA17FA123u);
		hideAlphaPokemon();

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

		fieldCampMesh = make_shared<Shape>();
		fieldCampMesh->loadMesh(resourceDirectory + "/camp/field_camp.obj");
		if (fieldCampMesh->partCount() != 6)
		{
			std::cerr << "Unable to load the six-part field camp model." << std::endl;
			exit(1);
		}
		fieldCampMesh->init();
		campTentTex = createSolidTexture(50, 151, 139);
		campEntranceTex = createSolidTexture(20, 54, 58);
		campWorkbenchTex = createSolidTexture(126, 82, 43);
		campSupplyTex = createSolidTexture(226, 166, 54);
		campPoleTex = createSolidTexture(178, 195, 201);
		campFlagTex = createSolidTexture(73, 184, 220);

		fieldLandmarkMesh = make_shared<Shape>();
		fieldLandmarkMesh->loadMesh(
			resourceDirectory + "/world/field_landmarks.obj");
		if (fieldLandmarkMesh->partCount() != 5)
		{
			std::cerr << "Unable to load the five-part field landmark model."
			          << std::endl;
			exit(1);
		}
		fieldLandmarkMesh->init();
		moonTreeTrunkTex = createSolidTexture(61, 47, 68);
		moonTreeCanopyLowTex = createSolidTexture(24, 70, 67);
		moonTreeCanopyHighTex = createSolidTexture(50, 126, 111);
		redSpireRockTex = createSolidTexture(167, 73, 46);
		redSpireCrystalTex = createSolidTexture(255, 158, 61);

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

		eevee = make_shared<Shape>();
		eevee->loadMesh(resourceDirectory + "/pokemon/eevee.obj");
		if (eevee->partCount() < 12)
		{
			std::cerr << "Unable to load the articulated Eevee model." << std::endl;
			exit(1);
		}
		eevee->resize();
		eevee->init();

		eeveeBodyTex = createSolidTexture(150, 88, 48);
		eeveeManeTex = createSolidTexture(239, 211, 160);
		eeveeInnerEarTex = createSolidTexture(75, 42, 42);
		eeveeEyeWhiteTex = createSolidTexture(248, 241, 221);
		eeveeEyeIrisTex = createSolidTexture(102, 66, 40);
		eeveeEyeDarkTex = createSolidTexture(31, 23, 27);

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
		rockColliders.reserve(
			ROCK_PLACEMENTS.size() + LANDMARK_PLACEMENTS.size() + 3);
		for (const WorldRockPlacement &rock : ROCK_PLACEMENTS)
		{
			StaticCollisionCylinder collider;
			collider.center = rock.center;
			collider.radius = std::max(rock.scale.x, rock.scale.z) * 1.18f;
			collider.baseY = terrainHeightMap.heightAt(rock.center.x, rock.center.y);
			collider.height = rock.scale.y * 2.36f;
			rockColliders.push_back(collider);
		}
		for (const WorldLandmarkPlacement &landmark : LANDMARK_PLACEMENTS)
		{
			StaticCollisionCylinder collider;
			collider.center = landmark.center;
			collider.radius = landmark.collisionRadius;
			collider.baseY = terrainHeightMap.heightAt(
				landmark.center.x, landmark.center.y);
			collider.height = landmark.height;
			rockColliders.push_back(collider);
		}
		auto addCampCollider = [&](const glm::vec2 &center, float radius,
		                           float height) {
			StaticCollisionCylinder collider;
			collider.center = center;
			collider.radius = radius;
			collider.baseY = terrainHeightMap.heightAt(center.x, center.y);
			collider.height = height;
			rockColliders.push_back(collider);
		};
		addCampCollider(fieldCamp.tentCenter, 2.35f, 3.0f);
		addCampCollider(fieldCamp.workbenchCenter, 1.65f, 1.2f);
		addCampCollider(fieldCamp.supplyCrateCenter, 1.05f, 1.5f);
		mycam.setStaticObstacles(std::move(rockColliders));
		resetPlayerAtFieldCamp();
		refreshPokemonCollisionObstacles();

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
		addSkyLightingUniforms(prog);
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
		heightshader->addUniform("meadowTerrainTint");
		heightshader->addUniform("moonRegionCenter");
		heightshader->addUniform("moonRegionRadii");
		heightshader->addUniform("moonTerrainTint");
		heightshader->addUniform("redRegionCenter");
		heightshader->addUniform("redRegionRadii");
		heightshader->addUniform("redTerrainTint");
		heightshader->addUniform("trailSegments[0]");
		heightshader->addUniform("trailHalfWidths[0]");
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

	void simulateStep(const GameSessionStep &step)
	{
		const double actionNow = step.simulationTimeSeconds;
		if (perfectCounterTarget && !perfectCounterWindowActive(actionNow))
		{
			clearPerfectCounterWindow();
			setStatus("Perfect counter window closed.");
		}
		if (captureSequenceActive || gameFinished)
		{
			mycam.w = mycam.a = mycam.s = mycam.d = 0;
			mycam.q = mycam.e = mycam.space = 0;
		}
		const float movementScale = battleSequenceActive
		                                ? battleMovementScale(
		                                      pendingBattlePlan,
		                                      currentBattleSample(actionNow))
		                                : 1.0f;
		mycam.process(step.deltaSeconds, movementScale);
		const glm::vec3 playerVelocity = mycam.velocity();
		const float playerSpeedRatio = glm::clamp(
			glm::length(glm::vec2(playerVelocity.x, playerVelocity.z)) / 7.0f,
			0.0f, 1.0f);
		playerAnimationPhase = advancePokemonAnimationPhase(
			playerAnimationPhase, static_cast<float>(step.deltaSeconds), true,
			false, playerSpeedRatio);

		const PlayerMotionEvents &motionEvents = mycam.motionEvents();
		if (motionEvents.dodgeStarted)
		{
			dodgeEffectOrigin = mypos;
			dodgeEffectStarted = actionNow;
			emitGameCue("dodge");
			setStatus("Charizard dashed forward.");
		}
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
		else if (motionEvents.hitDynamicObstacle)
		{
			setStatus("A wild Pokemon blocks the path.");
		}

		updateBattleSequence(actionNow);
		updateCaptureSequence(actionNow);
		if (updateFieldLure(fieldLure, static_cast<float>(step.deltaSeconds)) &&
		    !battleSequenceActive && !captureInteractionActive() && !gameFinished)
		{
			setStatus("The field lure scent faded.");
		}
		updatePokemonAgents(step.deltaSeconds, actionNow);
		updateCaptureProjectile(static_cast<float>(step.deltaSeconds), actionNow);
		refreshTarget();
		if (attackRequested)
		{
			attackTargetedPokemon();
			attackRequested = false;
		}
		updateWebTelemetry(actionNow);
		if (actionNow >= nextWindowTitleUpdate)
		{
			nextWindowTitleUpdate = actionNow + 0.5;
			updateWindowTitle();
		}
	}

	/****DRAW
	This is the most important function in your program - this is where you
	will actually issue the commands to draw any geometry you have set up to
	draw
	********/
	void render()
	{
		// Get current frame buffer size.
		int width, height;
		glfwGetFramebufferSize(windowManager->getHandle(), &width, &height);
		if (width <= 0 || height <= 0)
		{
			return;
		}
		float aspect = width / (float)height;
		glViewport(0, 0, width, height);
		const double actionNow = gameplayTime();
		sceneLighting = worldLightingAt(actionNow);

		// Clear framebuffer.
		glClearColor(sceneLighting.fogColor.r, sceneLighting.fogColor.g,
		             sceneLighting.fogColor.b, 1.0f);
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
		glm::mat4 playerView = mycam.viewMatrix();
		const glm::vec3 playerVelocity = mycam.velocity();
		const float playerSpeedRatio = glm::clamp(
			glm::length(glm::vec2(playerVelocity.x, playerVelocity.z)) / 7.0f,
			0.0f, 1.0f);
		PokemonAnimationInput playerAnimationInput;
		playerAnimationInput.flying = true;
		playerAnimationInput.speedRatio = playerSpeedRatio;
		playerAnimationInput.verticalSpeedRatio =
			glm::clamp(playerVelocity.y / 9.0f, -1.0f, 1.0f);
		playerAnimationInput.turnRatio = mycam.turnRatio();
		playerAnimationInput.phase = playerAnimationPhase;
		PokemonAnimationPose playerPose =
			samplePokemonAnimation(playerAnimationInput);
		const double captureRenderNow = actionNow;
		const CaptureSequenceSample captureVisualSample =
			currentCaptureSample(captureRenderNow);
		const bool missedCaptureBallVisible =
			captureRenderNow < captureMissBallVisibleUntil;
		CaptureBallVisualPose visibleCaptureBallPose;
		bool captureBallVisible = false;
		if (captureProjectile.active)
		{
			captureBallVisible = true;
			visibleCaptureBallPose.position = captureProjectile.position;
			visibleCaptureBallPose.pitch =
				captureProjectile.elapsedSeconds * 12.5663706f;
			visibleCaptureBallPose.roll =
				captureProjectile.elapsedSeconds * 7.5398224f;
		}
		else if (missedCaptureBallVisible)
		{
			captureBallVisible = true;
			visibleCaptureBallPose.position = captureMissBallPosition;
			visibleCaptureBallPose.roll = 1.5707963f;
		}
		else if (captureSequenceActive && captureVisualSample.ballVisible)
		{
			captureBallVisible = true;
			visibleCaptureBallPose = captureBallVisualPose(captureVisualSample);
		}
		const BattleSequenceSample battleVisualSample =
			currentBattleSample(captureRenderNow);
		if (battleSequenceActive)
		{
			applyPlayerBattlePose(playerPose, battleVisualSample,
			                      pendingPlayerMove,
			                      playerEvadedCurrentCounter ||
			                          (wildMoveReleased &&
			                           !pendingWildMoveVolume.hitTarget));
		}
		applyPlayerDodgePose(playerPose, mycam.isDodging(),
		                     mycam.isInvulnerable());

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
		glUniform1f(prog->getUniform("time"), static_cast<float>(actionNow));
		applySkyLighting(prog);
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
		const WorldRegionDescriptor &meadowRegion = worldRegionDescriptor(
			WorldRegionKind::WindwhisperMeadow);
		const WorldRegionDescriptor &moonRegion = worldRegionDescriptor(
			WorldRegionKind::MoonshadowEdge);
		const WorldRegionDescriptor &redRegion = worldRegionDescriptor(
			WorldRegionKind::RedrockHighlands);
		const glm::vec2 moonRadii(moonRegion.innerRadius, moonRegion.outerRadius);
		const glm::vec2 redRadii(redRegion.innerRadius, redRegion.outerRadius);
		glUniform3fv(heightshader->getUniform("meadowTerrainTint"), 1,
		             &meadowRegion.terrainTint[0]);
		glUniform2fv(heightshader->getUniform("moonRegionCenter"), 1,
		             &moonRegion.center[0]);
		glUniform2fv(heightshader->getUniform("moonRegionRadii"), 1,
		             &moonRadii[0]);
		glUniform3fv(heightshader->getUniform("moonTerrainTint"), 1,
		             &moonRegion.terrainTint[0]);
		glUniform2fv(heightshader->getUniform("redRegionCenter"), 1,
		             &redRegion.center[0]);
		glUniform2fv(heightshader->getUniform("redRegionRadii"), 1,
		             &redRadii[0]);
		glUniform3fv(heightshader->getUniform("redTerrainTint"), 1,
		             &redRegion.terrainTint[0]);
		std::array<glm::vec4, 8> trailUniforms;
		std::array<float, 8> trailWidths;
		for (std::size_t index = 0; index < TRAIL_SEGMENTS.size(); ++index)
		{
			const WorldTrailSegment &segment = TRAIL_SEGMENTS[index];
			trailUniforms[index] = glm::vec4(
				segment.start.x, segment.start.y, segment.end.x, segment.end.y);
			trailWidths[index] = segment.halfWidth;
		}
		glUniform4fv(heightshader->getUniform("trailSegments[0]"),
		             static_cast<GLsizei>(trailUniforms.size()),
		             &trailUniforms[0][0]);
		glUniform1fv(heightshader->getUniform("trailHalfWidths[0]"),
		             static_cast<GLsizei>(trailWidths.size()),
		             trailWidths.data());
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
		auto drawBillboardEffect = [&](const glm::vec3 &position, float diameter,
		                               const glm::vec3 &effectColor,
		                               float opacity) {
			const glm::mat4 translation =
				glm::translate(glm::mat4(1.0f), position);
			const glm::mat4 faceCamera = glm::mat4(
				glm::transpose(glm::mat3(playerView)));
			const glm::mat4 scale = glm::scale(
				glm::mat4(1.0f), glm::vec3(diameter, diameter, 1.0f));
			const glm::mat4 centerQuad = glm::translate(
				glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, -0.5f));
			const glm::mat4 model =
				translation * faceCamera * scale * centerQuad;
			glUniformMatrix4fv(targetshader->getUniform("M"), 1, GL_FALSE,
			                   &model[0][0]);
			glUniform3fv(targetshader->getUniform("ringColor"), 1,
			             &effectColor[0]);
			glUniform1f(targetshader->getUniform("opacity"), opacity);
			glUniform1f(targetshader->getUniform("fillAmount"), 1.0f);
			glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, (void *)0);
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
		const float dodgeVisualAge =
			static_cast<float>(captureRenderNow - dodgeEffectStarted);
		if (dodgeVisualAge >= 0.0f && dodgeVisualAge < 0.46f)
		{
			const float progress = glm::clamp(dodgeVisualAge / 0.46f, 0.0f, 1.0f);
			const float originGroundHeight = terrainHeightMap.heightAt(
				dodgeEffectOrigin.x, dodgeEffectOrigin.z);
			drawTargetRing(
				glm::vec3(dodgeEffectOrigin.x, originGroundHeight + 0.045f,
				          dodgeEffectOrigin.z),
				1.35f + progress * 3.15f, glm::vec3(0.22f, 0.82f, 1.0f),
				(1.0f - progress) * 0.52f);
		}
		if (mycam.isInvulnerable())
		{
			const float pulse = 1.0f + std::sin(indicatorTime * 24.0f) * 0.08f;
			drawTargetRing(
				glm::vec3(mypos.x, playerGroundHeight + 0.05f, mypos.z),
				1.65f * pulse, glm::vec3(0.56f, 0.94f, 1.0f), 0.42f);
		}
		const float campGroundHeight = terrainHeightMap.heightAt(
			fieldCamp.center.x, fieldCamp.center.y);
		const glm::vec3 campRingColor = researchReadyToSubmit()
			? glm::vec3(1.0f, 0.78f, 0.20f)
			: (researchSubmitted ? glm::vec3(0.35f, 0.78f, 1.0f)
			                     : glm::vec3(0.28f, 0.92f, 0.62f));
		drawTargetRing(
			glm::vec3(fieldCamp.center.x, campGroundHeight + 0.055f,
			          fieldCamp.center.y),
			fieldCamp.landingRadius * 2.0f, campRingColor,
			playerInsideCamp() ? 0.72f : 0.46f);
		for (const WorldLandmarkPlacement &landmark : LANDMARK_PLACEMENTS)
		{
			const float groundHeight = terrainHeightMap.heightAt(
				landmark.center.x, landmark.center.y);
			const glm::vec2 shadowDimensions =
				landmark.kind == WorldLandmarkKind::MoonTree
				    ? glm::vec2(landmark.occlusionRadius * 2.25f,
				                landmark.occlusionRadius * 1.35f)
				    : glm::vec2(landmark.collisionRadius * 2.1f,
				                landmark.collisionRadius * 1.35f);
			drawGroundShadow(
				glm::vec3(landmark.center.x, groundHeight + 0.033f,
				          landmark.center.y),
				shadowDimensions,
				landmark.kind == WorldLandmarkKind::MoonTree ? 0.20f : 0.17f);
		}
		for (std::size_t index = 0; index < INTEREST_POINT_PLACEMENTS.size(); ++index)
		{
			const WorldInterestPointPlacement &point =
				INTEREST_POINT_PLACEMENTS[index];
			const float groundHeight = terrainHeightMap.heightAt(
				point.center.x, point.center.y);
			const bool alphaMarker = point.kind == WorldInterestPointKind::AlphaNest;
			const bool recorded = alphaMarker
			                          ? alphaNestProgress.resolved
			                          : regionalResearchRecorded(point.kind);
			const bool alphaActive = alphaMarker && alphaNestProgress.active;
			const bool alphaUnlocked = alphaMarker && alphaNestPrerequisitesMet();
			const glm::vec3 markerColor = recorded
				? glm::vec3(0.48f, 0.78f, 1.0f)
				: (alphaActive
				       ? glm::vec3(1.0f, 0.16f, 0.08f)
				       : (alphaMarker
				              ? (alphaUnlocked ? glm::vec3(1.0f, 0.42f, 0.10f)
				                               : glm::vec3(0.38f, 0.22f, 0.48f))
				              : (point.kind == WorldInterestPointKind::Trailhead
				                     ? glm::vec3(0.22f, 0.88f, 1.0f)
				                     : (point.kind == WorldInterestPointKind::MoonshadowTracks
				                            ? glm::vec3(0.30f, 0.92f, 0.72f)
					                            : glm::vec3(1.0f, 0.62f, 0.20f)))));
			const float pulse = recorded ? 1.0f : 1.0f +
			                    std::sin(indicatorTime * (alphaActive ? 5.2f : 2.8f) +
			                             static_cast<float>(index) * 1.7f) *
			                        (alphaActive ? 0.12f : 0.06f);
			drawPlanarEffect(
				glm::vec3(point.center.x, groundHeight + 0.041f, point.center.y),
				glm::vec2(point.visualRadius * 1.55f), markerColor,
				recorded ? 0.10f : 0.16f, 1.0f);
			drawTargetRing(
				glm::vec3(point.center.x, groundHeight + 0.047f, point.center.y),
				point.visualRadius * (recorded ? 1.72f : 2.0f) * pulse,
				markerColor, recorded ? 0.42f : 0.64f);
			drawBillboardEffect(
				glm::vec3(point.center.x,
				          groundHeight + (recorded ? 0.68f : 1.25f), point.center.y),
				(recorded ? 0.28f : 0.38f) * pulse, markerColor,
				recorded ? 0.48f : 0.72f);
			if (recorded)
			{
				drawTargetRing(
					glm::vec3(point.center.x, groundHeight + 0.052f, point.center.y),
					point.visualRadius * 1.15f, glm::vec3(0.78f, 0.94f, 1.0f),
					0.52f);
			}
			else if (alphaMarker)
			{
				drawTargetRing(
					glm::vec3(point.center.x, groundHeight + 0.052f, point.center.y),
					point.visualRadius * (alphaActive ? 0.92f : 1.18f) * pulse,
					alphaActive ? glm::vec3(1.0f, 0.72f, 0.18f)
					            : glm::vec3(0.86f, 0.46f, 1.0f),
					alphaActive ? 0.84f : 0.48f);
			}
		}
		if (fieldLure.active)
		{
			const float lureProgress = glm::clamp(
				fieldLure.remainingSeconds / FIELD_LURE_DURATION_SECONDS,
				0.0f, 1.0f);
			const float lurePulse = 0.5f +
				0.5f * std::sin(indicatorTime * 6.0f);
			const glm::vec3 lurePosition(
				fieldLure.position.x,
				terrainHeightMap.heightAt(fieldLure.position.x,
				                          fieldLure.position.z) + 0.06f,
				fieldLure.position.z);
			drawTargetRing(lurePosition, 2.6f + lurePulse * 0.45f,
			               glm::vec3(1.0f, 0.34f, 0.76f),
			               0.34f + lureProgress * 0.22f);
			drawTargetRing(lurePosition, 0.62f + lurePulse * 0.18f,
			               glm::vec3(1.0f, 0.82f, 0.22f), 0.78f);
		}

		for (int i = 0; i < NUM_POKEMON; ++i)
		{
			if (umbreons[i].getCaught() == 1 || umbreons[i].isFainted() ||
			    !umbreons[i].isEcologicallyPresent() ||
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
			if (umbreons[i].isThreatening() &&
			    !isPendingBattleTarget(umbreons[i]) &&
			    !isPendingCaptureTarget(umbreons[i]))
			{
				const bool pursuing = umbreons[i].getBehaviorState() ==
				                       PokemonBehaviorState::Pursue;
				const float pulse = 1.0f +
				                    std::sin(indicatorTime * (pursuing ? 9.0f : 5.0f) +
				                             static_cast<float>(i)) *
				                        (pursuing ? 0.10f : 0.06f);
				drawTargetRing(
					glm::vec3(position.x,
					          terrainHeightMap.heightAt(position.x, position.z) + 0.045f,
					          position.z),
					(pursuing ? 2.35f : 2.05f) * pulse,
					pursuing ? glm::vec3(1.0f, 0.15f, 0.12f)
					          : glm::vec3(1.0f, 0.68f, 0.08f),
					pursuing ? 0.74f : 0.58f);
			}
		}

		for (int i = 0; i <= FLYING_POKEMON; ++i)
		{
			Pokemon &flyingPokemon = i == ALPHA_TARGET_INDEX
			                             ? alphaCharizard
			                             : charizards[i];
			if (flyingPokemon.getCaught() == 1 || flyingPokemon.isFainted() ||
			    !flyingPokemon.isEcologicallyPresent() ||
			    (isPendingCaptureTarget(flyingPokemon) &&
			     !captureVisualSample.pokemonVisible))
			{
				continue;
			}
			const glm::vec3 position = flyingPokemon.getPos();
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
			const float alphaScale = isAlphaPokemon(flyingPokemon) ? 1.45f : 1.0f;
			drawGroundShadow(glm::vec3(position.x, groundHeight + 0.038f, position.z),
			                 glm::vec2(2.45f, 1.42f) * growth * alphaScale,
			                 opacity);
		}

		Pokemon *lockedPokemon = targetedPokemon();
		if (lockedPokemon && lockedPokemon->getCaught() == 0 &&
		    lockedPokemon->isEcologicallyPresent())
		{
			glm::vec3 targetPosition = pokemonWorldPosition(*lockedPokemon);
			targetPosition.y += currentTarget.flying ? -0.65f : 0.08f;
			const float aimPulse = captureAiming
			                           ? 1.0f + std::sin(indicatorTime * 7.0f) * 0.08f
			                           : 1.0f;
			const float targetDiameter =
				(isAlphaPokemon(*lockedPokemon)
				     ? 4.4f
				     : (currentTarget.flying ? 3.2f : 2.35f)) *
				aimPulse;
			drawTargetRing(targetPosition, targetDiameter,
			               captureAiming ? glm::vec3(1.0f, 0.68f, 0.14f)
			                              : glm::vec3(0.18f, 0.82f, 1.0f),
			               captureAiming ? 0.96f : 0.88f);
		}
		if (captureAiming)
		{
			const std::vector<glm::vec3> prediction =
				capturePredictionPath(captureRenderNow);
			const float charge = currentCaptureCharge(captureRenderNow);
			for (std::size_t index = 1; index < prediction.size(); ++index)
			{
				const float progress = static_cast<float>(index) /
				                       static_cast<float>(prediction.size());
				const glm::vec3 color = glm::mix(
					glm::vec3(0.24f, 0.84f, 1.0f),
					glm::vec3(1.0f, 0.60f, 0.12f), progress);
				const glm::vec3 groundProjection(
					prediction[index].x,
					terrainHeightMap.heightAt(
						prediction[index].x, prediction[index].z) + 0.045f,
					prediction[index].z);
				drawTargetRing(
					groundProjection, 0.18f + charge * 0.10f, color,
					0.42f - progress * 0.12f);
				if ((index % 2u) == 1u || index + 1u == prediction.size())
				{
					drawBillboardEffect(
						prediction[index], 0.30f + charge * 0.18f, color,
						0.94f - progress * 0.18f);
				}
			}
			if (prediction.size() > 1)
			{
				const glm::vec3 &landing = prediction.back();
				drawTargetRing(
					glm::vec3(
						landing.x,
						terrainHeightMap.heightAt(landing.x, landing.z) + 0.055f,
						landing.z),
					0.82f + charge * 0.34f, glm::vec3(1.0f, 0.58f, 0.10f),
					0.86f);
			}
		}
		if (battleSequenceActive)
		{
			const BattleEffectPalette playerPalette =
				battleEffectPalette(pendingPlayerMove.type);
			const BattleEffectPalette wildPalette =
				battleEffectPalette(pendingWildMove.type);
			const float playerVisualScale =
				battleMoveVisualScale(pendingPlayerMove.id);
			const BattleMoveGeometry wildGeometry =
				battleMoveGeometryFor(pendingWildMove.id);
			glm::vec3 playerRingPosition(
				mypos.x, terrainHeightMap.heightAt(mypos.x, mypos.z) + 0.055f,
				mypos.z);
			glm::vec3 targetRingPosition = battleTargetPosition;
			glm::vec3 playerImpactRingPosition =
				pendingPlayerMoveVolume.impactPosition;
			playerImpactRingPosition.y = terrainHeightMap.heightAt(
				playerImpactRingPosition.x, playerImpactRingPosition.z) + 0.055f;
			glm::vec3 wildImpactRingPosition = pendingWildMoveVolume.hitTarget
			                                      ? battlePlayerHitPosition
			                                      : pendingWildMoveVolume.impactPosition;
			if (mycam.grounded() ||
			    pendingWildMoveVolume.impact == BattleMoveImpactKind::Terrain ||
			    pendingWildMoveVolume.impact == BattleMoveImpactKind::Obstacle)
			{
				wildImpactRingPosition.y = terrainHeightMap.heightAt(
					wildImpactRingPosition.x, wildImpactRingPosition.z) + 0.055f;
			}
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
				drawTargetRing(playerImpactRingPosition,
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
			else if (battleVisualSample.phase == BattlePhase::WildProjectile)
			{
				const float pulse =
					1.0f + std::sin(indicatorTime * 14.0f) * 0.08f;
				drawTargetRing(targetRingPosition, 1.28f,
				               wildPalette.coreColor, 0.48f);
				drawTargetRing(wildImpactRingPosition,
				               wildGeometry.dangerRadius * 2.0f * pulse,
				               wildPalette.effectColor, 0.82f);
			}
			else if (battleVisualSample.phase == BattlePhase::PlayerImpact)
			{
				const float progress = battleVisualSample.phaseProgress;
				drawTargetRing(wildImpactRingPosition,
				               wildGeometry.dangerRadius * 2.0f + progress * 2.4f,
				               wildPalette.effectColor,
				               (1.0f - progress) *
				                   (pendingWildMoveVolume.hitTarget ? 0.82f : 0.38f));
			}
		}
		if (captureBallVisible)
		{
			glm::vec3 ballIndicatorPosition = visibleCaptureBallPose.position;
			glm::vec3 ballIndicatorColor(1.0f, 0.68f, 0.16f);
			float ballIndicatorDiameter = 0.58f;
			if (captureProjectile.active)
			{
				ballIndicatorColor = glm::vec3(1.0f, 0.34f, 0.22f);
				ballIndicatorDiameter = 0.46f;
			}
			else if (missedCaptureBallVisible)
			{
				ballIndicatorColor = glm::vec3(1.0f, 0.24f, 0.12f);
				ballIndicatorDiameter = 0.50f;
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

		if (captureBallVisible)
		{
			const CaptureBallVisualPose &ballPose = visibleCaptureBallPose;
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
		glUniform1f(pokemon2->getUniform("surfaceDeform"), 0.0f);
		applyCharizardAnimation(pokemon2, PokemonAnimationPose(), false);
		M = glm::translate(
			glm::mat4(1.0f),
			glm::vec3(fieldCamp.center.x, campGroundHeight, fieldCamp.center.y));
		glUniformMatrix4fv(pokemon2->getUniform("M"), 1, GL_FALSE, &M[0][0]);
		for (int partIndex = 0; partIndex < fieldCampMesh->partCount(); ++partIndex)
		{
			const Shape::PartInfo &part = fieldCampMesh->partInfo(partIndex);
			glBindTexture(GL_TEXTURE_2D, campTextureForPart(part.name));
			fieldCampMesh->drawPart(pokemon2, partIndex, true);
		}

		for (const WorldLandmarkPlacement &landmark : LANDMARK_PLACEMENTS)
		{
			const float groundHeight = terrainHeightMap.heightAt(
				landmark.center.x, landmark.center.y);
			T = glm::translate(
				glm::mat4(1.0f),
				glm::vec3(landmark.center.x, groundHeight, landmark.center.y));
			R = glm::rotate(glm::mat4(1.0f), landmark.yaw,
			                glm::vec3(0.0f, 1.0f, 0.0f));
			S = glm::scale(glm::mat4(1.0f), glm::vec3(landmark.scale));
			M = T * R * S;
			glUniformMatrix4fv(pokemon2->getUniform("M"), 1, GL_FALSE, &M[0][0]);
			for (int partIndex = 0; partIndex < fieldLandmarkMesh->partCount();
			     ++partIndex)
			{
				const Shape::PartInfo &part =
					fieldLandmarkMesh->partInfo(partIndex);
				if (!landmarkPartMatches(landmark.kind, part.name))
				{
					continue;
				}
				glBindTexture(GL_TEXTURE_2D, landmarkTextureForPart(part.name));
				fieldLandmarkMesh->drawPart(pokemon2, partIndex, true);
			}
		}

		glBindTexture(GL_TEXTURE_2D, rockTex);
		glUniform1f(pokemon2->getUniform("surfaceDeform"), 0.18f);
		for (const WorldRockPlacement &rock : ROCK_PLACEMENTS)
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
			    !umbreons[i].isEcologicallyPresent() ||
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
			if (isPendingBattleTarget(umbreons[i]) &&
			    (battleVisualSample.phase != BattlePhase::TargetImpact ||
			     pendingPlayerMoveVolume.hitTarget))
			{
				applyWildBattlePose(pose, battleVisualSample, pendingWildMove);
			}
			const PokemonSpecies species = umbreons[i].getSpecies();
			const bool renderUmbreon = species == PokemonSpecies::Umbreon;
			const bool renderEevee = species == PokemonSpecies::Eevee;
			const float creatureScale = renderUmbreon ? 0.55f :
			                            (renderEevee ? 0.66f : 0.65f);
			const float groundOffset = renderUmbreon ? 0.34f :
			                           (renderEevee ? 0.38f : 0.47f);
			vec3 wildPosition = umbreons[i].getPos();
			if (isPendingBattleTarget(umbreons[i]))
			{
				const glm::vec3 lungeTarget = wildMoveReleased
				                                  ? pendingWildMoveVolume.impactPosition
				                                  : battlePlayerHitPosition;
				wildPosition += wildBattleLungeOffset(
					wildPosition, lungeTarget, battleVisualSample,
					pendingWildMove);
			}
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
			else if (renderEevee)
			{
				drawEevee(creatureRoot, pose);
			}
			else
			{
				drawBulbasaur(creatureRoot, pose);
			}
		}

		// charizard
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, fireTex);
		for (int i = 0; i <= FLYING_POKEMON; i++)
		{
			Pokemon &flyingPokemon = i == ALPHA_TARGET_INDEX
			                             ? alphaCharizard
			                             : charizards[i];
			if (flyingPokemon.getCaught() == 1 || flyingPokemon.isFainted() ||
			    !flyingPokemon.isEcologicallyPresent() ||
			    (isPendingCaptureTarget(flyingPokemon) &&
			     !captureVisualSample.pokemonVisible))
			{
				continue;
			}
			float distance = glm::length(glm::vec2(
				mypos.x - flyingPokemon.getPos().x,
				mypos.z - flyingPokemon.getPos().z));
			if (distance > 100)
			{
				continue;
			}
			const float flightSpeedRatio = flyingPokemon.getSpeedRatio();
			PokemonAnimationInput flightAnimationInput;
			flightAnimationInput.flying = true;
			flightAnimationInput.fleeing =
				flyingPokemon.getBehaviorState() == PokemonBehaviorState::Flee;
			flightAnimationInput.speedRatio = flightSpeedRatio;
			flightAnimationInput.verticalSpeedRatio =
				glm::clamp(flyingPokemon.getVelocity().y / 4.8f, -1.0f, 1.0f);
			flightAnimationInput.phase = flyingPokemon.getMotionPhase();
			PokemonAnimationPose flightPose =
				samplePokemonAnimation(flightAnimationInput);
			if (isPendingBattleTarget(flyingPokemon) &&
			    (battleVisualSample.phase != BattlePhase::TargetImpact ||
			     pendingPlayerMoveVolume.hitTarget))
			{
				applyWildBattlePose(
					flightPose, battleVisualSample, pendingWildMove);
			}
			vec3 flightPosition = flyingPokemon.getPos();
			flightPosition.y += flightPose.bodyBob;
			T = glm::translate(glm::mat4(1.0f), flightPosition);
			R = glm::rotate(glm::mat4(1.0f), flyingPokemon.getHeading(),
			                glm::vec3(0.0f, 1.0f, 0.0f));
			mat4 FlightPitch = glm::rotate(glm::mat4(1.0f), flightPose.bodyPitch,
			                               glm::vec3(1.0f, 0.0f, 0.0f));
			mat4 FlightRoll = glm::rotate(glm::mat4(1.0f), flightPose.bodyRoll,
			                              glm::vec3(0.0f, 0.0f, 1.0f));
			const float creatureScale =
				isAlphaPokemon(flyingPokemon) ? 2.25f : 1.6f;
			S = glm::scale(glm::mat4(1.0f), glm::vec3(creatureScale));
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
			const glm::vec3 playerImpactPosition =
				pendingPlayerMoveVolume.impactPosition;
			const glm::vec3 wildImpactPosition = wildMoveReleased
			                                        ? pendingWildMoveVolume.impactPosition
			                                        : battlePlayerHitPosition;
			const float wildVisualScale =
				battleMoveVisualScale(pendingWildMove.id);
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

			auto drawBattleVolume = [&](const glm::vec3 &position,
			                            const glm::vec3 &volumeScale,
			                            const BattleEffectPalette &palette,
			                            float opacity, float shellAmount) {
				const glm::mat4 translation =
					glm::translate(glm::mat4(1.0f), position);
				const glm::mat4 scale = glm::scale(
					glm::mat4(1.0f), glm::max(volumeScale, glm::vec3(0.01f)));
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
			auto drawBattleOrb = [&](const glm::vec3 &position, float orbScale,
			                         const BattleEffectPalette &palette,
			                         float opacity, float shellAmount) {
				drawBattleVolume(
					position, glm::vec3(orbScale), palette, opacity, shellAmount);
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
				if (pendingPlayerMove.id == BattleMoveId::Flamethrower)
				{
					constexpr int CONE_SEGMENTS = 7;
					for (int segment = 1; segment <= CONE_SEGMENTS; ++segment)
					{
						const float segmentFraction =
							static_cast<float>(segment) /
							static_cast<float>(CONE_SEGMENTS);
						const float progress =
							segmentFraction * battleVisualSample.phaseProgress;
						if (progress <= 0.0f)
						{
							continue;
						}
						const glm::vec3 position = glm::mix(
							battlePlayerOrigin, playerImpactPosition, progress);
						drawBattleOrb(
							position, (0.13f + progress * 0.36f) * pulse,
							playerPalette, 0.88f - progress * 0.18f, 0.06f);
					}
				}
				else
				{
					const bool translucentAirSlash =
						pendingPlayerMove.id == BattleMoveId::AirSlash;
					if (translucentAirSlash)
					{
						glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
					}
					const int trailCount = battleMoveTrailCount(pendingPlayerMove.id);
					for (int trailIndex = trailCount - 1; trailIndex >= 0; --trailIndex)
					{
						const float delayedProgress = glm::clamp(
							battleVisualSample.phaseProgress -
							    static_cast<float>(trailIndex) *
							        (translucentAirSlash ? 0.10f : 0.055f),
							0.0f, 1.0f);
						if (trailIndex > 0 && delayedProgress <= 0.0f)
						{
							continue;
						}
						const glm::vec3 position = battleProjectilePosition(
							battlePlayerOrigin, playerImpactPosition, delayedProgress);
						const float trailScale =
							1.0f - static_cast<float>(trailIndex) * 0.09f;
						const float trailOpacity = translucentAirSlash
							                           ? 0.64f -
							                                 static_cast<float>(trailIndex) * 0.12f
							                           : 0.92f -
							                                 static_cast<float>(trailIndex) * 0.10f;
						if (translucentAirSlash)
						{
							const float slashScale =
								pulse * playerVisualScale * trailScale;
							drawBattleVolume(
								position,
								glm::vec3(0.52f, 0.14f, 0.18f) * slashScale,
								playerPalette, trailOpacity, 0.14f);
						}
						else
						{
							drawBattleOrb(
								position,
								0.245f * pulse * playerVisualScale * trailScale,
								playerPalette, trailOpacity, 0.05f);
						}
					}
					if (translucentAirSlash)
					{
						glBlendFunc(GL_SRC_ALPHA, GL_ONE);
					}
				}
			}
			else if (battleVisualSample.phase == BattlePhase::TargetImpact)
			{
				const float progress = easedBattleProgress(
					battleVisualSample.phaseProgress);
				drawBattleOrb(playerImpactPosition,
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
				if (pendingWildMove.id == BattleMoveId::Bite)
				{
					const float progress = battleVisualSample.phaseProgress;
					const float visibility = glm::clamp(
						(progress - 0.22f) / 0.38f, 0.0f, 1.0f);
					const float jawGap =
						0.48f * (1.0f - easedBattleProgress(progress)) + 0.09f;
					const float opacity = visibility * (0.72f + pulse * 0.16f);
					const glm::vec3 biteCenter = wildImpactPosition;
					drawBattleOrb(
						biteCenter + glm::vec3(0.0f, jawGap, 0.0f),
						0.18f * pulse, wildPalette, opacity, 0.72f);
					drawBattleOrb(
						biteCenter - glm::vec3(0.0f, jawGap, 0.0f),
						0.18f * pulse, wildPalette, opacity, 0.72f);
				}
				else if (pendingWildMove.id == BattleMoveId::VineWhip)
				{
					constexpr int VINE_SEGMENTS = 5;
					for (int segment = 1; segment <= VINE_SEGMENTS; ++segment)
					{
						const float fraction = static_cast<float>(segment) /
						                       static_cast<float>(VINE_SEGMENTS);
						const float progress = fraction *
						                       battleVisualSample.phaseProgress;
						if (progress <= 0.0f)
						{
							continue;
						}
						drawBattleOrb(
							glm::mix(battleTargetPosition, wildImpactPosition, progress),
							0.13f * pulse * wildVisualScale, wildPalette,
							0.78f, 0.06f);
					}
				}
				else if (pendingWildMove.id == BattleMoveId::WingAttack)
				{
					glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
					for (int trailIndex = 2; trailIndex >= 0; --trailIndex)
					{
						const float progress = glm::clamp(
							battleVisualSample.phaseProgress -
							    static_cast<float>(trailIndex) * 0.11f,
							0.0f, 1.0f);
						if (trailIndex > 0 && progress <= 0.0f)
						{
							continue;
						}
						drawBattleOrb(
							battleProjectilePosition(battleTargetPosition,
							                         wildImpactPosition, progress),
							0.22f * pulse * wildVisualScale *
							    (1.0f - static_cast<float>(trailIndex) * 0.12f),
							wildPalette,
							0.64f - static_cast<float>(trailIndex) * 0.12f, 0.12f);
					}
					glBlendFunc(GL_SRC_ALPHA, GL_ONE);
				}
				else
				{
					const glm::vec3 position = battleProjectilePosition(
						battleTargetPosition, wildImpactPosition,
						battleVisualSample.phaseProgress);
					drawBattleOrb(
						position, 0.225f * pulse * wildVisualScale,
						wildPalette, 0.90f, 0.08f);
				}
			}
			else if (battleVisualSample.phase == BattlePhase::PlayerImpact)
			{
				const float progress = easedBattleProgress(
					battleVisualSample.phaseProgress);
				const bool wildConnected = pendingWildMoveVolume.hitTarget;
				const float impactScale = !wildConnected
				                              ? 0.20f + progress * 0.40f
				                              : playerEvadedCurrentCounter
				                              ? 0.18f + progress * 0.34f
				                              : 0.28f + progress * 0.68f;
				const float impactOpacity = !wildConnected
				                                ? (1.0f - progress) * 0.35f
				                                : playerEvadedCurrentCounter
				                                ? (1.0f - progress) * 0.28f
				                                : (1.0f - progress) * 0.78f;
				drawBattleOrb(wildImpactPosition, impactScale * wildVisualScale,
				              wildPalette,
				              impactOpacity,
				              !wildConnected ? 0.30f
				                             : (playerEvadedCurrentCounter ? 0.18f
				                                                            : 0.94f));
			}

			glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
			glDepthMask(GL_TRUE);
			battleEffectShader->unbind();
		}
	}

	void frame()
	{
		const double frameNow = glfwGetTime();
		double frameDeltaSeconds =
			lastFrameTime >= 0.0 ? frameNow - lastFrameTime : 0.0;
		lastFrameTime = frameNow;
		if (resetRequested)
		{
			resetGame();
			frameDeltaSeconds = 0.0;
		}
		gameSession.advance(frameDeltaSeconds,
		                    [this](const GameSessionStep &step) {
			                    simulateStep(step);
		                    });
		render();
		glfwSwapBuffers(windowManager->getHandle());
		glfwPollEvents();
	}

	bool configureNativeQaScenario(
		const std::string &scenario, std::string &errorMessage)
	{
		if (scenario == "camp")
		{
			resetPlayerAtFieldCamp();
			return true;
		}
		if (scenario == "trails")
		{
			for (const WorldInterestPointPlacement &point : INTEREST_POINT_PLACEMENTS)
			{
				bool onTrail = false;
				for (const WorldTrailSegment &segment : TRAIL_SEGMENTS)
				{
					onTrail = onTrail || worldTrailCoverage(point.center, segment) > 0.99f;
				}
				if (!onTrail)
				{
					errorMessage =
						"Native QA interest point is disconnected from the trail network.";
					return false;
				}
			}
			mycam.resetAt(glm::vec3(0.0f, 13.0f, 18.0f), 0.0f);
			setStatus(
				"Native QA: camp trail forks toward Moonshadow and Redrock observation points.");
			return true;
		}
		if (scenario == "alpha-nest" || scenario == "alpha-capture")
		{
			const WorldInterestPointPlacement *nest = alphaNestPoint();
			if (!nest)
			{
				errorMessage = "Native QA Alpha nest world anchor is missing.";
				return false;
			}
			const float nestGround = terrainHeightMap.heightAt(
				nest->center.x, nest->center.y);
			float maximumLandingHeightDelta = 0.0f;
			for (int sample = 0; sample < 8; ++sample)
			{
				const float angle = static_cast<float>(sample) * 3.1415926f / 4.0f;
				const glm::vec2 offset(std::cos(angle) * 1.6f,
				                       std::sin(angle) * 1.6f);
				maximumLandingHeightDelta = std::max(
					maximumLandingHeightDelta,
					std::fabs(terrainHeightMap.heightAt(
					              nest->center.x + offset.x,
					              nest->center.y + offset.y) -
					          nestGround));
			}
			if (maximumLandingHeightDelta > 0.55f)
			{
				errorMessage =
					"Native QA Alpha nest is too steep for a readable landing encounter.";
				return false;
			}

			caughtCount = 0;
			defeatedCount = 0;
			pokeballs = RESEARCH_STARTING_POKEBALLS;
			playerHealth = battleStatsFor(PokemonSpecies::Charizard).maximumHealth;
			researchSubmitted = false;
			gameFinished = false;
			battleSequenceActive = false;
			resetCaptureInteraction();
			clearPerfectCounterWindow();
			researchProgress = ResearchMissionProgress();
			researchProgress.moonshadowTrackSurveys = 1;
			researchProgress.redrockLookoutSurveys = 1;
			alphaNestProgress = AlphaNestProgress();
			hideAlphaPokemon();
			for (int index = 0; index < NUM_POKEMON; ++index)
			{
				umbreons[index] = Pokemon(0, index);
				umbreons[index].setCaught(1);
			}
			for (int index = 0; index < FLYING_POKEMON; ++index)
			{
				charizards[index] = Pokemon(1, index);
				charizards[index].setCaught(1);
			}

			mycam.resetAt(
				glm::vec3(nest->center.x, nestGround, nest->center.y), 0.0f);
			if (!interactWithWorld(false) || !alphaNestProgress.active ||
			    alphaNestProgress.resolved ||
			    !alphaCharizard.isEcologicallyPresent())
			{
				errorMessage =
					"Native QA production F interaction did not activate Alpha Charizard.";
				return false;
			}
			const HudTelemetry activationTelemetry =
				collectHudTelemetry(gameplayTime());
			std::string telemetryError;
			if (!activationTelemetry.regional.visible ||
			    !activationTelemetry.regional.alpha ||
			    activationTelemetry.regional.recorded ||
			    activationTelemetry.regional.ready ||
			    !validateHudTelemetry(activationTelemetry, &telemetryError))
			{
				errorMessage = "Native QA Alpha activation HUD contract failed" +
				               (telemetryError.empty()
				                    ? std::string(".")
				                    : std::string(": ") + telemetryError);
				return false;
			}

			const glm::vec2 cameraGroundPosition =
				nest->center + glm::vec2(6.5f, 12.0f);
			mycam.resetAt(
				glm::vec3(cameraGroundPosition.x,
				          alphaCharizard.getPos().y - 1.5f,
				          cameraGroundPosition.y),
				0.0f);
			currentTarget = PokemonTargetSelection();
			refreshTarget();
			const FieldRadarContact radarContact = nearestResearchRadarContact();
			Pokemon *radarPokemon = pokemonForRadarContact(radarContact);
			const HudTelemetry telemetry = collectHudTelemetry(gameplayTime());
			if (targetedPokemon() != &alphaCharizard ||
			    radarPokemon != &alphaCharizard || !telemetry.target.visible ||
			    telemetry.target.name != "Alpha Charizard" ||
			    !telemetry.radar.visible ||
			    telemetry.radar.name != "Alpha Charizard" ||
			    !validateHudTelemetry(telemetry, &telemetryError))
			{
				errorMessage = "Native QA Alpha target, radar, or HUD contract failed" +
				               (telemetryError.empty()
				                    ? std::string(".")
				                    : std::string(": ") + telemetryError);
				return false;
			}
			if (scenario == "alpha-capture")
			{
				for (int index = 0; index < NUM_POKEMON; ++index)
				{
					umbreons[index].setCaught(0);
				}
				for (int index = 0; index < FLYING_POKEMON; ++index)
				{
					charizards[index].setCaught(0);
				}
				const int ordinaryCaughtBefore = caughtCount;
				const int ordinaryDefeatedBefore = defeatedCount;
				if (!alphaCharizard.setHealth(std::max(
				        1, alphaCharizard.getMaximumHealth() / 8)))
				{
					errorMessage =
						"Native QA could not weaken Alpha Charizard for capture.";
					return false;
				}
				captureProjectileOrigin =
					alphaCharizard.getPos() + glm::vec3(0.0f, 0.0f, 8.0f);
				CaptureSweepHit hit;
				hit.hit = true;
				hit.fraction = 0.5f;
				hit.position = captureCollisionCenter(alphaCharizard);
				hit.quality = 1.0f;
				captureRandom = CaptureRandom(0x1234u);
				beginCaptureSequenceFromHit(
					alphaCharizard, hit, gameplayTime());
				if (!captureSequenceActive ||
				    pendingCaptureTarget != &alphaCharizard ||
				    !pendingCaptureResult.captured)
				{
					errorMessage =
						"Native QA weakened Alpha capture did not succeed.";
					return false;
				}
				finishCaptureSequence(false);
				const GameSaveData resolvedSave = currentGameSave();
				std::string saveError;
				if (!alphaNestProgress.resolved || alphaNestProgress.active ||
				    alphaCharizard.getCaught() == 0 ||
				    alphaCharizard.isEcologicallyPresent() ||
				    caughtCount != ordinaryCaughtBefore ||
				    defeatedCount != ordinaryDefeatedBefore ||
				    !resolvedSave.alphaNestResolved ||
				    !validateGameSave(resolvedSave, gameSaveLimits(), &saveError) ||
				    encodeGameSave(resolvedSave, gameSaveLimits()).empty())
				{
					errorMessage = "Native QA Alpha capture resolution failed" +
					               (saveError.empty()
					                    ? std::string(".")
					                    : std::string(": ") + saveError);
					return false;
				}
				for (int index = 0; index < NUM_POKEMON; ++index)
				{
					umbreons[index].setCaught(1);
				}
				for (int index = 0; index < FLYING_POKEMON; ++index)
				{
					charizards[index].setCaught(1);
				}
				const glm::vec2 resolvedViewPosition =
					nest->center + glm::vec2(0.0f, 7.0f);
				mycam.resetAt(
					glm::vec3(
						resolvedViewPosition.x,
						terrainHeightMap.heightAt(
							resolvedViewPosition.x, resolvedViewPosition.y),
						resolvedViewPosition.y),
					0.0f);
				currentTarget = PokemonTargetSelection();
				setStatus(
					"Native QA: weakened Alpha capture resolved without changing ordinary survey counters.");
				return true;
			}
			setStatus(
				"Native QA: production F interaction activated the visible Alpha Charizard encounter.");
			return true;
		}
		if (scenario == "moonshadow-survey" ||
		    scenario == "redrock-survey")
		{
			const WorldInterestPointKind requestedKind =
				scenario == "moonshadow-survey"
				    ? WorldInterestPointKind::MoonshadowTracks
				    : WorldInterestPointKind::RedrockLookout;
			const WorldInterestPointPlacement *site = nullptr;
			for (const WorldInterestPointPlacement &point : INTEREST_POINT_PLACEMENTS)
			{
				if (point.kind == requestedKind)
				{
					site = &point;
					break;
				}
			}
			if (!site)
			{
				errorMessage = "Native QA regional research site is missing.";
				return false;
			}

			qaLightingCyclePhaseOverride =
				requestedKind == WorldInterestPointKind::MoonshadowTracks
				    ? 0.75f
				    : 0.25f;
			caughtCount = 0;
			pokeballs = RESEARCH_STARTING_POKEBALLS;
			playerHealth =
				battleStatsFor(PokemonSpecies::Charizard).maximumHealth;
			researchSubmitted = false;
			gameFinished = false;
			battleSequenceActive = false;
			resetCaptureInteraction();
			researchProgress = ResearchMissionProgress();
			const float siteGround = terrainHeightMap.heightAt(
				site->center.x, site->center.y);
			float maximumLandingHeightDelta = 0.0f;
			for (int sample = 0; sample < 8; ++sample)
			{
				const float angle = static_cast<float>(sample) * 3.1415926f / 4.0f;
				const glm::vec2 offset(std::cos(angle) * 1.4f,
				                       std::sin(angle) * 1.4f);
				maximumLandingHeightDelta = std::max(
					maximumLandingHeightDelta,
					std::fabs(terrainHeightMap.heightAt(
					              site->center.x + offset.x,
					              site->center.y + offset.y) -
					          siteGround));
			}
			if (maximumLandingHeightDelta > 0.5f)
			{
				errorMessage =
					"Native QA regional marker is too steep for a readable landing site.";
				return false;
			}
			mycam.resetAt(
				glm::vec3(site->center.x, siteGround, site->center.y), 0.0f);
			if (!interactWithWorld(false))
			{
				errorMessage =
					"Native QA F interaction did not record the regional site.";
				return false;
			}
			const bool moonRecorded =
				researchProgress.moonshadowTrackSurveys == 1;
			const bool redRecorded =
				researchProgress.redrockLookoutSurveys == 1;
			if ((requestedKind == WorldInterestPointKind::MoonshadowTracks &&
			     (!moonRecorded || redRecorded)) ||
			    (requestedKind == WorldInterestPointKind::RedrockLookout &&
			     (!redRecorded || moonRecorded)))
			{
				errorMessage =
					"Native QA regional interaction credited the wrong objective.";
				return false;
			}
			interactWithWorld(false);
			if (researchProgress.moonshadowTrackSurveys > 1 ||
			    researchProgress.redrockLookoutSurveys > 1)
			{
				errorMessage =
					"Native QA repeated interaction duplicated regional credit.";
				return false;
			}

			const glm::vec2 cameraGroundPosition =
				site->center + glm::vec2(0.0f, 4.8f);
			mycam.resetAt(
				glm::vec3(
					cameraGroundPosition.x,
					terrainHeightMap.heightAt(cameraGroundPosition.x,
					                          cameraGroundPosition.y),
					cameraGroundPosition.y),
				0.0f);
			const HudTelemetry telemetry = collectHudTelemetry(gameplayTime());
			std::string telemetryError;
			const std::size_t expectedObjectiveIndex =
				requestedKind == WorldInterestPointKind::MoonshadowTracks ? 3u : 4u;
			if (!telemetry.regional.visible || !telemetry.regional.recorded ||
			    telemetry.regional.ready ||
			    telemetry.completedMissionObjectives != 1 ||
			    telemetry.missionObjectives[expectedObjectiveIndex].current != 1 ||
			    !validateHudTelemetry(telemetry, &telemetryError))
			{
				errorMessage = "Native QA regional HUD did not publish the recorded state" +
				               (telemetryError.empty()
				                    ? std::string(".")
				                    : std::string(": ") + telemetryError);
				return false;
			}
			setStatus(
				requestedKind == WorldInterestPointKind::MoonshadowTracks
				    ? "Native QA: Moonshadow night tracks recorded through the production F interaction."
				    : "Native QA: Redrock lookout recorded through the production F interaction.");
			return true;
		}
		if (scenario == "ecology-day" || scenario == "ecology-night")
		{
			const bool day = scenario == "ecology-day";
			qaLightingCyclePhaseOverride = day ? 0.25f : 0.75f;
			const float daylight = worldLightingAt(gameplayTime()).daylight;
			int umbreonPresent = 0;
			int meadowPresent = 0;
			for (int index = 0; index < NUM_POKEMON; ++index)
			{
				umbreons[index] = Pokemon(0, index);
				const bool present = pokemonEcologySlotPresent(
					umbreons[index].getSpecies(), umbreons[index].getID(), daylight);
				umbreons[index].setEcologicallyPresent(present);
				if (present &&
				    umbreons[index].getSpecies() == PokemonSpecies::Umbreon)
				{
					++umbreonPresent;
				}
				else if (present)
				{
					++meadowPresent;
				}
			}
			for (int index = 0; index < FLYING_POKEMON; ++index)
			{
				charizards[index] = Pokemon(1, index);
				charizards[index].setEcologicallyPresent(
					pokemonEcologySlotPresent(PokemonSpecies::Charizard, index,
					                          daylight));
			}
			if ((day && meadowPresent <= umbreonPresent) ||
			    (!day && umbreonPresent <= meadowPresent))
			{
				errorMessage =
					"Native QA ecology pool did not match its intended time band.";
				return false;
			}
			mycam.resetAt(glm::vec3(-7.0f, 8.0f, 4.0f), 0.22f);
			currentTarget = PokemonTargetSelection();
			refreshPokemonCollisionObstacles();
			setStatus(
				day ? "Native QA: daytime meadow species active; Umbreon sheltering."
				    : "Native QA: nighttime Umbreon emerged; meadow wildlife sheltering.");
			return true;
		}
		Pokemon *target = nullptr;
		for (int index = 0; index < NUM_POKEMON; ++index)
		{
			umbreons[index].setCaught(1);
		}
		for (int index = 0; index < FLYING_POKEMON; ++index)
		{
			charizards[index].setCaught(1);
		}
		auto placeGroundEncounter = [&](const glm::vec3 &playerPosition,
		                                const glm::vec3 &targetPosition) {
			target = &umbreons[0];
			target->setCaught(0);
			target->restoreHealth();
			target->setPosition(targetPosition);
			const float groundHeight = terrainHeightMap.heightAt(
				playerPosition.x, playerPosition.z);
			mycam.resetAt(
				glm::vec3(playerPosition.x, groundHeight, playerPosition.z), 0.0f);
		};

		if (scenario == "landmarks")
		{
			mycam.resetAt(glm::vec3(0.0f, 10.0f, 10.0f), 0.0f);
			setStatus(
				"Native QA: Moonshadow Edge and Redrock Highlands visible from the air.");
			return true;
		}
		if (scenario == "umbreon" || scenario == "perfect-dodge")
		{
			placeGroundEncounter(glm::vec3(0.0f, 0.0f, 20.0f),
			                     glm::vec3(0.0f, 0.0f, 15.8f));
		}
		else if (scenario == "charizard")
		{
			target = &charizards[0];
			target->setCaught(0);
			target->restoreHealth();
			target->setPosition(glm::vec3(0.0f, 19.5f, 8.0f));
			mycam.resetAt(glm::vec3(0.0f, 18.0f, 20.0f), 0.0f);
		}
		else if (scenario == "capture-aim" || scenario == "capture-hit")
		{
			placeGroundEncounter(glm::vec3(0.0f, 0.0f, 20.0f),
			                     glm::vec3(
				                     0.0f, 0.0f,
				                     scenario == "capture-hit" ? 13.0f : 12.0f));
		}
		else if (scenario == "ember" || scenario == "air-slash" ||
		         scenario == "flamethrower")
		{
			placeGroundEncounter(glm::vec3(0.0f, 0.0f, 20.0f),
			                     glm::vec3(0.0f, 0.0f, 14.0f));
		}
		else if (scenario == "cover-blocked")
		{
			placeGroundEncounter(glm::vec3(-5.0f, 0.0f, 21.0f),
			                     glm::vec3(-5.0f, 0.0f, 9.0f));
		}
		else
		{
				errorMessage = "Unknown Native QA scenario: " + scenario +
				               ". Expected camp, trails, alpha-nest, alpha-capture, moonshadow-survey, redrock-survey, "
				               "landmarks, ecology-day, ecology-night, "
			               "capture-aim, capture-hit, umbreon, "
			               "charizard, ember, "
			               "air-slash, flamethrower, perfect-dodge, or "
			               "cover-blocked.";
			return false;
		}

		playerHealth = battleStatsFor(PokemonSpecies::Charizard).maximumHealth;
		gameFinished = false;
		resetCaptureInteraction();
		battleSequenceActive = false;
		pendingBattleTarget = nullptr;
		clearPerfectCounterWindow();
		nextWildEncounterTime = -1.0;
		refreshPokemonCollisionObstacles();
		const double now = gameplayTime();
		if (scenario == "capture-aim" || scenario == "capture-hit")
		{
			refreshTarget();
			if (targetedPokemon() != target)
			{
				errorMessage = "Native QA capture target could not be locked.";
				return false;
			}
			beginCaptureAim();
			if (scenario == "capture-hit")
			{
				captureRandom = CaptureRandom(0x1234u);
				captureAimStarted = now;
				const int pokeballsBeforeThrow = pokeballs;
				releaseCaptureThrow(false);
				for (int step = 1;
				     step <= 180 && captureProjectile.active &&
				     !captureSequenceActive;
				     ++step)
				{
					updateCaptureProjectile(
						1.0f / 60.0f, now + static_cast<double>(step) / 60.0);
				}
				if (!captureSequenceActive || pendingCaptureTarget != target ||
				    !pendingCaptureResult.captured ||
				    pokeballs != pokeballsBeforeThrow - 1)
				{
					errorMessage =
						"Native QA capture throw did not produce a successful target hit.";
					return false;
				}
				captureSequenceStarted =
					now - static_cast<double>(captureThrowFlightPhaseDuration() + 0.15f);
				const CaptureSequenceSample sample = currentCaptureSample(now);
				if (sample.phase != CapturePhase::Absorbing ||
				    sample.pokemonVisible || !sample.ballVisible)
				{
					errorMessage =
						"Native QA target hit did not reach the absorbing phase.";
					return false;
				}
				setStatus(
					"Native QA: minimum-charge Poke Ball hit and began capture.");
				return true;
			}
			captureAimStarted =
				now - static_cast<double>(captureProjectileConfig.fullChargeSeconds) *
				          0.72;
			const float charge = currentCaptureCharge(now);
			const std::vector<glm::vec3> prediction = capturePredictionPath(now);
			if (!captureAiming || charge < 0.719f || charge > 0.721f ||
			    prediction.size() < 6)
			{
				errorMessage =
					"Native QA capture aim did not produce the expected charged arc.";
				return false;
			}
			setStatus("Native QA: Poke Ball aim held at 72% charge.");
			return true;
		}

		if (scenario == "ember" || scenario == "air-slash" ||
		    scenario == "flamethrower" || scenario == "cover-blocked")
		{
			const int moveSlot = scenario == "air-slash"
			                         ? 1
			                         : (scenario == "flamethrower" ? 2 : 0);
			selectPlayerMove(moveSlot);
			refreshTarget();
			if (targetedPokemon() != target)
			{
				errorMessage = "Native QA player-move target could not be locked.";
				return false;
			}
			attackTargetedPokemon();
			if (!battleSequenceActive)
			{
				errorMessage = "Unable to start the requested Native QA player move.";
				return false;
			}
			const float qaElapsed = pendingPlayerMove.timing.startupSeconds +
				(scenario == "cover-blocked"
				     ? pendingPlayerMove.timing.activeSeconds + 0.09f
				     : pendingPlayerMove.timing.activeSeconds * 0.58f);
			battleSequenceStarted = now - static_cast<double>(qaElapsed);
			updateBattleSequence(now);
			const BattleSequenceSample sample = currentBattleSample(now);
			if (scenario == "cover-blocked")
			{
				if (sample.phase != BattlePhase::TargetImpact ||
				    pendingPlayerMoveVolume.impact !=
				        BattleMoveImpactKind::Obstacle ||
				    pendingPlayerMoveVolume.hitTarget)
				{
					errorMessage =
						"Native QA cover scene did not resolve an obstacle impact.";
					return false;
				}
			}
			else if (sample.phase != BattlePhase::PlayerProjectile ||
			         !playerMoveReleased || !pendingPlayerMoveVolume.hitTarget)
			{
				std::ostringstream diagnostic;
				diagnostic
					<< "Native QA player move did not reach a target-bound active phase"
					<< " (phase=" << static_cast<int>(sample.phase)
					<< ", released=" << playerMoveReleased
					<< ", impact="
					<< static_cast<int>(pendingPlayerMoveVolume.impact)
					<< ", travel=" << pendingPlayerMoveVolume.travelDistance
					<< ", originY=" << battlePlayerOrigin.y
					<< ", targetY=" << battleTargetPosition.y << ").";
				errorMessage = diagnostic.str();
				return false;
			}
			return true;
		}

		startWildEncounter(*target, now);
		if (!battleSequenceActive)
		{
			errorMessage = "Unable to start the requested Native QA encounter.";
			return false;
		}
		if (scenario == "perfect-dodge")
		{
			const glm::vec3 dodgeOrigin = mypos;
			if (!mycam.requestDodge())
			{
				errorMessage = "Native QA perfect dodge could not be requested.";
				return false;
			}
			mycam.process(1.0 / 60.0);
			dodgeEffectOrigin = dodgeOrigin;
			dodgeEffectStarted = now - 0.18;
			const float qaElapsed = pendingWildMove.timing.startupSeconds +
			                        pendingWildMove.timing.activeSeconds + 0.04f;
			battleSequenceStarted = now - static_cast<double>(qaElapsed);
			updateBattleSequence(now);
			const BattleSequenceSample sample = currentBattleSample(now);
			if (sample.phase != BattlePhase::PlayerImpact ||
			    !playerEvadedCurrentCounter || !perfectDodgePending ||
			    !mycam.isInvulnerable())
			{
				errorMessage =
					"Native QA encounter did not resolve a real perfect dodge.";
				return false;
			}
			return true;
		}
		const float qaElapsed = pendingWildMove.timing.startupSeconds +
		                        pendingWildMove.timing.activeSeconds * 0.58f;
		battleSequenceStarted = now - static_cast<double>(qaElapsed);
		updateBattleSequence(now);
		const BattleSequenceSample sample = currentBattleSample(now);
		if (sample.phase != BattlePhase::WildProjectile || !wildMoveReleased)
		{
			errorMessage = "Native QA encounter did not reach its active phase.";
			return false;
		}
		return true;
	}
};
#ifdef __EMSCRIPTEN__
void webMainLoop(void *userData)
{
	static_cast<Application *>(userData)->frame();
}
#else
bool captureFrontFramebuffer(
	GLFWwindow *window, const std::string &outputPath, std::string &errorMessage)
{
	int width = 0;
	int height = 0;
	glfwGetFramebufferSize(window, &width, &height);
	if (width <= 0 || height <= 0)
	{
		errorMessage = "The Native framebuffer has no drawable area.";
		return false;
	}
	std::vector<unsigned char> pixels(
		static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 3u);
	glFinish();
	glPixelStorei(GL_PACK_ALIGNMENT, 1);
	glReadBuffer(GL_FRONT);
	glReadPixels(0, 0, width, height, GL_RGB, GL_UNSIGNED_BYTE, pixels.data());
	glPixelStorei(GL_PACK_ALIGNMENT, 4);
	const GLenum readError = glGetError();
	if (readError != GL_NO_ERROR)
	{
		errorMessage = "OpenGL framebuffer read failed with error " +
		               std::to_string(static_cast<unsigned int>(readError)) + ".";
		return false;
	}
	return writeBottomUpRgbPpm(
		outputPath, width, height, pixels, &errorMessage);
}
#endif
//******************************************************************************************
int main(int argc, char **argv)
{
	srand(time(NULL));
	const bool resourceDirectoryExplicit = argc >= 2;
	std::string resourceDir = "resources"; // Where the resources are loaded from
	std::string qaCapturePath;
	std::string qaScenario = "camp";
	if (argc >= 2)
	{
		resourceDir = argv[1];
	}
#ifndef __EMSCRIPTEN__
	for (int index = 2; index < argc; ++index)
	{
		const std::string argument = argv[index];
		if (argument == "--qa-capture" && index + 1 < argc)
		{
			qaCapturePath = argv[++index];
		}
		else if (argument == "--qa-scenario" && index + 1 < argc)
		{
			qaScenario = argv[++index];
		}
		else
		{
			std::cerr << "Unknown or incomplete Native QA argument: "
			          << argument << std::endl;
			return 1;
		}
	}
#endif
	auto hasResources = [](const std::string &directory) {
		std::ifstream sphere(directory + "/sphere.obj");
		return sphere.good();
	};
	const std::string locatedResourceDirectory = locateResourceDirectory(
		argc > 0 ? argv[0] : std::string(), resourceDir,
		resourceDirectoryExplicit, hasResources);
	if (locatedResourceDirectory.empty())
	{
		std::cerr << "Resource directory not found: " << resourceDir << std::endl;
		std::cerr << "Run from the project root or pass the resources path as an argument." << std::endl;
		return 1;
	}
	resourceDir = locatedResourceDirectory;

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
	if (!qaCapturePath.empty())
	{
		std::string scenarioError;
		if (!application->configureNativeQaScenario(qaScenario, scenarioError))
		{
			std::cerr << "Unable to configure Native QA scene: "
			          << scenarioError << std::endl;
			windowManager->shutdown();
			delete windowManager;
			delete application;
			return 1;
		}
	}

	// Native builds own the event loop. Browsers must return control to the
	// JavaScript event loop, so Emscripten calls one frame at a time instead.
#ifdef __EMSCRIPTEN__
	emscripten_set_main_loop_arg(webMainLoop, application, 0, 1);
#else
	if (!qaCapturePath.empty())
	{
		application->frame();
		std::string captureError;
		const bool captured = captureFrontFramebuffer(
			windowManager->getHandle(), qaCapturePath, captureError);
		if (captured)
		{
			std::cout << "Native QA frame captured: " << qaCapturePath << std::endl;
		}
		else
		{
			std::cerr << "Unable to capture Native QA frame: "
			          << captureError << std::endl;
		}
		windowManager->shutdown();
		delete windowManager;
		delete application;
		return captured ? 0 : 1;
	}
	while (!glfwWindowShouldClose(windowManager->getHandle()))
	{
		application->frame();
	}

	// Quit program.
	windowManager->shutdown();
#endif
	return 0;
}
