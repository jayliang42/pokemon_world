#include "Pokemon.h"

#include "BattleMechanics.h"
#include "PokemonAnimation.h"

#include <algorithm>
#include <cmath>

namespace
{
constexpr float PI = 3.14159265358979323846f;
constexpr float TWO_PI = PI * 2.0f;
constexpr float MAX_DELTA_SECONDS = 0.05f;
constexpr float SPAWN_LIMIT = 44.0f;
constexpr float FIELD_LIMIT = 46.0f;
constexpr float SAFE_SPAWN_RADIUS = 8.0f;
constexpr float MIN_FLIGHT_HEIGHT = 12.0f;
constexpr float MAX_FLIGHT_HEIGHT = 30.0f;
constexpr float GROUND_WANDER_SPEED = 1.65f;
constexpr float GROUND_FLEE_SPEED = 4.2f;
constexpr float FLYING_WANDER_SPEED = 2.8f;
constexpr float FLYING_FLEE_SPEED = 4.8f;
constexpr float GROUND_ACCELERATION = 5.5f;
constexpr float FLYING_ACCELERATION = 4.0f;
constexpr float FLEE_ENTER_DISTANCE = 5.5f;
constexpr float FLEE_EXIT_DISTANCE = 9.0f;
constexpr float ARRIVAL_DISTANCE = 0.65f;

float clampValue(float value, float minimum, float maximum)
{
	return std::max(minimum, std::min(maximum, value));
}

glm::vec3 moveToward(const glm::vec3 &current, const glm::vec3 &target,
	                  float maximumDelta)
{
	const glm::vec3 difference = target - current;
	const float distance = glm::length(difference);
	if (distance <= maximumDelta || distance <= 0.0001f)
	{
		return target;
	}
	return current + difference / distance * maximumDelta;
}

float shortestAngle(float angle)
{
	while (angle > PI)
	{
		angle -= TWO_PI;
	}
	while (angle < -PI)
	{
		angle += TWO_PI;
	}
	return angle;
}

float horizontalDistance(const glm::vec3 &first, const glm::vec3 &second)
{
	return glm::length(glm::vec2(first.x - second.x, first.z - second.z));
}
}

Pokemon::Pokemon()
{
	restoreHealth();
}

Pokemon::Pokemon(int flyPokemon, int pokemonID, std::uint32_t seed)
	: flying_(flyPokemon != 0), pokemonID_(pokemonID)
{
	restoreHealth();
	randomState_ = seed != 0
	                   ? seed
	                   : (static_cast<std::uint32_t>(pokemonID + 1) * 747796405u) ^
	                         (flying_ ? 0x9E3779B9u : 0x85EBCA6Bu);
	motionPhase_ = static_cast<float>((pokemonID % 17 + 17) % 17) * 0.37f;

	for (int attempt = 0; attempt < 24; ++attempt)
	{
		position_.x = random(-SPAWN_LIMIT, SPAWN_LIMIT);
		position_.z = random(-SPAWN_LIMIT, SPAWN_LIMIT);
		if (glm::length(glm::vec2(position_.x, position_.z)) >= SAFE_SPAWN_RADIUS)
		{
			break;
		}
	}
	position_.y = flying_ ? random(16.0f, 26.0f) : 0.0f;
	heading_ = random(-PI, PI);
	targetHeading_ = heading_;

	if (!flying_ && pokemonID % 4 == 0)
	{
		chooseIdle();
	}
	else
	{
		chooseWanderDestination();
	}
}

void Pokemon::update(double deltaSeconds)
{
	update(deltaSeconds, glm::vec3(1000.0f, 0.0f, 1000.0f));
}

void Pokemon::update(double deltaSeconds, const glm::vec3 &playerPosition)
{
	if (caught_ || isFainted())
	{
		velocity_ = glm::vec3(0.0f);
		return;
	}

	const float step = clampValue(static_cast<float>(deltaSeconds), 0.0f, MAX_DELTA_SECONDS);
	if (step <= 0.0f)
	{
		return;
	}
	age_ += step;
	updateBehavior(step, playerPosition);
	integrateMotion(step, desiredVelocity(playerPosition));
	motionPhase_ = advancePokemonAnimationPhase(
		motionPhase_, step, flying_, behaviorState_ == PokemonBehaviorState::Flee,
		getSpeedRatio());
}

void Pokemon::setCaught(int flag)
{
	caught_ = flag != 0;
	if (caught_)
	{
		velocity_ = glm::vec3(0.0f);
	}
}

void Pokemon::startle()
{
	if (caught_ || isFainted())
	{
		return;
	}
	enterFlee();
	stateTimer_ = 1.5f;
}

int Pokemon::applyDamage(int amount)
{
	if (amount <= 0 || caught_ || isFainted())
	{
		return 0;
	}
	const int previousHealth = health_;
	health_ = std::max(0, health_ - amount);
	if (isFainted())
	{
		velocity_ = glm::vec3(0.0f);
	}
	return previousHealth - health_;
}

