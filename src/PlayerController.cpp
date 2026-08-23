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

	PlayerInput input;
	input.forward = clampAxis(rawInput.forward);
	input.turn = clampAxis(rawInput.turn);
	input.vertical = clampAxis(rawInput.vertical);

	yaw_ = wrapAngle(yaw_ + input.turn * config_.turnSpeed * deltaSeconds);
	glm::vec3 forward(-std::sin(yaw_), 0.0f, -std::cos(yaw_));
	float targetSpeed = input.forward >= 0.0f
	                        ? input.forward * config_.maxForwardSpeed
	                        : input.forward * config_.maxReverseSpeed;
	glm::vec3 targetHorizontalVelocity = forward * targetSpeed;
	float horizontalRate = std::fabs(input.forward) > AXIS_EPSILON
	                           ? config_.horizontalAcceleration
	                           : config_.horizontalDeceleration;
	horizontalVelocity_ = moveToward(horizontalVelocity_, targetHorizontalVelocity,
	                                 horizontalRate * deltaSeconds);

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
	gravityEnabled_ = false;
	grounded_ = position_.y <= groundHeight + CONTACT_EPSILON;
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

void PlayerController::setGravityEnabled(bool enabled)
{
	gravityEnabled_ = enabled;
}

void PlayerController::toggleGravity()
{
	gravityEnabled_ = !gravityEnabled_;
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

float PlayerController::groundHeightAt(float worldX, float worldZ) const
{
	if (!groundHeightProvider_)
	{
		return config_.groundY;
	}

	const float height = groundHeightProvider_(worldX, worldZ);
	return std::isfinite(height) ? height : config_.groundY;
}
