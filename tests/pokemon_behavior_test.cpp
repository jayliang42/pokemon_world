#include "Pokemon.h"
#include "PokemonAnimation.h"
#include "BattleMechanics.h"

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

float horizontalDistance(const glm::vec3 &first, const glm::vec3 &second)
{
	return glm::length(glm::vec2(first.x - second.x, first.z - second.z));
}

void testDeterministicSpawnAvoidsPlayerAndFieldEdge()
{
	Pokemon first(0, 7, 12345u);
	Pokemon second(0, 7, 12345u);
	expectNear(glm::distance(first.getPos(), second.getPos()), 0.0f, 0.0001f,
	           "same seed produces the same spawn and behavior setup");
	expectTrue(horizontalDistance(first.getPos(), glm::vec3(0.0f)) >= 8.0f,
	           "ground Pokemon do not spawn directly on the player");
	expectTrue(std::fabs(first.getPos().x) <= 44.0f && std::fabs(first.getPos().z) <= 44.0f,
	           "spawn leaves room before the field boundary");
}

void testPokemonSpeciesAssignmentIsStable()
{
	Pokemon umbreonSpecies(0, 0, 12u);
	Pokemon bulbasaurSpecies(0, 1, 12u);
	Pokemon charizardSpecies(1, 0, 12u);
	expectTrue(umbreonSpecies.getSpecies() == PokemonSpecies::Umbreon,
	           "even ground slots are assigned to Umbreon");
	expectTrue(bulbasaurSpecies.getSpecies() == PokemonSpecies::Bulbasaur,
	           "odd ground slots are assigned to Bulbasaur");
	expectTrue(charizardSpecies.getSpecies() == PokemonSpecies::Charizard,
	           "flying slots are assigned to Charizard");
	expectTrue(std::string(pokemonSpeciesName(bulbasaurSpecies.getSpecies())) ==
	               "Bulbasaur",
	           "species names are suitable for target feedback");
}

void testNearbyPlayerTriggersSmoothFleeMotion()
{
	Pokemon pokemon(0, 2, 99u);
	pokemon.setPosition(glm::vec3(2.0f, 0.0f, 0.0f));
	const glm::vec3 playerPosition(0.0f);
	const float startingDistance = horizontalDistance(pokemon.getPos(), playerPosition);

	pokemon.update(0.05, playerPosition);
	expectTrue(pokemon.getBehaviorState() == PokemonBehaviorState::Flee,
	           "nearby player switches a ground Pokemon into Flee state");
	const float firstFrameSpeed = glm::length(pokemon.getVelocity());
	expectTrue(firstFrameSpeed > 0.0f && firstFrameSpeed < 4.2f,
	           "flee movement accelerates instead of jumping to full speed");

	for (int i = 0; i < 40; ++i)
	{
		pokemon.update(0.05, playerPosition);
	}
	expectTrue(horizontalDistance(pokemon.getPos(), playerPosition) > startingDistance + 2.0f,
	           "flee steering increases distance from the player");
	expectTrue(glm::length(pokemon.getVelocity()) <= 4.2001f,
	           "flee speed remains under its configured cap");
}

void testStartleForcesAFieldPokemonToFlee()
{
	Pokemon pokemon(1, 2, 99u);
	pokemon.setPosition(glm::vec3(0.0f, 20.0f, -20.0f));
	pokemon.startle();
	expectTrue(pokemon.getBehaviorState() == PokemonBehaviorState::Flee,
	           "capture failure can startle a distant Pokemon into fleeing");
	pokemon.update(0.05, glm::vec3(0.0f));
	expectTrue(glm::length(pokemon.getVelocity()) > 0.0f,
	           "a startled Pokemon accelerates away from the player");

	pokemon.setCaught(1);
	pokemon.startle();
	expectTrue(pokemon.getCaught() == 1,
	           "startle does not revive an already captured Pokemon");
}