bool Pokemon::setHealth(int health)
{
	if (health < 0 || health > maximumHealth_)
	{
		return false;
	}
	health_ = health;
	if (isFainted())
	{
		velocity_ = glm::vec3(0.0f);
	}
	return true;
}

void Pokemon::restoreHealth()
{
	maximumHealth_ = battleStatsFor(getSpecies()).maximumHealth;
	health_ = maximumHealth_;
}

void Pokemon::setDestination(float x, float y, float z)
{
	destination_.x = clampValue(x, -FIELD_LIMIT, FIELD_LIMIT);
	destination_.y = flying_ ? clampValue(y, MIN_FLIGHT_HEIGHT, MAX_FLIGHT_HEIGHT) : 0.0f;
	destination_.z = clampValue(z, -FIELD_LIMIT, FIELD_LIMIT);
	behaviorState_ = PokemonBehaviorState::Wander;
}

void Pokemon::setPosition(const glm::vec3 &position)
{
	position_.x = clampValue(position.x, -FIELD_LIMIT, FIELD_LIMIT);
	position_.y = flying_ ? clampValue(position.y, MIN_FLIGHT_HEIGHT, MAX_FLIGHT_HEIGHT) : 0.0f;
	position_.z = clampValue(position.z, -FIELD_LIMIT, FIELD_LIMIT);
	destination_ = position_;
	velocity_ = glm::vec3(0.0f);
	behaviorState_ = PokemonBehaviorState::Idle;
	stateTimer_ = 0.0f;
}

int Pokemon::getCaught() const
{
	return caught_ ? 1 : 0;
}

int Pokemon::getHealth() const
{
	return health_;
}

int Pokemon::getMaximumHealth() const
{
	return maximumHealth_;
}

float Pokemon::getHealthRatio() const
{
	return maximumHealth_ > 0
	           ? static_cast<float>(health_) / static_cast<float>(maximumHealth_)
	           : 0.0f;
}

bool Pokemon::isFainted() const
{
	return health_ <= 0;
}

glm::vec3 Pokemon::getPos() const
{
	return position_;
}

glm::vec3 Pokemon::getVelocity() const
{
	return velocity_;
}

float Pokemon::getHeading() const
{
	return heading_;
}

float Pokemon::getMotionPhase() const
{
	return motionPhase_;
}

float Pokemon::getSpeedRatio() const
{
	const float speedLimit = flying_
	                           ? (behaviorState_ == PokemonBehaviorState::Flee
	                                  ? FLYING_FLEE_SPEED
	                                  : FLYING_WANDER_SPEED)
	                           : (behaviorState_ == PokemonBehaviorState::Flee
	                                  ? GROUND_FLEE_SPEED
	                                  : GROUND_WANDER_SPEED);
	return speedLimit > 0.0f ? clampValue(glm::length(velocity_) / speedLimit, 0.0f, 1.0f) : 0.0f;
}

PokemonBehaviorState Pokemon::getBehaviorState() const
{
	return behaviorState_;
}

bool Pokemon::isFlying() const
{
	return flying_;
}

int Pokemon::getID() const
{
	return pokemonID_;
}

PokemonSpecies Pokemon::getSpecies() const
{
	return flying_
	           ? PokemonSpecies::Charizard
	           : groundPokemonSpeciesForIndex(pokemonID_);
}

float Pokemon::random(float minimum, float maximum)
{
	randomState_ = randomState_ * 1664525u + 1013904223u;
	const float unit = static_cast<float>(randomState_ >> 8) / 16777215.0f;
	return minimum + (maximum - minimum) * unit;
}

void Pokemon::chooseIdle()
{
	behaviorState_ = PokemonBehaviorState::Idle;
	stateTimer_ = random(0.8f, 2.2f);
	destination_ = position_;
}

void Pokemon::chooseWanderDestination()
{
	behaviorState_ = PokemonBehaviorState::Wander;
	stateTimer_ = flying_ ? random(3.0f, 5.5f) : random(2.5f, 5.0f);
	const float angle = random(-PI, PI);
	const float radius = flying_ ? random(7.0f, 15.0f) : random(3.5f, 8.0f);
	setDestination(position_.x + std::sin(angle) * radius,
	               flying_ ? random(MIN_FLIGHT_HEIGHT, MAX_FLIGHT_HEIGHT) : 0.0f,
	               position_.z + std::cos(angle) * radius);
}

void Pokemon::enterFlee()
{
	behaviorState_ = PokemonBehaviorState::Flee;
	stateTimer_ = 1.0f;
}

