#pragma once

#include <functional>

#include <glm/glm.hpp>

struct PlayerInput
{
	float forward = 0.0f;
	float turn = 0.0f;
	float vertical = 0.0f;
};

struct PlayerMotionEvents
{
	bool landed = false;
	bool hitBoundary = false;
	bool hitCeiling = false;
};

struct PlayerPhysicsConfig
{
	float groundY = 0.0f;
	float maxAltitude = 32.0f;
	float fieldHalfExtent = 48.0f;
	float collisionRadius = 0.8f;
	float maxForwardSpeed = 7.0f;
	float maxReverseSpeed = 4.5f;
	float horizontalAcceleration = 14.0f;
	float horizontalDeceleration = 18.0f;
	float turnSpeed = 2.4f;
	float maxAscendSpeed = 9.0f;
	float maxDescendSpeed = 10.0f;
	float verticalAcceleration = 28.0f;
	float verticalDrag = 20.0f;
	float gravityAcceleration = 18.0f;
	float terminalFallSpeed = 20.0f;
	float maxDeltaSeconds = 0.05f;
};

class PlayerController
{
public:
	using GroundHeightProvider = std::function<float(float, float)>;

	explicit PlayerController(const PlayerPhysicsConfig &config = PlayerPhysicsConfig());

	PlayerMotionEvents update(const PlayerInput &input, float deltaSeconds);
	void reset();
	void reset(const glm::vec3 &position, float yaw = 0.0f);
	void setGroundHeightProvider(GroundHeightProvider provider);
	void setGravityEnabled(bool enabled);
	void toggleGravity();

	const glm::vec3 &position() const;
	glm::vec3 velocity() const;
	float yaw() const;
	float verticalVelocity() const;
	bool gravityEnabled() const;
	bool grounded() const;

private:
	float groundHeightAt(float worldX, float worldZ) const;

	PlayerPhysicsConfig config_;
	GroundHeightProvider groundHeightProvider_;
	glm::vec3 position_ = glm::vec3(0.0f);
	glm::vec3 horizontalVelocity_ = glm::vec3(0.0f);
	float verticalVelocity_ = 0.0f;
	float yaw_ = 0.0f;
	bool gravityEnabled_ = false;
	bool grounded_ = true;
};
