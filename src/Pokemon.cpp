#include "Pokemon.h"

#include "BattleMechanics.h"
#include "PokemonAnimation.h"
#include "PokemonEcology.h"
#include "PokemonPerception.h"
#include "PokemonSpawn.h"

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
constexpr float UMBREON_PURSUE_SPEED = 2.7f;
constexpr float GROUND_ACCELERATION = 5.5f;
constexpr float FLYING_ACCELERATION = 4.0f;
constexpr float FLEE_EXIT_DISTANCE = 9.0f;
constexpr float UMBREON_DISENGAGE_DISTANCE = 13.0f;
constexpr float UMBREON_ATTACK_DISTANCE = 5.2f;
constexpr float UMBREON_ALERT_DURATION = 0.7f;
constexpr float UMBREON_ATTACK_COOLDOWN = 1.8f;
constexpr float CHARIZARD_PURSUE_SPEED = 3.2f;
constexpr float CHARIZARD_DISENGAGE_DISTANCE = 24.0f;
constexpr float CHARIZARD_ATTACK_DISTANCE = 18.0f;
constexpr float CHARIZARD_ATTACK_VERTICAL_RANGE = 8.0f;
constexpr float CHARIZARD_ALERT_DURATION = 0.9f;
constexpr float CHARIZARD_ATTACK_COOLDOWN = 3.2f;
constexpr float ARRIVAL_DISTANCE = 0.65f;
constexpr float SUSPICIOUS_ALERTNESS = 0.35f;
constexpr float DETECTED_ALERTNESS = 0.70f;
constexpr float CALM_ALERTNESS = 0.18f;
constexpr float COMPANION_ALERTNESS = 0.86f;

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

PokemonPerceptionConfig perceptionConfigFor(PokemonSpecies species)
{
	PokemonPerceptionConfig config;
	switch (species)
	{
	case PokemonSpecies::Bulbasaur:
		config.visionRange = 11.0f;
		config.visionHalfAngleRadians = 0.9599311f;
		config.verticalTolerance = 7.0f;
		config.hearingRange = 7.0f;
		config.alertDecayPerSecond = 0.22f;
		break;
	case PokemonSpecies::Eevee:
		config.visionRange = 13.0f;
		config.visionHalfAngleRadians = 1.1344640f;
		config.verticalTolerance = 8.0f;
		config.hearingRange = 8.5f;
		break;
	case PokemonSpecies::Umbreon:
		config.visionRange = 15.0f;
		config.visionHalfAngleRadians = 1.2217305f;
		config.verticalTolerance = 10.0f;
		config.hearingRange = 10.0f;
		config.visionAlertGainPerSecond = 0.92f;
		config.hearingAlertGainPerSecond = 0.52f;
		config.alertDecayPerSecond = 0.16f;
		break;
	case PokemonSpecies::Charizard:
		config.visionRange = 20.0f;
		config.visionHalfAngleRadians = 1.3962634f;
		config.verticalTolerance = 20.0f;
		config.hearingRange = 12.0f;
		config.visionAlertGainPerSecond = 1.20f;
		config.alertDecayPerSecond = 0.16f;
		break;
	}
	return config;
}

