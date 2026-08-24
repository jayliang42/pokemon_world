#include "PlayerController.h"

#include <algorithm>
#include <cmath>
#include <utility>

namespace
{
constexpr float PI = 3.14159265358979323846f;
constexpr float AXIS_EPSILON = 0.0001f;
constexpr float CONTACT_EPSILON = 0.0001f;

float clampAxis(float value)
{
	return std::max(-1.0f, std::min(1.0f, value));
}

float moveToward(float current, float target, float maximumDelta)
{
	if (current < target)
	{
		return std::min(current + maximumDelta, target);
	}
	if (current > target)
	{
		return std::max(current - maximumDelta, target);
	}
	return target;
}

glm::vec3 moveToward(const glm::vec3 &current, const glm::vec3 &target, float maximumDelta)
{
	glm::vec3 difference = target - current;
	float distance = glm::length(difference);
	if (distance <= maximumDelta || distance <= AXIS_EPSILON)
	{
		return target;
	}
	return current + difference / distance * maximumDelta;
}

float wrapAngle(float angle)
{
	while (angle > PI)
	{
		angle -= 2.0f * PI;
	}
	while (angle < -PI)
	{
		angle += 2.0f * PI;
	}
	return angle;
}
}

float playerTurnAxis(bool turnLeft, bool turnRight)
{
	return static_cast<float>(turnRight) - static_cast<float>(turnLeft);
}

PlayerController::PlayerController(const PlayerPhysicsConfig &config)
	: config_(config)
{
	reset();
}

