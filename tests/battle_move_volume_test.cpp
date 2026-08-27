#include "BattleMoveVolume.h"

#include <cmath>
#include <iostream>
#include <limits>
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

BattleMoveVolumeInput standardInput()
{
	BattleMoveVolumeInput input;
	input.origin = glm::vec3(0.0f, 1.0f, 0.0f);
	input.aimDirection = glm::vec3(1.0f, 0.0f, 0.0f);
	input.targetCenter = glm::vec3(8.0f, 1.0f, 0.0f);
	input.targetRadius = 0.3f;
	input.groundHeightProvider = [](float, float) { return 0.0f; };
	return input;
}

void testThreePlayerMovesExposeDistinctGeometry()
{
	const BattleMoveGeometry ember =
		battleMoveGeometryFor(BattleMoveId::Ember);
	const BattleMoveGeometry airSlash =
		battleMoveGeometryFor(BattleMoveId::AirSlash);
	const BattleMoveGeometry flamethrower =
		battleMoveGeometryFor(BattleMoveId::Flamethrower);
	expectTrue(ember.shape == BattleMoveShape::NarrowProjectile &&
	               airSlash.shape == BattleMoveShape::WideProjectile &&
	               flamethrower.shape == BattleMoveShape::Cone,
	           "the player moves use narrow, wide, and cone spatial identities");
	expectTrue(airSlash.projectileRadius > ember.projectileRadius &&
	               airSlash.range > ember.range &&
	               flamethrower.range < ember.range,
	           "wide coverage, probing range, and short cone risk are distinct");
	expectTrue(playerBattleMoveReleaseHeight(BattleMoveId::AirSlash) >
	               playerBattleMoveReleaseHeight(BattleMoveId::Ember),
	           "the wide Air Slash volume releases above the mouth-height moves");
}

void testWildMovesExposeDistinctEncounterGeometry()
{
	const BattleMoveGeometry bite =
		battleMoveGeometryFor(BattleMoveId::Bite);
	const BattleMoveGeometry vineWhip =
		battleMoveGeometryFor(BattleMoveId::VineWhip);
	const BattleMoveGeometry tackle =
		battleMoveGeometryFor(BattleMoveId::Tackle);
	const BattleMoveGeometry wingAttack =
		battleMoveGeometryFor(BattleMoveId::WingAttack);
	expectTrue(bite.shape == BattleMoveShape::MeleeLunge &&
	               tackle.shape == BattleMoveShape::MeleeLunge,
	           "Bite and Tackle are close-range lunge volumes");
	expectTrue(vineWhip.shape == BattleMoveShape::LineProjectile &&
	               wingAttack.shape == BattleMoveShape::WideProjectile,
	           "Vine Whip is linear while Wing Attack is a wide aerial projectile");
	expectTrue(bite.range < vineWhip.range &&
	               vineWhip.range < wingAttack.range,
	           "wild melee, field ranged, and aerial ranged attacks have distinct reach");
	expectTrue(wingAttack.dangerRadius > vineWhip.dangerRadius &&
	               tackle.dangerRadius > bite.dangerRadius,
	           "dodge clearance follows each move's actual danger footprint");
}

void testWildMoveReachAndWidthAffectHits()
{
	BattleMoveVolumeInput distant = standardInput();
	const BattleMoveVolumeResult bite =
		resolveBattleMoveVolume(BattleMoveId::Bite, distant);
	const BattleMoveVolumeResult vineWhip =
		resolveBattleMoveVolume(BattleMoveId::VineWhip, distant);
	expectTrue(!bite.hitTarget && vineWhip.hitTarget,
	           "Vine Whip reaches a field target that is beyond Bite range");

	BattleMoveVolumeInput offset = standardInput();
	offset.origin.y = 10.0f;
	offset.targetCenter = glm::vec3(12.0f, 10.0f, 1.25f);
	const BattleMoveVolumeResult wingAttack =
		resolveBattleMoveVolume(BattleMoveId::WingAttack, offset);
	expectTrue(wingAttack.hitTarget,
	           "Wing Attack's wide aerial volume catches an offset player");
}

void testWideProjectileHitsOffsetTargetThatEmberMisses()
{
	BattleMoveVolumeInput input = standardInput();
	input.targetCenter.z = 0.72f;
	const BattleMoveVolumeResult ember =
		resolveBattleMoveVolume(BattleMoveId::Ember, input);
	const BattleMoveVolumeResult airSlash =
		resolveBattleMoveVolume(BattleMoveId::AirSlash, input);
	expectTrue(!ember.hitTarget && ember.impact == BattleMoveImpactKind::Miss,
	           "a target outside Ember's small sphere is missed");
	expectTrue(airSlash.hitTarget &&
	               airSlash.impact == BattleMoveImpactKind::Target,
	           "Air Slash's wide projectile catches the same moving target");
}