void Pokemon::updateBehavior(float deltaSeconds, const glm::vec3 &playerPosition)
{
	const float playerDistance = horizontalDistance(position_, playerPosition);
	const float enterDistance = flying_ ? FLEE_ENTER_DISTANCE + 1.5f : FLEE_ENTER_DISTANCE;
	if (playerDistance < enterDistance)
	{
		if (behaviorState_ != PokemonBehaviorState::Flee)
		{
			enterFlee();
		}
		stateTimer_ = 1.0f;
		return;
	}

	stateTimer_ -= deltaSeconds;
	if (behaviorState_ == PokemonBehaviorState::Flee)
	{
		if (playerDistance > FLEE_EXIT_DISTANCE && stateTimer_ <= 0.0f)
		{
			chooseWanderDestination();
		}
		return;
	}

	if ((behaviorState_ == PokemonBehaviorState::Wander && isAtDestination()) ||
	    stateTimer_ <= 0.0f)
	{
		if (!flying_ && random(0.0f, 1.0f) < 0.28f)
		{
			chooseIdle();
		}
		else
		{
			chooseWanderDestination();
		}
	}
}

glm::vec3 Pokemon::desiredVelocity(const glm::vec3 &playerPosition) const
{
	if (behaviorState_ == PokemonBehaviorState::Idle)
	{
		return glm::vec3(0.0f);
	}

	glm::vec3 direction;
	float speed;
	if (behaviorState_ == PokemonBehaviorState::Flee)
	{
		direction = position_ - playerPosition;
		direction.y = 0.0f;
		if (glm::length(direction) <= 0.001f)
		{
			direction = glm::vec3(std::sin(heading_), 0.0f, std::cos(heading_));
		}
		direction = glm::normalize(direction);
		const glm::vec3 tangent(-direction.z, 0.0f, direction.x);
		direction = glm::normalize(direction + tangent * std::sin(age_ * 2.1f + motionPhase_) * 0.22f);
		speed = flying_ ? FLYING_FLEE_SPEED : GROUND_FLEE_SPEED;
	}
	else
	{
		direction = destination_ - position_;
		if (!flying_)
		{
			direction.y = 0.0f;
		}
		if (glm::length(direction) <= 0.001f)
		{
			return glm::vec3(0.0f);
		}
		direction = glm::normalize(direction);
		speed = flying_ ? FLYING_WANDER_SPEED : GROUND_WANDER_SPEED;
	}

	if (flying_ && behaviorState_ == PokemonBehaviorState::Flee)
	{
		const float preferredHeight = 20.0f + std::sin(age_ * 0.7f + motionPhase_) * 3.0f;
		direction.y = clampValue((preferredHeight - position_.y) * 0.12f, -0.45f, 0.45f);
		direction = glm::normalize(direction);
	}
	return direction * speed;
}

void Pokemon::integrateMotion(float deltaSeconds, const glm::vec3 &desired)
{
	const float acceleration = flying_ ? FLYING_ACCELERATION : GROUND_ACCELERATION;
	velocity_ = moveToward(velocity_, desired, acceleration * deltaSeconds);
	if (!flying_)
	{
		velocity_.y = 0.0f;
	}

	position_ += velocity_ * deltaSeconds;
	bool reachedBoundary = false;
	if (position_.x < -FIELD_LIMIT || position_.x > FIELD_LIMIT)
	{
		position_.x = clampValue(position_.x, -FIELD_LIMIT, FIELD_LIMIT);
		velocity_.x = 0.0f;
		reachedBoundary = true;
	}
	if (position_.z < -FIELD_LIMIT || position_.z > FIELD_LIMIT)
	{
		position_.z = clampValue(position_.z, -FIELD_LIMIT, FIELD_LIMIT);
		velocity_.z = 0.0f;
		reachedBoundary = true;
	}
	if (flying_)
	{
		if (position_.y < MIN_FLIGHT_HEIGHT || position_.y > MAX_FLIGHT_HEIGHT)
		{
			position_.y = clampValue(position_.y, MIN_FLIGHT_HEIGHT, MAX_FLIGHT_HEIGHT);
			velocity_.y = 0.0f;
			reachedBoundary = true;
		}
	}
	else
	{
		position_.y = 0.0f;
	}
	if (reachedBoundary && behaviorState_ != PokemonBehaviorState::Flee)
	{
		stateTimer_ = 0.0f;
	}

	const glm::vec2 planarVelocity(velocity_.x, velocity_.z);
	if (glm::length(planarVelocity) > 0.05f)
	{
		targetHeading_ = std::atan2(velocity_.x, velocity_.z);
		const float turnRate = behaviorState_ == PokemonBehaviorState::Flee ? 4.5f : 2.6f;
		const float turn = shortestAngle(targetHeading_ - heading_);
		heading_ += clampValue(turn, -turnRate * deltaSeconds, turnRate * deltaSeconds);
		heading_ = shortestAngle(heading_);
	}
}

bool Pokemon::isAtDestination() const
{
	return glm::distance(position_, destination_) <= ARRIVAL_DISTANCE;
}