void testHealthDamageFaintingAndRestore()
{
	Pokemon pokemon(0, 1, 99u);
	expectTrue(pokemon.getHealth() == pokemon.getMaximumHealth() &&
	               pokemon.getMaximumHealth() ==
	                   battleStatsFor(PokemonSpecies::Bulbasaur).maximumHealth,
	           "Pokemon begin at their species maximum health");
	const int firstDamage = pokemon.applyDamage(12);
	expectTrue(firstDamage == 12 && pokemon.getHealth() ==
	               pokemon.getMaximumHealth() - 12,
	           "damage reduces health and reports the applied amount");
	expectTrue(pokemon.setHealth(31) && pokemon.getHealth() == 31,
	           "validated restore can apply an exact saved health value");
	expectTrue(!pokemon.setHealth(-1) && !pokemon.setHealth(
	               pokemon.getMaximumHealth() + 1) && pokemon.getHealth() == 31,
	           "saved health outside the species range is rejected without mutation");
	const glm::vec3 positionBeforeFainting = pokemon.getPos();
	pokemon.applyDamage(10000);
	expectTrue(pokemon.isFainted() && pokemon.getHealth() == 0,
	           "lethal damage clamps health at zero and marks fainting");
	for (int i = 0; i < 20; ++i)
	{
		pokemon.update(0.05, glm::vec3(0.0f));
	}
	expectNear(glm::distance(pokemon.getPos(), positionBeforeFainting), 0.0f,
	           0.0001f, "fainted Pokemon stop updating movement");
	pokemon.startle();
	expectNear(glm::length(pokemon.getVelocity()), 0.0f, 0.0001f,
	           "fainted Pokemon cannot be startled back into motion");
	pokemon.restoreHealth();
	expectTrue(!pokemon.isFainted() &&
	               pokemon.getHealth() == pokemon.getMaximumHealth(),
	           "restoring health revives a Pokemon at full health");
}

void testFleeStateReturnsToWanderAfterReachingSafety()
{
	Pokemon pokemon(0, 3, 123u);
	pokemon.setPosition(glm::vec3(2.0f, 0.0f, 0.0f));
	pokemon.update(0.05, glm::vec3(0.0f));
	expectTrue(pokemon.getBehaviorState() == PokemonBehaviorState::Flee,
	           "test setup enters Flee state");

	const glm::vec3 distantPlayer(40.0f, 0.0f, 40.0f);
	for (int i = 0; i < 80; ++i)
	{
		pokemon.update(0.05, distantPlayer);
	}
	expectTrue(pokemon.getBehaviorState() != PokemonBehaviorState::Flee,
	           "Pokemon leaves Flee state after the threat is gone");
}

void testLargeFrameIsClampedAndCaughtPokemonStops()
{
	Pokemon regular(0, 5, 321u);
	Pokemon stalled(0, 5, 321u);
	const glm::vec3 distantPlayer(40.0f, 0.0f, 40.0f);
	regular.update(0.05, distantPlayer);
	stalled.update(3.0, distantPlayer);
	expectNear(glm::distance(regular.getPos(), stalled.getPos()), 0.0f, 0.0001f,
	           "long frame uses the same clamped AI step as a normal frame");

	regular.setCaught(1);
	const glm::vec3 caughtPosition = regular.getPos();
	for (int i = 0; i < 20; ++i)
	{
		regular.update(0.05, glm::vec3(0.0f));
	}
	expectNear(glm::distance(regular.getPos(), caughtPosition), 0.0f, 0.0001f,
	           "caught Pokemon stop updating immediately");
	expectNear(glm::length(regular.getVelocity()), 0.0f, 0.0001f,
	           "caught Pokemon clear residual velocity");
}

void testFlyingPokemonStaysWithinFlightBand()
{
	Pokemon flying(1, 4, 777u);
	for (int i = 0; i < 2000; ++i)
	{
		flying.update(0.05, glm::vec3(40.0f, 0.0f, 40.0f));
	}
	expectTrue(flying.getPos().y >= 12.0f && flying.getPos().y <= 30.0f,
	           "flying Pokemon remain inside the configured altitude band");
	expectTrue(std::fabs(flying.getPos().x) <= 46.0f && std::fabs(flying.getPos().z) <= 46.0f,
	           "flying Pokemon remain inside the playable field");
}

void testAnimationPhaseFollowsLocomotion()
{
	const float idleGround = advancePokemonAnimationPhase(0.0f, 0.05f, false, false, 0.0f);
	const float walkingGround = advancePokemonAnimationPhase(0.0f, 0.05f, false, false, 1.0f);
	const float fleeingGround = advancePokemonAnimationPhase(0.0f, 0.05f, false, true, 1.0f);
	expectTrue(idleGround > 0.0f, "idle animation keeps a slow breathing phase");
	expectTrue(walkingGround > idleGround, "walking advances animation faster than idle");
	expectTrue(fleeingGround > walkingGround, "fleeing advances animation faster than walking");

	float wrapped = 6.27f;
	wrapped = advancePokemonAnimationPhase(wrapped, 0.05f, true, true, 1.0f);
	expectTrue(wrapped >= 0.0f && wrapped < 6.28319f,
	           "animation phase wraps without growing indefinitely");
}