PlayerMotionEvents PlayerController::update(const PlayerInput &rawInput, float deltaSeconds)
{
	PlayerMotionEvents events;
	deltaSeconds = std::max(0.0f, std::min(config_.maxDeltaSeconds, deltaSeconds));
	if (deltaSeconds <= 0.0f)
	{
		return events;
	}

	const bool wasDodging = isDodging();
	dodgeTimeRemaining_ = std::max(0.0f, dodgeTimeRemaining_ - deltaSeconds);
	dodgeCooldownRemaining_ = std::max(0.0f, dodgeCooldownRemaining_ - deltaSeconds);
	dodgeInvulnerabilityRemaining_ =
		std::max(0.0f, dodgeInvulnerabilityRemaining_ - deltaSeconds);
	if (wasDodging && !isDodging())
	{
		horizontalVelocity_ = glm::vec3(0.0f);
	}

	PlayerInput input;
	input.forward = clampAxis(rawInput.forward);
	input.turn = clampAxis(rawInput.turn);
	input.vertical = clampAxis(rawInput.vertical);

	glm::vec3 forward(-std::sin(yaw_), 0.0f, -std::cos(yaw_));
	if (dodgeRequested_ && !isDodging() &&
	    dodgeCooldownRemaining_ <= CONTACT_EPSILON)
	{
		glm::vec3 dodgeDirection(horizontalVelocity_.x, 0.0f,
		                         horizontalVelocity_.z);
		if (glm::length(dodgeDirection) <= AXIS_EPSILON)
		{
			dodgeDirection = forward;
		}
		else
		{
			dodgeDirection = glm::normalize(dodgeDirection);
		}
		horizontalVelocity_ = dodgeDirection * config_.dodgeSpeed;
		dodgeTimeRemaining_ = std::max(0.0f, config_.dodgeDuration);
		dodgeCooldownRemaining_ = std::max(0.0f, config_.dodgeCooldown);
		dodgeInvulnerabilityRemaining_ =
			std::max(0.0f, config_.dodgeInvulnerability);
		events.dodgeStarted = true;
	}
	dodgeRequested_ = false;

	if (!isDodging())
	{
		yaw_ = wrapAngle(yaw_ + input.turn * config_.turnSpeed * deltaSeconds);
		forward = glm::vec3(-std::sin(yaw_), 0.0f, -std::cos(yaw_));
		float targetSpeed = input.forward >= 0.0f
		                        ? input.forward * config_.maxForwardSpeed
		                        : input.forward * config_.maxReverseSpeed;
		glm::vec3 targetHorizontalVelocity = forward * targetSpeed;
		float horizontalRate = std::fabs(input.forward) > AXIS_EPSILON
		                           ? config_.horizontalAcceleration
		                           : config_.horizontalDeceleration;
		horizontalVelocity_ = moveToward(horizontalVelocity_, targetHorizontalVelocity,
		                                 horizontalRate * deltaSeconds);
	}

	glm::vec3 nextPosition = position_ + horizontalVelocity_ * deltaSeconds;
	float centerLimit = std::max(0.0f, config_.fieldHalfExtent - config_.collisionRadius);
	if (nextPosition.x < -centerLimit || nextPosition.x > centerLimit)
	{
		bool enteringContact = position_.x > -centerLimit + CONTACT_EPSILON &&
		                       position_.x < centerLimit - CONTACT_EPSILON;
		events.hitBoundary = events.hitBoundary || enteringContact;
		nextPosition.x = std::max(-centerLimit, std::min(centerLimit, nextPosition.x));
		horizontalVelocity_.x = 0.0f;
	}
	if (nextPosition.z < -centerLimit || nextPosition.z > centerLimit)
	{
		bool enteringContact = position_.z > -centerLimit + CONTACT_EPSILON &&
		                       position_.z < centerLimit - CONTACT_EPSILON;
		events.hitBoundary = events.hitBoundary || enteringContact;
		nextPosition.z = std::max(-centerLimit, std::min(centerLimit, nextPosition.z));
		horizontalVelocity_.z = 0.0f;
	}

	std::vector<unsigned char> nextObstacleContacts(collisionObstacles_.size(), 0);
	for (std::size_t index = 0; index < collisionObstacles_.size(); ++index)
	{
		const StaticCollisionCylinder &obstacle = collisionObstacles_[index];
		if (obstacle.radius <= 0.0f || obstacle.height <= 0.0f ||
		    position_.y >= obstacle.baseY + obstacle.height - CONTACT_EPSILON)
		{
			continue;
		}

		const float minimumDistance = config_.collisionRadius + obstacle.radius;
		glm::vec2 separation(nextPosition.x - obstacle.center.x,
		                     nextPosition.z - obstacle.center.y);
		const float distanceSquared = glm::dot(separation, separation);
		if (distanceSquared >= minimumDistance * minimumDistance)
		{
			continue;
		}

		glm::vec2 normal(1.0f, 0.0f);
		if (distanceSquared > AXIS_EPSILON * AXIS_EPSILON)
		{
			normal = separation / std::sqrt(distanceSquared);
		}
		else
		{
			glm::vec2 previousSeparation(position_.x - obstacle.center.x,
			                             position_.z - obstacle.center.y);
			const float previousDistance = glm::length(previousSeparation);
			if (previousDistance > AXIS_EPSILON)
			{
				normal = previousSeparation / previousDistance;
			}
		}

		nextPosition.x = obstacle.center.x + normal.x * minimumDistance;
		nextPosition.z = obstacle.center.y + normal.y * minimumDistance;
		glm::vec2 planarVelocity(horizontalVelocity_.x, horizontalVelocity_.z);
		const float inwardSpeed = glm::dot(planarVelocity, normal);
		if (inwardSpeed < 0.0f)
		{
			planarVelocity -= normal * inwardSpeed;
			horizontalVelocity_.x = planarVelocity.x;
			horizontalVelocity_.z = planarVelocity.y;
		}

		nextObstacleContacts[index] = 1;
		if (index >= obstacleContacts_.size() || obstacleContacts_[index] == 0)
		{
			if (index < staticObstacles_.size())
			{
				events.hitObstacle = true;
			}
			else
			{
				events.hitDynamicObstacle = true;
			}
		}
	}
	obstacleContacts_ = std::move(nextObstacleContacts);

	if (std::fabs(input.vertical) > AXIS_EPSILON)
	{
		float targetVerticalSpeed = input.vertical > 0.0f
		                                ? input.vertical * config_.maxAscendSpeed
		                                : input.vertical * config_.maxDescendSpeed;
		verticalVelocity_ = moveToward(verticalVelocity_, targetVerticalSpeed,
		                               config_.verticalAcceleration * deltaSeconds);
	}
	else if (gravityEnabled_)
	{
		verticalVelocity_ = std::max(verticalVelocity_ - config_.gravityAcceleration * deltaSeconds,
		                             -config_.terminalFallSpeed);
	}
	else
	{
		verticalVelocity_ = moveToward(verticalVelocity_, 0.0f,
		                               config_.verticalDrag * deltaSeconds);
	}

	const float previousGroundHeight = groundHeightAt(position_.x, position_.z);
	const float nextGroundHeight = groundHeightAt(nextPosition.x, nextPosition.z);
	const float nextCeilingHeight = nextGroundHeight + config_.maxAltitude;
	bool wasAirborne = !grounded_ || position_.y > previousGroundHeight + CONTACT_EPSILON;
	nextPosition.y = position_.y + verticalVelocity_ * deltaSeconds;
	if (nextPosition.y <= nextGroundHeight)
	{
		events.landed = wasAirborne && verticalVelocity_ < -0.5f;
		nextPosition.y = nextGroundHeight;
		verticalVelocity_ = 0.0f;
		grounded_ = true;
	}
	else if (nextPosition.y >= nextCeilingHeight)
	{
		events.hitCeiling = position_.y < nextCeilingHeight - CONTACT_EPSILON;
		nextPosition.y = nextCeilingHeight;
		verticalVelocity_ = 0.0f;
		grounded_ = false;
	}
	else
	{
		grounded_ = false;
	}

	position_ = nextPosition;
	return events;
}

