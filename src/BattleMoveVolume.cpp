#include "BattleMoveVolume.h"

#include "CaptureProjectile.h"

#include <algorithm>
#include <cmath>

namespace
{
constexpr float EPSILON = 0.000001f;

bool finiteVector(const glm::vec3 &value)
{
	return std::isfinite(value.x) && std::isfinite(value.y) &&
	       std::isfinite(value.z);
}

BattleMoveVolumeResult resultAt(
	BattleMoveImpactKind impact, const glm::vec3 &origin,
	const glm::vec3 &position)
{
	BattleMoveVolumeResult result;
	result.impact = impact;
	result.impactPosition = position;
	result.travelDistance = glm::distance(origin, position);
	result.hitTarget = impact == BattleMoveImpactKind::Target;
	return result;
}

BattleMoveVolumeResult traceEnvironment(
	const glm::vec3 &start, const glm::vec3 &end, float radius,
	const BattleMoveVolumeInput &input)
{
	CaptureSweepHit earliest = sweepCaptureSphereAgainstTerrain(
		start, end, radius, input.groundHeightProvider);
	BattleMoveImpactKind impact = earliest.hit
		                              ? BattleMoveImpactKind::Terrain
		                              : BattleMoveImpactKind::Miss;
	for (const BattleMoveBlockerCylinder &blocker : input.blockers)
	{
		CaptureCollisionCylinder cylinder;
		cylinder.center = blocker.center;
		cylinder.radius = blocker.radius;
		cylinder.baseY = blocker.baseY;
		cylinder.height = blocker.height;
		const CaptureSweepHit hit = sweepCaptureSphereAgainstCylinder(
			start, end, radius, cylinder);
		if (hit.hit && (!earliest.hit || hit.fraction < earliest.fraction))
		{
			earliest = hit;
			impact = BattleMoveImpactKind::Obstacle;
		}
	}
	return earliest.hit
	           ? resultAt(impact, start, earliest.position)
	           : resultAt(BattleMoveImpactKind::Miss, start, end);
}
}

BattleMoveGeometry battleMoveGeometryFor(BattleMoveId moveId)
{
	BattleMoveGeometry geometry;
	switch (moveId)
	{
	case BattleMoveId::Ember:
		geometry.shape = BattleMoveShape::NarrowProjectile;
		geometry.range = 14.0f;
		geometry.projectileRadius = 0.24f;
		geometry.projectileSpeed = 15.0f;
		geometry.dangerRadius = 0.82f;
		break;
	case BattleMoveId::AirSlash:
		geometry.shape = BattleMoveShape::WideProjectile;
		geometry.range = 20.0f;
		geometry.projectileRadius = 0.75f;
		geometry.projectileSpeed = 18.0f;
		geometry.dangerRadius = 1.40f;
		break;
	case BattleMoveId::Flamethrower:
		geometry.shape = BattleMoveShape::Cone;
		geometry.range = 10.5f;
		geometry.projectileRadius = 0.10f;
		geometry.coneHalfAngleRadians = 0.34f;
		geometry.projectileSpeed = 0.0f;
		geometry.dangerRadius = 2.35f;
		break;
	case BattleMoveId::Bite:
		geometry.shape = BattleMoveShape::MeleeLunge;
		geometry.range = 4.8f;
		geometry.projectileRadius = 0.42f;
		geometry.projectileSpeed = 0.0f;
		geometry.dangerRadius = 1.15f;
		break;
	case BattleMoveId::VineWhip:
		geometry.shape = BattleMoveShape::LineProjectile;
		geometry.range = 8.5f;
		geometry.projectileRadius = 0.34f;
		geometry.projectileSpeed = 15.0f;
		geometry.dangerRadius = 0.95f;
		break;
	case BattleMoveId::Tackle:
		geometry.shape = BattleMoveShape::MeleeLunge;
		geometry.range = 4.6f;
		geometry.projectileRadius = 0.46f;
		geometry.projectileSpeed = 0.0f;
		geometry.dangerRadius = 1.35f;
		break;
	case BattleMoveId::WingAttack:
		geometry.shape = BattleMoveShape::WideProjectile;
		geometry.range = 18.0f;
		geometry.projectileRadius = 1.0f;
		geometry.projectileSpeed = 18.0f;
		geometry.dangerRadius = 1.75f;
		break;
	default:
		geometry.shape = BattleMoveShape::NarrowProjectile;
		geometry.range = 12.0f;
		geometry.projectileRadius = 0.28f;
		geometry.projectileSpeed = 13.0f;
		geometry.dangerRadius = 0.9f;
		break;
	}
	return geometry;
}