void testAnimationPoseReflectsMovementState()
{
	PokemonAnimationInput idleInput;
	idleInput.phase = 1.5707963f;
	const PokemonAnimationPose idle = samplePokemonAnimation(idleInput);
	expectNear(idle.strideAngle, 0.0f, 0.0001f,
	           "idle ground Pokemon do not cycle their legs");
	expectTrue(idle.breathingScale > 1.0f, "idle ground Pokemon continue breathing");

	PokemonAnimationInput walkingInput = idleInput;
	walkingInput.speedRatio = 1.0f;
	const PokemonAnimationPose walking = samplePokemonAnimation(walkingInput);
	walkingInput.fleeing = true;
	const PokemonAnimationPose fleeing = samplePokemonAnimation(walkingInput);
	expectTrue(std::fabs(walking.strideAngle) > 0.4f,
	           "walking produces a visible leg stride");
	expectTrue(std::fabs(fleeing.strideAngle) > std::fabs(walking.strideAngle),
	           "fleeing uses a stronger stride than walking");
	expectTrue(fleeing.bodyBob > walking.bodyBob,
	           "fleeing uses a stronger body lift than walking");

	PokemonAnimationInput flyingInput;
	flyingInput.flying = true;
	flyingInput.speedRatio = 0.75f;
	flyingInput.verticalSpeedRatio = 1.0f;
	flyingInput.turnRatio = 1.0f;
	flyingInput.phase = 1.5707963f;
	const PokemonAnimationPose flying = samplePokemonAnimation(flyingInput);
	expectTrue(flying.wingAngle > 0.4f, "flying Pokemon receive a wing flap pose");
	expectTrue(flying.bodyPitch > 0.0f, "climbing pitches a flying Pokemon upward");
	expectTrue(flying.bodyRoll < 0.0f, "turning banks a flying Pokemon into the turn");
}

void testNamedPartsReceiveArticulatedMotion()
{
	PokemonAnimationPose pose;
	pose.strideAngle = 0.5f;
	pose.wingAngle = 0.4f;
	pose.tailAngle = 0.2f;

	const PokemonPartAnimation frontLeft =
		samplePokemonPartAnimation("leg-front-left", pose);
	const PokemonPartAnimation frontRight =
		samplePokemonPartAnimation("leg-front-right", pose);
	expectNear(frontLeft.pitch, -frontRight.pitch, 0.0001f,
	           "left and right legs move in opposite stride directions");

	const PokemonPartAnimation leftWing =
		samplePokemonPartAnimation("wing-left", pose);
	const PokemonPartAnimation rightWing =
		samplePokemonPartAnimation("wing-right", pose);
	expectNear(leftWing.roll, -rightWing.roll, 0.0001f,
	           "left and right wings flap around mirrored joints");

	const PokemonPartAnimation tail = samplePokemonPartAnimation("tail", pose);
	expectNear(tail.yaw, pose.tailAngle, 0.0001f,
	           "tail groups receive lateral sway");
	const PokemonPartAnimation body = samplePokemonPartAnimation("body", pose);
	expectNear(body.pitch + body.yaw + body.roll, 0.0f, 0.0001f,
	           "body groups remain attached to the root transform");
}
}

int main()
{
	testDeterministicSpawnAvoidsPlayerAndFieldEdge();
	testPokemonSpeciesAssignmentIsStable();
	testNearbyPlayerTriggersSmoothFleeMotion();
	testStartleForcesAFieldPokemonToFlee();
	testHealthDamageFaintingAndRestore();
	testFleeStateReturnsToWanderAfterReachingSafety();
	testLargeFrameIsClampedAndCaughtPokemonStops();
	testFlyingPokemonStaysWithinFlightBand();
	testAnimationPhaseFollowsLocomotion();
	testAnimationPoseReflectsMovementState();
	testNamedPartsReceiveArticulatedMotion();

	if (failures != 0)
	{
		std::cerr << failures << " Pokemon behavior test(s) failed" << std::endl;
		return 1;
	}

	std::cout << "All Pokemon behavior tests passed" << std::endl;
	return 0;
}
