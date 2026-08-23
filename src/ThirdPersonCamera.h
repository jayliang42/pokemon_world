#pragma once

#include "PlayerController.h"

#include <functional>
#include <vector>

struct ThirdPersonCameraConfig
{
	float targetHeight = 0.95f;
	float boomDistance = 6.5f;
	float boomHeight = 1.65f;
	float collisionRadius = 0.3f;
	float minimumDistance = 1.25f;
	float returnSpeed = 2.0f;
	int collisionSamples = 48;
};

struct ThirdPersonCameraPose
{
	glm::vec3 position = glm::vec3(0.0f);
	glm::vec3 target = glm::vec3(0.0f);
	float boomFraction = 1.0f;
	bool obstructed = false;
};

class ThirdPersonCamera
{
public:
	using GroundHeightProvider = std::function<float(float, float)>;

	explicit ThirdPersonCamera(const ThirdPersonCameraConfig &config = ThirdPersonCameraConfig());

	ThirdPersonCameraPose update(const glm::vec3 &playerPosition, float playerYaw,
	                             float deltaSeconds);
	void reset();
	void setGroundHeightProvider(GroundHeightProvider provider);
	void setStaticObstacles(std::vector<StaticCollisionCylinder> obstacles);

private:
	float maximumClearBoomFraction(const glm::vec3 &target,
	                               const glm::vec3 &desiredPosition) const;
	bool collidesAt(const glm::vec3 &position) const;

	ThirdPersonCameraConfig config_;
	GroundHeightProvider groundHeightProvider_;
	std::vector<StaticCollisionCylinder> staticObstacles_;
	float currentBoomFraction_ = 1.0f;
};