void PlayerController::reset()
{
	reset(glm::vec3(0.0f, groundHeightAt(0.0f, 0.0f), 0.0f));
}

void PlayerController::reset(const glm::vec3 &position, float yaw)
{
	float centerLimit = std::max(0.0f, config_.fieldHalfExtent - config_.collisionRadius);
	position_.x = std::max(-centerLimit, std::min(centerLimit, position.x));
	position_.z = std::max(-centerLimit, std::min(centerLimit, position.z));
	const float groundHeight = groundHeightAt(position_.x, position_.z);
	position_.y = std::max(groundHeight, std::min(groundHeight + config_.maxAltitude, position.y));
	horizontalVelocity_ = glm::vec3(0.0f);
	verticalVelocity_ = 0.0f;
	yaw_ = wrapAngle(yaw);
	dodgeTimeRemaining_ = 0.0f;
	dodgeCooldownRemaining_ = 0.0f;
	dodgeInvulnerabilityRemaining_ = 0.0f;
	dodgeRequested_ = false;
	gravityEnabled_ = false;
	grounded_ = position_.y <= groundHeight + CONTACT_EPSILON;
	obstacleContacts_.assign(collisionObstacles_.size(), 0);
}

void PlayerController::setGroundHeightProvider(GroundHeightProvider provider)
{
	const bool wasGrounded = grounded_;
	groundHeightProvider_ = std::move(provider);
	const float groundHeight = groundHeightAt(position_.x, position_.z);
	if (wasGrounded || position_.y < groundHeight)
	{
		position_.y = groundHeight;
		verticalVelocity_ = 0.0f;
	}
	grounded_ = position_.y <= groundHeight + CONTACT_EPSILON;
}

void PlayerController::setStaticObstacles(std::vector<StaticCollisionCylinder> obstacles)
{
	staticObstacles_ = std::move(obstacles);
	rebuildObstacleSet();
}

void PlayerController::setDynamicObstacles(std::vector<StaticCollisionCylinder> obstacles)
{
	dynamicObstacles_ = std::move(obstacles);
	rebuildObstacleSet();
}

void PlayerController::rebuildObstacleSet()
{
	collisionObstacles_ = staticObstacles_;
	collisionObstacles_.insert(collisionObstacles_.end(), dynamicObstacles_.begin(),
	                          dynamicObstacles_.end());
	if (obstacleContacts_.size() != collisionObstacles_.size())
	{
		obstacleContacts_.assign(collisionObstacles_.size(), 0);
	}
}

void PlayerController::setGravityEnabled(bool enabled)
{
	gravityEnabled_ = enabled;
}

void PlayerController::toggleGravity()
{
	gravityEnabled_ = !gravityEnabled_;
}

bool PlayerController::requestDodge()
{
	if (dodgeRequested_ || isDodging() ||
	    dodgeCooldownRemaining_ > CONTACT_EPSILON ||
	    config_.dodgeSpeed <= AXIS_EPSILON ||
	    config_.dodgeDuration <= AXIS_EPSILON)
	{
		return false;
	}
	dodgeRequested_ = true;
	return true;
}

const glm::vec3 &PlayerController::position() const
{
	return position_;
}

glm::vec3 PlayerController::velocity() const
{
	return glm::vec3(horizontalVelocity_.x, verticalVelocity_, horizontalVelocity_.z);
}

float PlayerController::yaw() const
{
	return yaw_;
}

float PlayerController::verticalVelocity() const
{
	return verticalVelocity_;
}

bool PlayerController::gravityEnabled() const
{
	return gravityEnabled_;
}

bool PlayerController::grounded() const
{
	return grounded_;
}

bool PlayerController::isDodging() const
{
	return dodgeTimeRemaining_ > CONTACT_EPSILON;
}

bool PlayerController::isInvulnerable() const
{
	return dodgeInvulnerabilityRemaining_ > CONTACT_EPSILON;
}

float PlayerController::dodgeCooldownRemaining() const
{
	return dodgeCooldownRemaining_;
}

float PlayerController::dodgeCooldownFraction() const
{
	if (config_.dodgeCooldown <= AXIS_EPSILON)
	{
		return 0.0f;
	}
	return std::max(0.0f, std::min(1.0f,
	                                 dodgeCooldownRemaining_ /
	                                     config_.dodgeCooldown));
}

float PlayerController::groundHeightAt(float worldX, float worldZ) const
{
	if (!groundHeightProvider_)
	{
		return config_.groundY;
	}

	const float height = groundHeightProvider_(worldX, worldZ);
	return std::isfinite(height) ? height : config_.groundY;
}