void testFlamethrowerUsesShortCone()
{
	BattleMoveVolumeInput inside = standardInput();
	inside.targetCenter = glm::vec3(8.0f, 1.0f, 2.0f);
	const BattleMoveVolumeResult coneHit =
		resolveBattleMoveVolume(BattleMoveId::Flamethrower, inside);
	BattleMoveVolumeInput distant = inside;
	distant.targetCenter = glm::vec3(12.0f, 1.0f, 0.0f);
	const BattleMoveVolumeResult rangeMiss =
		resolveBattleMoveVolume(BattleMoveId::Flamethrower, distant);
	expectTrue(coneHit.hitTarget,
	           "a target inside the Flamethrower cone is hit");
	expectTrue(!rangeMiss.hitTarget,
	           "a target beyond Flamethrower's short range is missed");
}

void testTerrainAndRockCanInterceptMoveBeforeTarget()
{
	BattleMoveVolumeInput terrainInput = standardInput();
	terrainInput.groundHeightProvider = [](float x, float) {
		return x > 3.5f && x < 4.5f ? 2.0f : 0.0f;
	};
	const BattleMoveVolumeResult terrainHit =
		resolveBattleMoveVolume(BattleMoveId::Ember, terrainInput);
	expectTrue(!terrainHit.hitTarget &&
	               terrainHit.impact == BattleMoveImpactKind::Terrain &&
	               terrainHit.impactPosition.x < terrainInput.targetCenter.x,
	           "a terrain ridge ends Ember before the target");

	BattleMoveVolumeInput rockInput = standardInput();
	BattleMoveBlockerCylinder rock;
	rock.center = glm::vec2(4.0f, 0.0f);
	rock.radius = 0.8f;
	rock.baseY = 0.0f;
	rock.height = 2.5f;
	rockInput.blockers.push_back(rock);
	const BattleMoveVolumeResult rockHit =
		resolveBattleMoveVolume(BattleMoveId::AirSlash, rockInput);
	expectTrue(!rockHit.hitTarget &&
	               rockHit.impact == BattleMoveImpactKind::Obstacle &&
	               rockHit.impactPosition.x < rockInput.targetCenter.x,
	           "a boulder intercepts the wide projectile before the target");
}

void testAirSlashReleaseHeightClearsRisingGround()
{
	BattleMoveVolumeInput input;
	input.origin = glm::vec3(
		0.0f, playerBattleMoveReleaseHeight(BattleMoveId::AirSlash), 0.0f);
	input.targetCenter = glm::vec3(6.0f, 1.18f, 0.0f);
	input.targetRadius = 0.72f;
	input.aimDirection = input.targetCenter - input.origin;
	input.groundHeightProvider = [](float x, float) {
		return std::max(0.0f, std::min(0.5f, x / 12.0f));
	};
	const BattleMoveVolumeResult result =
		resolveBattleMoveVolume(BattleMoveId::AirSlash, input);
	expectTrue(result.hitTarget &&
	               result.impact == BattleMoveImpactKind::Target,
	           "Air Slash's wing-height release clears ordinary rising terrain");
}

void testInvalidInputFailsClosed()
{
	BattleMoveVolumeInput input = standardInput();
	input.aimDirection.x = std::numeric_limits<float>::quiet_NaN();
	const BattleMoveVolumeResult result =
		resolveBattleMoveVolume(BattleMoveId::Ember, input);
	expectTrue(!result.hitTarget && result.impact == BattleMoveImpactKind::Miss,
	           "non-finite aim input fails closed");
}
}

int main()
{
	testThreePlayerMovesExposeDistinctGeometry();
	testWildMovesExposeDistinctEncounterGeometry();
	testWildMoveReachAndWidthAffectHits();
	testWideProjectileHitsOffsetTargetThatEmberMisses();
	testFlamethrowerUsesShortCone();
	testTerrainAndRockCanInterceptMoveBeforeTarget();
	testAirSlashReleaseHeightClearsRisingGround();
	testInvalidInputFailsClosed();
	if (failures != 0)
	{
		return 1;
	}
	std::cout << "Battle move volume tests passed" << std::endl;
	return 0;
}
