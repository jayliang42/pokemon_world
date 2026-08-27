#pragma once

#include <cstdint>

#include <glm/glm.hpp>

#include "PokemonNavigation.h"
#include "PokemonEcology.h"
#include "PokemonSpecies.h"

enum class PokemonBehaviorState
{
	Idle,
	Wander,
	Alert,
	Pursue,
	Flee,
};

struct PokemonBehaviorEvents
{
	bool alertStarted = false;
	bool attackReady = false;
};

class Pokemon
{
public:
	Pokemon();
	Pokemon(int flyPokemon, int pokemonID, std::uint32_t seed = 0);

	PokemonBehaviorEvents update(double deltaSeconds);
	PokemonBehaviorEvents update(double deltaSeconds,
	                             const glm::vec3 &playerPosition);
	PokemonBehaviorEvents update(
		double deltaSeconds, const glm::vec3 &playerPosition,
		const std::vector<PokemonNavigationBlocker> &navigationBlockers,
		float playerNoise = 1.0f, bool lineOfSightClear = true,
		float daylight = 1.0f);
	void setCaught(int flag);
	void startle();
	void receiveCompanionAlert();
	void coolDownAfterAttack();
	int applyDamage(int amount);
	bool setHealth(int health);
	void restoreHealth();
	void setEcologicallyPresent(bool present);
	void setDestination(float x, float y, float z);
	void investigateAt(float x, float y, float z);
	void setPosition(const glm::vec3 &position);

	int getCaught() const;
	int getHealth() const;
	int getMaximumHealth() const;
	float getHealthRatio() const;
	bool isFainted() const;
	bool isEcologicallyPresent() const;
	glm::vec3 getPos() const;
	glm::vec3 getVelocity() const;
	float getHeading() const;
	float getMotionPhase() const;
	float getSpeedRatio() const;
	float getAlertness() const;
	bool canSeePlayer() const;
	bool canHearPlayer() const;
	PokemonBehaviorState getBehaviorState() const;
	bool isThreatening() const;
	bool isFlying() const;
	int getID() const;
	PokemonSpecies getSpecies() const;

private:
	float random(float minimum, float maximum);
	void chooseIdle();
	void chooseWanderDestination();
	void enterFlee();
	PokemonBehaviorEvents updateBehavior(float deltaSeconds,
	                                     const glm::vec3 &playerPosition,
	                                     float playerNoise,
	                                     bool lineOfSightClear,
	                                     const PokemonEcologySample &ecology);
	glm::vec3 desiredVelocity(const glm::vec3 &playerPosition,
	                         float wanderSpeedScale) const;
	void integrateMotion(
		float deltaSeconds, const glm::vec3 &desiredVelocity,
		const std::vector<PokemonNavigationBlocker> &navigationBlockers);
	bool isAtDestination() const;
	float navigationRadius() const;

	glm::vec3 position_ = glm::vec3(0.0f);
	glm::vec3 destination_ = glm::vec3(0.0f);
	glm::vec3 velocity_ = glm::vec3(0.0f);
	float heading_ = 0.0f;
	float targetHeading_ = 0.0f;
	float motionPhase_ = 0.0f;
	float stateTimer_ = 0.0f;
	float age_ = 0.0f;
	float alertness_ = 0.0f;
	bool caught_ = false;
	bool ecologicallyPresent_ = true;
	bool flying_ = false;
	bool playerVisible_ = false;
	bool playerHeard_ = false;
	bool investigationDestinationPending_ = false;
	int pokemonID_ = -1;
	int health_ = 1;
	int maximumHealth_ = 1;
	std::uint32_t randomState_ = 1;
	PokemonBehaviorState behaviorState_ = PokemonBehaviorState::Idle;
};
