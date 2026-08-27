#pragma once

#include <functional>
#include <vector>

#include <glm/glm.hpp>

#include "BattleMechanics.h"

enum class BattleMoveShape
{
	NarrowProjectile,
	WideProjectile,
	LineProjectile,
	MeleeLunge,
	Cone,
};

enum class BattleMoveImpactKind
{
	Miss,
	Target,
	Terrain,
	Obstacle,
};

struct BattleMoveGeometry
{
	BattleMoveShape shape = BattleMoveShape::NarrowProjectile;
	float range = 1.0f;
	float projectileRadius = 0.1f;
	float coneHalfAngleRadians = 0.0f;
	float projectileSpeed = 1.0f;
	float dangerRadius = 0.5f;
};

struct BattleMoveBlockerCylinder
{
	glm::vec2 center = glm::vec2(0.0f);
	float radius = 1.0f;
	float baseY = 0.0f;
	float height = 1.0f;
};

struct BattleMoveVolumeInput
{
	glm::vec3 origin = glm::vec3(0.0f);
	glm::vec3 aimDirection = glm::vec3(0.0f, 0.0f, -1.0f);
	glm::vec3 targetCenter = glm::vec3(0.0f);
	float targetRadius = 0.5f;
	std::vector<BattleMoveBlockerCylinder> blockers;
	std::function<float(float, float)> groundHeightProvider;
};

struct BattleMoveVolumeResult
{
	BattleMoveImpactKind impact = BattleMoveImpactKind::Miss;
	glm::vec3 impactPosition = glm::vec3(0.0f);
	float travelDistance = 0.0f;
	bool hitTarget = false;
};

BattleMoveGeometry battleMoveGeometryFor(BattleMoveId moveId);
float playerBattleMoveReleaseHeight(BattleMoveId moveId);
const char *battleMoveShapeName(BattleMoveShape shape);
BattleMoveVolumeResult resolveBattleMoveVolume(
	BattleMoveId moveId, const BattleMoveVolumeInput &input);