float playerBattleMoveReleaseHeight(BattleMoveId moveId)
{
	return moveId == BattleMoveId::AirSlash ? 1.45f : 0.90f;
}

const char *battleMoveShapeName(BattleMoveShape shape)
{
	switch (shape)
	{
	case BattleMoveShape::NarrowProjectile: return "Narrow";
	case BattleMoveShape::WideProjectile: return "Wide";
	case BattleMoveShape::LineProjectile: return "Line";
	case BattleMoveShape::MeleeLunge: return "Melee";
	case BattleMoveShape::Cone: return "Cone";
	}
	return "Narrow";
}

BattleMoveVolumeResult resolveBattleMoveVolume(
	BattleMoveId moveId, const BattleMoveVolumeInput &input)
{
	BattleMoveVolumeResult result;
	result.impactPosition = input.origin;
	if (!finiteVector(input.origin) || !finiteVector(input.aimDirection) ||
	    !finiteVector(input.targetCenter) ||
	    !std::isfinite(input.targetRadius) || input.targetRadius <= 0.0f ||
	    !input.groundHeightProvider)
	{
		return result;
	}
	const float aimLength = glm::length(input.aimDirection);
	if (!std::isfinite(aimLength) || aimLength <= EPSILON)
	{
		return result;
	}
	const glm::vec3 direction = input.aimDirection / aimLength;
	const BattleMoveGeometry geometry = battleMoveGeometryFor(moveId);
	if (geometry.range <= 0.0f)
	{
		return result;
	}

	if (geometry.shape == BattleMoveShape::Cone)
	{
		const glm::vec3 toTarget = input.targetCenter - input.origin;
		const float along = glm::dot(toTarget, direction);
		const glm::vec3 perpendicular = toTarget - direction * along;
		const float coneRadius = along > 0.0f
			                         ? std::tan(geometry.coneHalfAngleRadians) * along
			                         : 0.0f;
		const bool targetInside =
			along > 0.0f && along <= geometry.range &&
			glm::length(perpendicular) <= coneRadius + input.targetRadius;
		const glm::vec3 end = targetInside
			                      ? input.targetCenter
			                      : input.origin + direction * geometry.range;
		const BattleMoveVolumeResult environment = traceEnvironment(
			input.origin, end, geometry.projectileRadius, input);
		if (environment.impact != BattleMoveImpactKind::Miss)
		{
			return environment;
		}
		return targetInside
		           ? resultAt(BattleMoveImpactKind::Target, input.origin,
		                      input.targetCenter)
		           : environment;
	}

	const glm::vec3 end = input.origin + direction * geometry.range;
	const CaptureSweepHit targetHit = sweepCaptureSphereAgainstSphere(
		input.origin, end, geometry.projectileRadius, input.targetCenter,
		input.targetRadius);
	const BattleMoveVolumeResult environment = traceEnvironment(
		input.origin, end, geometry.projectileRadius, input);
	if (environment.impact != BattleMoveImpactKind::Miss)
	{
		const float environmentFraction =
			environment.travelDistance / geometry.range;
		if (!targetHit.hit || environmentFraction <= targetHit.fraction)
		{
			return environment;
		}
	}
	return targetHit.hit
	           ? resultAt(BattleMoveImpactKind::Target, input.origin,
	                      targetHit.position)
	           : environment;
}
