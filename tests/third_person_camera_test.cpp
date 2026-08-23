#include "ThirdPersonCamera.h"

#include <cmath>
#include <iostream>
#include <string>

namespace
{
int failures = 0;

void expectTrue(bool condition, const std::string &message)
{
	if (!condition)
	{
		std::cerr << "FAIL: " << message << std::endl;
		++failures;
	}
}

void expectNear(float actual, float expected, float tolerance, const std::string &message)
{
	if (std::fabs(actual - expected) > tolerance)
	{
		std::cerr << "FAIL: " << message << " (expected " << expected
		          << ", got " << actual << ")" << std::endl;
		++failures;
	}
}

void testUsesFullBoomWhenViewIsClear()
{
	ThirdPersonCamera camera;
	camera.setGroundHeightProvider([](float, float) { return 0.0f; });
	ThirdPersonCameraPose pose = camera.update(glm::vec3(0.0f), 0.0f, 0.05f);

	expectNear(pose.target.y, 1.35f, 0.0001f, "camera targets the player's upper body");
	expectNear(pose.position.x, 0.0f, 0.0001f, "zero yaw keeps the camera centered");
	expectNear(pose.position.y, 3.75f, 0.0001f, "clear camera uses the configured boom height");
	expectNear(pose.position.z, 6.5f, 0.0001f, "clear camera stays behind the player");
	expectNear(pose.boomFraction, 1.0f, 0.0001f, "clear view uses the full camera boom");
	expectTrue(!pose.obstructed, "clear view is not reported as obstructed");
}

void testTerrainShortensCameraBeforeClipping()
{
	ThirdPersonCamera camera;
	camera.setGroundHeightProvider([](float, float worldZ) {
		return worldZ > 2.5f ? 5.0f : 0.0f;
	});
	ThirdPersonCameraPose pose = camera.update(glm::vec3(0.0f), 0.0f, 0.05f);

	expectTrue(pose.obstructed, "terrain between player and camera is detected");
	expectTrue(pose.position.z < 2.5f, "camera stops in front of the obstructing terrain");
	expectTrue(pose.position.y > 0.3f, "terrain collision keeps camera clearance above the ground");
}

void testTallObstacleShortensCameraButLowObstacleDoesNot()
{
	ThirdPersonCamera camera;
	camera.setGroundHeightProvider([](float, float) { return 0.0f; });
	StaticCollisionCylinder obstacle;
	obstacle.center = glm::vec2(0.0f, 3.0f);
	obstacle.radius = 1.0f;
	obstacle.baseY = 0.0f;
	obstacle.height = 5.0f;
	camera.setStaticObstacles({obstacle});
	ThirdPersonCameraPose blocked = camera.update(glm::vec3(0.0f), 0.0f, 0.05f);
	expectTrue(blocked.obstructed, "tall obstacle intersects the camera boom");
	expectTrue(blocked.position.z < 2.0f, "camera stops before the obstacle radius");

	camera.reset();
	obstacle.height = 0.5f;
	camera.setStaticObstacles({obstacle});
	ThirdPersonCameraPose clear = camera.update(glm::vec3(0.0f), 0.0f, 0.05f);
	expectNear(clear.boomFraction, 1.0f, 0.0001f,
	           "camera passes above an obstacle that stays below the boom");
}

void testCameraSnapsInAndReturnsGradually()
{
	ThirdPersonCamera camera;
	camera.setGroundHeightProvider([](float, float) { return 0.0f; });
	StaticCollisionCylinder obstacle;
	obstacle.center = glm::vec2(0.0f, 3.0f);
	obstacle.radius = 1.0f;
	obstacle.baseY = 0.0f;
	obstacle.height = 5.0f;
	camera.setStaticObstacles({obstacle});
	ThirdPersonCameraPose blocked = camera.update(glm::vec3(0.0f), 0.0f, 0.05f);

	camera.setStaticObstacles({});
	ThirdPersonCameraPose firstClearFrame = camera.update(glm::vec3(0.0f), 0.0f, 0.05f);
	expectTrue(firstClearFrame.boomFraction > blocked.boomFraction,
	           "camera begins extending after an obstruction clears");
	expectTrue(firstClearFrame.boomFraction < 1.0f,
	           "camera returns gradually instead of popping to full distance");
}
}

int main()
{
	testUsesFullBoomWhenViewIsClear();
	testTerrainShortensCameraBeforeClipping();
	testTallObstacleShortensCameraButLowObstacleDoesNot();
	testCameraSnapsInAndReturnsGradually();

	if (failures != 0)
	{
		std::cerr << failures << " third-person camera test(s) failed" << std::endl;
		return 1;
	}

	std::cout << "All third-person camera tests passed" << std::endl;
	return 0;
}
