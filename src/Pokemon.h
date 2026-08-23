#pragma once

#include <cstdint>

#include <glm/glm.hpp>

enum class PokemonBehaviorState
{
	Idle,
	Wander,
	Flee,
};

class Pokemon
{
public:
	Pokemon();
	Pokemon(int flyPokemon, int pokemonID, std::uint32_t seed = 0);

	void update(double deltaSeconds);
	void update(double deltaSeconds, const glm::vec3 &playerPosition);
	void setCaught(int flag);
	void setDestination(float x, float y, float z);
	void setPosition(const glm::vec3 &position);

	int getCaught() const;
	glm::vec3 getPos() const;
	glm::vec3 getVelocity() const;
	float getHeading() const;
	float getMotionPhase() const;
	float getSpeedRatio() const;
	PokemonBehaviorState getBehaviorState() const;
	bool isFlying() const;
	int getID() const;

private:
	float random(float minimum, float maximum);
	void chooseIdle();
	void chooseWanderDestination();
	void enterFlee();
	void updateBehavior(float deltaSeconds, const glm::vec3 &playerPosition);
	glm::vec3 desiredVelocity(const glm::vec3 &playerPosition) const;
	void integrateMotion(float deltaSeconds, const glm::vec3 &desiredVelocity);
	bool isAtDestination() const;

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
	std::uint32_t randomState_ = 1;
	PokemonBehaviorState behaviorState_ = PokemonBehaviorState::Idle;
};