PokemonPerceptionConfig applyEcology(
	PokemonPerceptionConfig config, const PokemonEcologySample &ecology)
{
	config.visionRange *= ecology.visionRangeScale;
	config.hearingRange *= ecology.hearingRangeScale;
	config.visionAlertGainPerSecond *= ecology.alertGainScale;
	config.hearingAlertGainPerSecond *= ecology.alertGainScale;
	config.alertDecayPerSecond *= ecology.alertDecayScale;
	return config;
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

	const PokemonSpawnArea &spawnArea = pokemonSpawnArea(getSpecies());
	bool placedInRegion = false;
	for (int attempt = 0; attempt < 48; ++attempt)
	{
		const float angle = random(-PI, PI);
		const float minimumRadiusSquared =
			spawnArea.minimumRadius * spawnArea.minimumRadius;
		const float maximumRadiusSquared =
			spawnArea.maximumRadius * spawnArea.maximumRadius;
		const float radius = std::sqrt(random(minimumRadiusSquared,
		                                      maximumRadiusSquared));
		const glm::vec2 candidate = spawnArea.center +
			glm::vec2(std::sin(angle), std::cos(angle)) * radius;
		if (std::fabs(candidate.x) <= SPAWN_LIMIT &&
		    std::fabs(candidate.y) <= SPAWN_LIMIT &&
		    glm::length(candidate) >= SAFE_SPAWN_RADIUS &&
		    dominantWorldRegion(candidate) == spawnArea.region)
		{
			position_.x = candidate.x;
			position_.z = candidate.y;
			placedInRegion = true;
			break;
		}
	}
	if (!placedInRegion)
	{
		position_.x = spawnArea.center.x;
		position_.z = spawnArea.center.y + spawnArea.minimumRadius;
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

PokemonBehaviorEvents Pokemon::update(double deltaSeconds)
{
	return update(deltaSeconds, glm::vec3(1000.0f, 0.0f, 1000.0f));
}

PokemonBehaviorEvents Pokemon::update(double deltaSeconds,
	                                  const glm::vec3 &playerPosition)
{
	return update(deltaSeconds, playerPosition, {});
}

PokemonBehaviorEvents Pokemon::update(
	double deltaSeconds, const glm::vec3 &playerPosition,
	const std::vector<PokemonNavigationBlocker> &navigationBlockers,
	float playerNoise, bool lineOfSightClear, float daylight)
{
	PokemonBehaviorEvents events;
	if (caught_ || isFainted() || !ecologicallyPresent_)
	{
		velocity_ = glm::vec3(0.0f);
		return events;
	}

	const float step = clampValue(static_cast<float>(deltaSeconds), 0.0f, MAX_DELTA_SECONDS);
	if (step <= 0.0f)
	{
		return events;
	}
	age_ += step;
	const PokemonEcologySample ecology =
		samplePokemonEcology(getSpecies(), daylight);
	events = updateBehavior(
		step, playerPosition, playerNoise, lineOfSightClear, ecology);
	glm::vec3 desired =
		desiredVelocity(playerPosition, ecology.wanderSpeedScale);
	if (!flying_)
	{
		desired = steerGroundPokemonVelocity(
			position_, desired, navigationRadius(), pokemonID_, navigationBlockers);
	}
	integrateMotion(step, desired, navigationBlockers);
	motionPhase_ = advancePokemonAnimationPhase(
		motionPhase_, step, flying_, behaviorState_ == PokemonBehaviorState::Flee,
		getSpeedRatio());
	return events;
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
	alertness_ = 1.0f;
	enterFlee();
	stateTimer_ = 1.5f;
}

void Pokemon::receiveCompanionAlert()
{
	if (caught_ || isFainted() || behaviorState_ == PokemonBehaviorState::Flee ||
	    behaviorState_ == PokemonBehaviorState::Pursue)
	{
		return;
	}
	alertness_ = std::max(alertness_, COMPANION_ALERTNESS);
	destination_ = position_;
	velocity_ = glm::vec3(0.0f);
}

void Pokemon::coolDownAfterAttack()
{
	const bool territorialUmbreon =
		!flying_ && getSpecies() == PokemonSpecies::Umbreon;
	const bool aerialCharizard =
		flying_ && getSpecies() == PokemonSpecies::Charizard;
	if (caught_ || isFainted() || (!territorialUmbreon && !aerialCharizard))
	{
		return;
	}
	behaviorState_ = PokemonBehaviorState::Alert;
	alertness_ = std::max(alertness_, DETECTED_ALERTNESS);
	stateTimer_ = aerialCharizard ? CHARIZARD_ATTACK_COOLDOWN
	                              : UMBREON_ATTACK_COOLDOWN;
	destination_ = position_;
	velocity_ = glm::vec3(0.0f);
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

void Pokemon::setEcologicallyPresent(bool present)
{
	if (ecologicallyPresent_ == present)
	{
		return;
	}
	ecologicallyPresent_ = present;
	velocity_ = glm::vec3(0.0f);
	playerVisible_ = false;
	playerHeard_ = false;
	alertness_ = 0.0f;
	destination_ = position_;
	behaviorState_ = PokemonBehaviorState::Idle;
	stateTimer_ = present ? random(0.25f, 0.75f) : 0.0f;
}

void Pokemon::setDestination(float x, float y, float z)
{
	destination_.x = clampValue(x, -FIELD_LIMIT, FIELD_LIMIT);
	destination_.y = flying_ ? clampValue(y, MIN_FLIGHT_HEIGHT, MAX_FLIGHT_HEIGHT) : 0.0f;
	destination_.z = clampValue(z, -FIELD_LIMIT, FIELD_LIMIT);
	behaviorState_ = PokemonBehaviorState::Wander;
}

void Pokemon::investigateAt(float x, float y, float z)
{
	setDestination(x, y, z);
	investigationDestinationPending_ = true;
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
	alertness_ = 0.0f;
	playerVisible_ = false;
	playerHeard_ = false;
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

bool Pokemon::isEcologicallyPresent() const
{
	return ecologicallyPresent_;
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
	float speedLimit = GROUND_WANDER_SPEED;
	if (flying_)
	{
		speedLimit = behaviorState_ == PokemonBehaviorState::Flee
		                 ? FLYING_FLEE_SPEED
		                 : (behaviorState_ == PokemonBehaviorState::Pursue
		                        ? CHARIZARD_PURSUE_SPEED
		                        : FLYING_WANDER_SPEED);
	}
	else if (behaviorState_ == PokemonBehaviorState::Flee)
	{
		speedLimit = GROUND_FLEE_SPEED;
	}
	else if (behaviorState_ == PokemonBehaviorState::Pursue)
	{
		speedLimit = UMBREON_PURSUE_SPEED;
	}
	return speedLimit > 0.0f ? clampValue(glm::length(velocity_) / speedLimit, 0.0f, 1.0f) : 0.0f;
}

float Pokemon::getAlertness() const
{
	return alertness_;
}

bool Pokemon::canSeePlayer() const
{
	return playerVisible_;
}

bool Pokemon::canHearPlayer() const
{
	return playerHeard_;
}

PokemonBehaviorState Pokemon::getBehaviorState() const
{
	return behaviorState_;
}

bool Pokemon::isThreatening() const
{
	return behaviorState_ == PokemonBehaviorState::Alert ||
	       behaviorState_ == PokemonBehaviorState::Pursue;
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

PokemonBehaviorEvents Pokemon::updateBehavior(
	float deltaSeconds, const glm::vec3 &playerPosition, float playerNoise,
	bool lineOfSightClear, const PokemonEcologySample &ecology)
{
	PokemonBehaviorEvents events;
	const bool investigating = investigationDestinationPending_;
	investigationDestinationPending_ = false;
	const float playerDistance = horizontalDistance(position_, playerPosition);
	PokemonPerceptionInput perceptionInput;
	perceptionInput.observerPosition = position_;
	perceptionInput.observerHeading = heading_;
	perceptionInput.subjectPosition = playerPosition;
	perceptionInput.subjectNoise = playerNoise;
	perceptionInput.currentAlertness = alertness_;
	perceptionInput.deltaSeconds = deltaSeconds;
	perceptionInput.lineOfSightClear = lineOfSightClear;
	const PokemonPerceptionSample perception = samplePokemonPerception(
		applyEcology(perceptionConfigFor(getSpecies()), ecology), perceptionInput);
	alertness_ = perception.alertness;
	playerVisible_ = perception.visible;
	playerHeard_ = perception.heard;
	const bool perceivedPlayer = playerVisible_ || playerHeard_;
	const glm::vec2 toPlayer(playerPosition.x - position_.x,
	                         playerPosition.z - position_.z);
	if (perceivedPlayer && alertness_ >= SUSPICIOUS_ALERTNESS &&
	    glm::length(toPlayer) > 0.001f)
	{
		targetHeading_ = std::atan2(toPlayer.x, toPlayer.y);
	}
	if (behaviorState_ == PokemonBehaviorState::Flee)
	{
		if (perceivedPlayer || alertness_ >= DETECTED_ALERTNESS)
		{
			stateTimer_ = 1.0f;
		}
		else
		{
			stateTimer_ -= deltaSeconds;
			if (alertness_ <= CALM_ALERTNESS &&
			    playerDistance > FLEE_EXIT_DISTANCE && stateTimer_ <= 0.0f)
			{
				chooseWanderDestination();
			}
		}
		return events;
	}

	const bool territorialUmbreon =
	    !flying_ && getSpecies() == PokemonSpecies::Umbreon;
	const bool aerialCharizard =
	    flying_ && getSpecies() == PokemonSpecies::Charizard;
	const bool aggressiveEncounter = territorialUmbreon || aerialCharizard;
	if (aggressiveEncounter)
	{
		if (behaviorState_ == PokemonBehaviorState::Alert)
		{
			if (alertness_ <= CALM_ALERTNESS)
			{
				chooseWanderDestination();
				return events;
			}
			if (alertness_ >= DETECTED_ALERTNESS)
			{
				stateTimer_ -= deltaSeconds;
				if (stateTimer_ <= 0.0f)
				{
					behaviorState_ = PokemonBehaviorState::Pursue;
					stateTimer_ = 0.0f;
				}
			}
			return events;
		}

		if (behaviorState_ == PokemonBehaviorState::Pursue)
		{
			const float disengageDistance = aerialCharizard
			                                  ? CHARIZARD_DISENGAGE_DISTANCE
			                                  : UMBREON_DISENGAGE_DISTANCE;
			if (alertness_ <= CALM_ALERTNESS ||
			    (playerDistance > disengageDistance && !perceivedPlayer))
			{
				chooseWanderDestination();
				return events;
			}
			stateTimer_ = std::max(0.0f, stateTimer_ - deltaSeconds);
			const float attackDistance = aerialCharizard
			                                 ? CHARIZARD_ATTACK_DISTANCE
			                                 : UMBREON_ATTACK_DISTANCE;
			const bool verticalAligned =
				!aerialCharizard ||
				std::fabs(playerPosition.y - position_.y) <=
					CHARIZARD_ATTACK_VERTICAL_RANGE;
			if (playerDistance <= attackDistance && verticalAligned &&
			    stateTimer_ <= 0.0f)
			{
				events.attackReady = true;
				stateTimer_ = aerialCharizard ? CHARIZARD_ATTACK_COOLDOWN
				                               : UMBREON_ATTACK_COOLDOWN;
			}
			return events;
		}

		if (alertness_ >= SUSPICIOUS_ALERTNESS)
		{
			behaviorState_ = PokemonBehaviorState::Alert;
			stateTimer_ = aerialCharizard ? CHARIZARD_ALERT_DURATION
			                              : UMBREON_ALERT_DURATION;
			destination_ = position_;
			velocity_ = glm::vec3(0.0f);
			if (glm::length(toPlayer) > 0.001f)
			{
				targetHeading_ = std::atan2(toPlayer.x, toPlayer.y);
			}
			events.alertStarted = true;
			return events;
		}
	}

	if (!aggressiveEncounter && behaviorState_ == PokemonBehaviorState::Alert)
	{
		if (alertness_ >= DETECTED_ALERTNESS)
		{
			enterFlee();
			return events;
		}
		if (alertness_ <= CALM_ALERTNESS)
		{
			chooseWanderDestination();
		}
		return events;
	}

	if (!aggressiveEncounter && alertness_ >= SUSPICIOUS_ALERTNESS)
	{
		behaviorState_ = PokemonBehaviorState::Alert;
		stateTimer_ = 0.0f;
		destination_ = position_;
		velocity_ = glm::vec3(0.0f);
		events.alertStarted = true;
		return events;
	}

	stateTimer_ -= deltaSeconds;
	if (!investigating &&
	    ((behaviorState_ == PokemonBehaviorState::Wander && isAtDestination()) ||
	     stateTimer_ <= 0.0f))
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
	return events;
}

glm::vec3 Pokemon::desiredVelocity(const glm::vec3 &playerPosition,
	                               float wanderSpeedScale) const
{
	if (behaviorState_ == PokemonBehaviorState::Idle ||
	    behaviorState_ == PokemonBehaviorState::Alert)
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
	else if (behaviorState_ == PokemonBehaviorState::Pursue)
	{
		if (flying_ && getSpecies() == PokemonSpecies::Charizard)
		{
			glm::vec3 horizontalToPlayer = playerPosition - position_;
			horizontalToPlayer.y = 0.0f;
			const float distance = glm::length(horizontalToPlayer);
			if (distance <= 0.001f)
			{
				horizontalToPlayer =
					glm::vec3(std::sin(heading_), 0.0f, std::cos(heading_));
			}
			else
			{
				horizontalToPlayer /= distance;
			}
			if (distance > 16.0f)
			{
				direction = horizontalToPlayer;
			}
			else if (distance < 11.0f)
			{
				direction = -horizontalToPlayer;
			}
			else
			{
				const float orbitSign = pokemonID_ % 2 == 0 ? 1.0f : -1.0f;
				const glm::vec3 tangent(-horizontalToPlayer.z, 0.0f,
				                         horizontalToPlayer.x);
				direction = tangent * orbitSign +
				            horizontalToPlayer * ((distance - 14.0f) * 0.15f);
			}
			const float preferredHeight = clampValue(
				playerPosition.y + 5.0f, MIN_FLIGHT_HEIGHT, MAX_FLIGHT_HEIGHT);
			direction.y = clampValue(
				(preferredHeight - position_.y) * 0.18f, -0.55f, 0.55f);
			if (glm::length(direction) <= 0.001f)
			{
				return glm::vec3(0.0f);
			}
			direction = glm::normalize(direction);
			speed = CHARIZARD_PURSUE_SPEED;
		}
		else
		{
		direction = playerPosition - position_;
		direction.y = 0.0f;
		if (glm::length(direction) <= UMBREON_ATTACK_DISTANCE)
		{
			return glm::vec3(0.0f);
		}
		direction = glm::normalize(direction);
		speed = UMBREON_PURSUE_SPEED;
		}
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
		speed = (flying_ ? FLYING_WANDER_SPEED : GROUND_WANDER_SPEED) *
		        std::max(wanderSpeedScale, 0.0f);
	}

	if (flying_ && behaviorState_ == PokemonBehaviorState::Flee)
	{
		const float preferredHeight = 20.0f + std::sin(age_ * 0.7f + motionPhase_) * 3.0f;
		direction.y = clampValue((preferredHeight - position_.y) * 0.12f, -0.45f, 0.45f);
		direction = glm::normalize(direction);
	}
	return direction * speed;
}

void Pokemon::integrateMotion(
	float deltaSeconds, const glm::vec3 &desired,
	const std::vector<PokemonNavigationBlocker> &navigationBlockers)
{
	const float acceleration = flying_ ? FLYING_ACCELERATION : GROUND_ACCELERATION;
	velocity_ = moveToward(velocity_, desired, acceleration * deltaSeconds);
	if (!flying_)
	{
		velocity_.y = 0.0f;
	}

	position_ += velocity_ * deltaSeconds;
	if (!flying_)
	{
		const PokemonNavigationResult navigation =
			resolveGroundPokemonPosition(
				glm::vec2(position_.x, position_.z),
				navigationRadius(), pokemonID_, navigationBlockers);
		position_.x = navigation.position.x;
		position_.z = navigation.position.y;
		if (navigation.collided)
		{
			glm::vec2 planarVelocity(velocity_.x, velocity_.z);
			const float inwardSpeed =
				glm::dot(planarVelocity, navigation.collisionNormal);
			if (inwardSpeed < 0.0f)
			{
				planarVelocity -= navigation.collisionNormal * inwardSpeed;
				velocity_.x = planarVelocity.x;
				velocity_.z = planarVelocity.y;
			}
		}
	}
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
	if (reachedBoundary &&
	    (behaviorState_ == PokemonBehaviorState::Idle ||
	     behaviorState_ == PokemonBehaviorState::Wander))
	{
		stateTimer_ = 0.0f;
	}

	const glm::vec2 planarVelocity(velocity_.x, velocity_.z);
	const bool moving = glm::length(planarVelocity) > 0.05f;
	if (moving)
	{
		targetHeading_ = std::atan2(velocity_.x, velocity_.z);
	}
	if (moving || behaviorState_ == PokemonBehaviorState::Alert)
	{
		const float turnRate = behaviorState_ == PokemonBehaviorState::Flee
		                           ? 4.5f
		                           : (behaviorState_ == PokemonBehaviorState::Alert
		                                  ? 4.0f
		                                  : 2.6f);
		const float turn = shortestAngle(targetHeading_ - heading_);
		heading_ += clampValue(turn, -turnRate * deltaSeconds, turnRate * deltaSeconds);
		heading_ = shortestAngle(heading_);
	}
}

bool Pokemon::isAtDestination() const
{
	return glm::distance(position_, destination_) <= ARRIVAL_DISTANCE;
}

float Pokemon::navigationRadius() const
{
	if (getSpecies() == PokemonSpecies::Umbreon)
	{
		return 0.60f;
	}
	return getSpecies() == PokemonSpecies::Eevee ? 0.56f : 0.54f;
}
