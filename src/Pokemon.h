#pragma once

#include <cstdint>

#include <glm/glm.hpp>

#include "PokemonNavigation.h"
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
		const std::vector<PokemonNavigationBlocker> &navigationBlockers);
	void setCaught(int flag);
	void startle();
	void coolDownAfterAttack();
	int applyDamage(int amount);
	bool setHealth(int health);
	void restoreHealth();
	void setDestination(float x, float y, float z);
	void setPosition(const glm::vec3 &position);

	int getCaught() const;
	int getHealth() const;
	int getMaximumHealth() const;
	float getHealthRatio() const;
	bool isFainted() const;
	glm::vec3 getPos() const;
	glm::vec3 getVelocity() const;
	float getHeading() const;
	float getMotionPhase() const;
	float getSpeedRatio() const;
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
	                                     const glm::vec3 &playerPosition);
	glm::vec3 desiredVelocity(const glm::vec3 &playerPosition) const;
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
	bool caught_ = false;
	bool flying_ = false;
	int pokemonID_ = -1;
	int health_ = 1;
	int maximumHealth_ = 1;
	std::uint32_t randomState_ = 1;
	PokemonBehaviorState behaviorState_ = PokemonBehaviorState::Idle;
};
