#include "Pokemon.h"
#include "PokemonAnimation.h"
#include "BattleMechanics.h"
#include "PokemonSpawn.h"

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

glm::vec3 positionRelativeToHeading(const Pokemon &pokemon, float distance)
{
	return pokemon.getPos() +
	       glm::vec3(std::sin(pokemon.getHeading()), 0.0f,
	                 std::cos(pokemon.getHeading())) * distance;
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
	Pokemon eeveeSpecies(0, 3, 12u);
	Pokemon charizardSpecies(1, 0, 12u);
	expectTrue(umbreonSpecies.getSpecies() == PokemonSpecies::Umbreon,
	           "even ground slots are assigned to Umbreon");
	expectTrue(bulbasaurSpecies.getSpecies() == PokemonSpecies::Bulbasaur,
	           "odd ground slots are assigned to Bulbasaur");
	expectTrue(eeveeSpecies.getSpecies() == PokemonSpecies::Eevee,
	           "reserved field slots are assigned to Eevee");
	expectTrue(charizardSpecies.getSpecies() == PokemonSpecies::Charizard,
	           "flying slots are assigned to Charizard");
	expectTrue(std::string(pokemonSpeciesName(bulbasaurSpecies.getSpecies())) ==
	               "Bulbasaur",
	           "species names are suitable for target feedback");
	expectTrue(std::string(pokemonSpeciesName(eeveeSpecies.getSpecies())) ==
	               "Eevee",
	           "Eevee has a stable target-feedback name");
}

void testEcologicalDormancyRemovesInteractionStateAndCanRecover()
{
	Pokemon pokemon(0, 0, 912u);
	pokemon.setPosition(glm::vec3(0.0f));
	const glm::vec3 nearbyPlayer = positionRelativeToHeading(pokemon, 2.0f);
	pokemon.update(0.05, nearbyPlayer, {}, 0.0f, true, 0.0f);
	expectTrue(pokemon.getAlertness() > 0.0f,
	           "a present Pokemon can build encounter alertness");
	pokemon.setEcologicallyPresent(false);
	expectTrue(!pokemon.isEcologicallyPresent() &&
	               pokemon.getBehaviorState() == PokemonBehaviorState::Idle &&
	               pokemon.getAlertness() == 0.0f &&
	               glm::length(pokemon.getVelocity()) == 0.0f,
	           "ecological dormancy clears interaction and movement state");
	const PokemonBehaviorEvents dormantEvents =
		pokemon.update(0.05, nearbyPlayer, {}, 1.0f, true, 0.0f);
	expectTrue(!dormantEvents.alertStarted && !dormantEvents.attackReady,
	           "a dormant Pokemon cannot start an encounter");
	pokemon.setEcologicallyPresent(true);
	expectTrue(pokemon.isEcologicallyPresent(),
	           "the same saved Pokemon slot can return in another time band");
}

void testSpeciesSpawnInTheirDesignedRegions()
{
	for (std::uint32_t seed = 1; seed <= 4; ++seed)
	{
		for (int index = 0; index < 48; ++index)
		{
			const Pokemon pokemon(0, index, seed * 1000u +
			                                   static_cast<std::uint32_t>(index));
			const PokemonSpawnArea &area = pokemonSpawnArea(pokemon.getSpecies());
			const glm::vec3 position = pokemon.getPos();
			expectTrue(dominantWorldRegion({position.x, position.z}) == area.region,
			           "ground Pokemon spawn in their species habitat");
			expectTrue(horizontalDistance(position, glm::vec3(0.0f)) >= 8.0f,
			           "regional spawning preserves the camp safe radius");
		}
		for (int index = 0; index < 8; ++index)
		{
			const Pokemon pokemon(1, index, seed * 2000u +
			                                   static_cast<std::uint32_t>(index));
			const glm::vec3 position = pokemon.getPos();
			expectTrue(dominantWorldRegion({position.x, position.z}) ==
			               WorldRegionKind::RedrockHighlands,
			           "wild Charizard spawn over Redrock Highlands");
		}
	}
}

void testNearbyPlayerTriggersSmoothFleeMotion()
{
	Pokemon pokemon(0, 1, 99u);
	pokemon.setPosition(glm::vec3(2.0f, 0.0f, 0.0f));
	glm::vec3 playerPosition = positionRelativeToHeading(pokemon, 2.0f);
	pokemon.update(0.05, playerPosition, {}, 0.0f);
	expectTrue(pokemon.getAlertness() > 0.0f &&
	               pokemon.getBehaviorState() != PokemonBehaviorState::Flee,
	           "a first sighting raises alertness without causing instant flight");
	bool enteredFlee = false;
	for (int i = 0; i < 40; ++i)
	{
		playerPosition = positionRelativeToHeading(pokemon, 2.0f);
		pokemon.update(0.05, playerPosition, {}, 0.0f);
		if (pokemon.getBehaviorState() == PokemonBehaviorState::Flee)
		{
			enteredFlee = true;
			break;
		}
	}
	const float startingDistance = horizontalDistance(pokemon.getPos(), playerPosition);
	expectTrue(pokemon.getBehaviorState() == PokemonBehaviorState::Flee,
	           "sustained nearby vision eventually makes a timid Pokemon flee");
	expectTrue(enteredFlee, "the flee transition occurs within a bounded observation window");
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

void testTerritorialUmbreonAlertsBeforePursuit()
{
	Pokemon pokemon(0, 2, 99u);
	pokemon.setPosition(glm::vec3(8.0f, 0.0f, 0.0f));
	const glm::vec3 playerPosition = positionRelativeToHeading(pokemon, 6.0f);
	const float startingDistance = horizontalDistance(pokemon.getPos(), playerPosition);

	PokemonBehaviorEvents alertEvents;
	for (int i = 0; i < 30 &&
	                pokemon.getBehaviorState() != PokemonBehaviorState::Alert; ++i)
	{
		const PokemonBehaviorEvents events =
			pokemon.update(0.05, playerPosition, {}, 0.0f);
		alertEvents.alertStarted = alertEvents.alertStarted || events.alertStarted;
	}
	expectTrue(pokemon.getBehaviorState() == PokemonBehaviorState::Alert,
	           "sustained vision puts a territorial Umbreon on alert");
	expectTrue(alertEvents.alertStarted && !alertEvents.attackReady,
	           "entering alert emits one warning before any attack");
	expectNear(glm::length(pokemon.getVelocity()), 0.0f, 0.0001f,
	           "Umbreon pauses during its initial warning");

	for (int i = 0; i < 50 &&
	                pokemon.getBehaviorState() != PokemonBehaviorState::Pursue; ++i)
	{
		pokemon.update(0.05, playerPosition, {}, 0.0f);
	}
	expectTrue(pokemon.getBehaviorState() == PokemonBehaviorState::Pursue,
	           "Umbreon pursues after its warning window");
	expectTrue(horizontalDistance(pokemon.getPos(), playerPosition) < startingDistance,
	           "pursuit closes distance to the player");
	expectTrue(glm::length(pokemon.getVelocity()) <= 2.7001f,
	           "pursuit remains under its configured speed cap");
}

void testTerritorialUmbreonAttackUsesCooldown()
{
	Pokemon pokemon(0, 2, 199u);
	pokemon.setPosition(glm::vec3(0.0f));
	const glm::vec3 playerPosition = positionRelativeToHeading(pokemon, 4.5f);
	bool attackReady = false;
	for (int i = 0; i < 70; ++i)
	{
		attackReady = pokemon.update(0.05, playerPosition, {}, 0.0f).attackReady ||
		              attackReady;
	}
	expectTrue(attackReady,
	           "Umbreon signals an attack after warning a player in range");

	pokemon.coolDownAfterAttack();
	expectTrue(pokemon.getBehaviorState() == PokemonBehaviorState::Alert,
	           "an attacking Umbreon returns to a watchful cooldown");
	bool attackedDuringCooldown = false;
	for (int i = 0; i < 24; ++i)
	{
		attackedDuringCooldown =
		    pokemon.update(0.05, playerPosition, {}, 0.0f).attackReady ||
		    attackedDuringCooldown;
	}
	expectTrue(!attackedDuringCooldown,
	           "Umbreon cannot immediately repeat its attack during cooldown");

	bool attackedAfterCooldown = false;
	for (int i = 0; i < 30; ++i)
	{
		attackedAfterCooldown =
		    pokemon.update(0.05, playerPosition, {}, 0.0f).attackReady ||
		    attackedAfterCooldown;
	}
	expectTrue(attackedAfterCooldown,
	           "Umbreon can attack again after its cooldown expires");
}

void testAerialCharizardSignalsWingAttackAndUsesCooldown()
{
	Pokemon pokemon(1, 0, 219u);
	pokemon.setPosition(glm::vec3(0.0f, 20.0f, 0.0f));
	glm::vec3 playerPosition = positionRelativeToHeading(pokemon, 14.0f);
	playerPosition.y = 20.0f;

	bool alertStarted = false;
	bool attackReady = false;
	for (int i = 0; i < 120 && !attackReady; ++i)
	{
		const PokemonBehaviorEvents events =
			pokemon.update(0.05, playerPosition, {}, 0.0f);
		alertStarted = alertStarted || events.alertStarted;
		attackReady = attackReady || events.attackReady;
	}
	expectTrue(alertStarted,
	           "aerial Charizard warns the player before lining up an attack");
	expectTrue(attackReady,
	           "aerial Charizard signals Wing Attack inside its engagement band");
	expectTrue(pokemon.getBehaviorState() == PokemonBehaviorState::Pursue,
	           "aerial Charizard stays engaged while attacking");
	expectTrue(glm::length(pokemon.getVelocity()) <= 3.2001f,
	           "aerial pursuit stays under its configured speed cap");

	pokemon.coolDownAfterAttack();
	expectTrue(pokemon.getBehaviorState() == PokemonBehaviorState::Alert,
	           "aerial Charizard returns to a watchful cooldown after attacking");
	bool attackedDuringCooldown = false;
	for (int i = 0; i < 40; ++i)
	{
		attackedDuringCooldown =
			pokemon.update(0.05, playerPosition, {}, 0.0f).attackReady ||
			attackedDuringCooldown;
	}
	expectTrue(!attackedDuringCooldown,
	           "aerial Charizard cannot immediately repeat Wing Attack");

	bool attackedAfterCooldown = false;
	for (int i = 0; i < 60; ++i)
	{
		attackedAfterCooldown =
			pokemon.update(0.05, playerPosition, {}, 0.0f).attackReady ||
			attackedAfterCooldown;
	}
	expectTrue(attackedAfterCooldown,
	           "aerial Charizard can attack again after its cooldown expires");
}

void testAerialCharizardRequiresVerticalAttackAlignment()
{
	Pokemon pokemon(1, 0, 319u);
	pokemon.setPosition(glm::vec3(0.0f, 20.0f, 0.0f));
	glm::vec3 playerPosition = positionRelativeToHeading(pokemon, 14.0f);
	playerPosition.y = 0.0f;

	bool attackReady = false;
	for (int i = 0; i < 140; ++i)
	{
		attackReady = pokemon.update(0.05, playerPosition, {}, 0.0f).attackReady ||
		              attackReady;
	}
	expectTrue(!attackReady,
	           "aerial Charizard does not launch Wing Attack across a large altitude gap");
}

void testTerritorialUmbreonDisengagesAndCanBeStartled()
{
	Pokemon pokemon(0, 2, 299u);
	pokemon.setPosition(glm::vec3(0.0f));
	const glm::vec3 nearbyPlayer = positionRelativeToHeading(pokemon, 5.0f);
	for (int i = 0; i < 45 && !pokemon.isThreatening(); ++i)
	{
		pokemon.update(0.05, nearbyPlayer, {}, 0.0f);
	}
	for (int i = 0; i < 140 && pokemon.isThreatening(); ++i)
	{
		pokemon.update(0.05, glm::vec3(40.0f, 0.0f, 40.0f), {}, 0.0f);
	}
	expectTrue(!pokemon.isThreatening(),
	           "Umbreon disengages after alertness decays outside its territory");

	pokemon.setPosition(glm::vec3(4.0f, 0.0f, 0.0f));
	pokemon.update(0.05, glm::vec3(0.0f));
	pokemon.startle();
	pokemon.update(0.05, glm::vec3(0.0f));
	expectTrue(pokemon.getBehaviorState() == PokemonBehaviorState::Flee,
	           "capture and battle reactions override territorial aggression");
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
	pokemon.startle();
	expectTrue(pokemon.getBehaviorState() == PokemonBehaviorState::Flee,
	           "test setup enters Flee state");

	const glm::vec3 distantPlayer(40.0f, 0.0f, 40.0f);
	for (int i = 0; i < 100; ++i)
	{
		pokemon.update(0.05, distantPlayer);
	}
	expectTrue(pokemon.getBehaviorState() != PokemonBehaviorState::Flee,
	           "Pokemon leaves Flee state after the threat is gone");
}

void testViewConeHearingAndAlertDecayDriveBehavior()
{
	Pokemon pokemon(0, 1, 456u);
	pokemon.setPosition(glm::vec3(0.0f));
	for (int i = 0; i < 20; ++i)
	{
		pokemon.update(
			0.05, positionRelativeToHeading(pokemon, -4.0f), {}, 0.0f);
	}
	expectNear(pokemon.getAlertness(), 0.0f, 0.0001f,
	           "a quiet player behind a Pokemon remains outside its view cone");
	expectTrue(!pokemon.canSeePlayer() && !pokemon.canHearPlayer(),
	           "quiet rear approach produces no visual or hearing stimulus");

	for (int i = 0; i < 20; ++i)
	{
		pokemon.update(
			0.05, positionRelativeToHeading(pokemon, -3.0f), {}, 1.0f);
	}
	expectTrue(pokemon.getAlertness() > 0.20f && pokemon.canHearPlayer(),
	           "nearby rear movement raises alertness through hearing");
	const float raisedAlertness = pokemon.getAlertness();
	for (int i = 0; i < 20; ++i)
	{
		pokemon.update(0.05, glm::vec3(40.0f, 0.0f, 40.0f), {}, 0.0f);
	}
	expectTrue(pokemon.getAlertness() < raisedAlertness,
	           "alertness decays when sight and sound stimuli disappear");
}

void testWorldOcclusionBlocksSightButKeepsHearing()
{
	Pokemon pokemon(0, 1, 456u);
	pokemon.setPosition(glm::vec3(0.0f));
	for (int i = 0; i < 30; ++i)
	{
		pokemon.update(
			0.05, positionRelativeToHeading(pokemon, 4.0f), {}, 0.0f, false);
	}
	expectNear(pokemon.getAlertness(), 0.0f, 0.0001f,
	           "terrain cover prevents a quiet player from raising alertness");
	expectTrue(!pokemon.canSeePlayer(),
	           "an occluded player is not reported as visible");
	pokemon.update(
		0.05, positionRelativeToHeading(pokemon, 4.0f), {}, 1.0f, false);
	expectTrue(!pokemon.canSeePlayer() && pokemon.canHearPlayer() &&
	               pokemon.getAlertness() > 0.0f,
	           "nearby movement remains audible through visual cover");
}

void testCompanionAlertMakesTimidPokemonFlee()
{
	Pokemon bulbasaur(0, 1, 789u);
	bulbasaur.setPosition(glm::vec3(0.0f));
	bulbasaur.receiveCompanionAlert();
	const glm::vec3 distantPlayer(40.0f, 0.0f, 40.0f);
	bulbasaur.update(0.05, distantPlayer, {}, 0.0f, false);
	expectTrue(bulbasaur.getBehaviorState() == PokemonBehaviorState::Alert,
	           "a timid companion pauses in alert before fleeing");
	bulbasaur.update(0.05, distantPlayer, {}, 0.0f, false);
	expectTrue(bulbasaur.getBehaviorState() == PokemonBehaviorState::Flee,
	           "a sustained companion warning makes a timid Pokemon flee");
}

void testGroundPokemonNavigationResolvesVisibleBlockers()
{
	Pokemon pokemon(0, 1, 123u);
	pokemon.setPosition(glm::vec3(0.0f));
	const std::vector<PokemonNavigationBlocker> blockers = {
		{-100, glm::vec2(0.0f), 0.8f},
	};
	pokemon.update(0.05, glm::vec3(40.0f, 0.0f, 40.0f), blockers);
	const glm::vec3 position = pokemon.getPos();
	expectTrue(glm::length(glm::vec2(position.x, position.z)) >= 1.43f,
	           "ground Pokemon are separated from a visible navigation blocker");
}

void testInvestigationDestinationOverridesExpiredWanderTimer()
{
	Pokemon eevee(0, 3, 123u);
	eevee.setPosition(glm::vec3(0.0f));
	const glm::vec3 lurePosition(10.0f, 0.0f, 0.0f);
	const float startingDistance = horizontalDistance(eevee.getPos(), lurePosition);
	eevee.investigateAt(lurePosition.x, lurePosition.y, lurePosition.z);
	eevee.update(0.05, glm::vec3(40.0f, 0.0f, 40.0f), {}, 0.0f);
	expectTrue(horizontalDistance(eevee.getPos(), lurePosition) < startingDistance &&
	               eevee.getVelocity().x > 0.0f,
	           "a fresh lure investigation wins over an expired random wander timer");
}

void testUmbreonPerceptionUsesTheWorldDaylightProfile()
{
	Pokemon daytime(0, 0, 2468u);
	Pokemon nighttime(0, 0, 2468u);
	daytime.setPosition(glm::vec3(0.0f));
	nighttime.setPosition(glm::vec3(0.0f));
	const glm::vec3 playerPosition = positionRelativeToHeading(daytime, 13.0f);
	daytime.update(0.05, playerPosition, {}, 0.0f, true, 1.0f);
	nighttime.update(0.05, playerPosition, {}, 0.0f, true, 0.0f);
	expectTrue(!daytime.canSeePlayer() && nighttime.canSeePlayer(),
	           "Umbreon territory is observably wider at night than in daylight");
	expectTrue(nighttime.getAlertness() > daytime.getAlertness(),
	           "nighttime Umbreon begins building alertness at the wider boundary");
}

void testEeveeWanderSpeedUsesTheWorldDaylightProfile()
{
	Pokemon daytime(0, 3, 1357u);
	Pokemon nighttime(0, 3, 1357u);
	daytime.setPosition(glm::vec3(0.0f));
	nighttime.setPosition(glm::vec3(0.0f));
	for (int step = 0; step < 12; ++step)
	{
		daytime.investigateAt(20.0f, 0.0f, 0.0f);
		nighttime.investigateAt(20.0f, 0.0f, 0.0f);
		daytime.update(0.05, glm::vec3(40.0f), {}, 0.0f, false, 1.0f);
		nighttime.update(0.05, glm::vec3(40.0f), {}, 0.0f, false, 0.0f);
	}
	const float daytimeSpeed = glm::length(daytime.getVelocity());
	const float nighttimeSpeed = glm::length(nighttime.getVelocity());
	expectTrue(daytimeSpeed > nighttimeSpeed + 0.3f,
	           "Eevee visibly wanders faster by day than at night");
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
	testEcologicalDormancyRemovesInteractionStateAndCanRecover();
	testSpeciesSpawnInTheirDesignedRegions();
	testNearbyPlayerTriggersSmoothFleeMotion();
	testTerritorialUmbreonAlertsBeforePursuit();
	testTerritorialUmbreonAttackUsesCooldown();
	testAerialCharizardSignalsWingAttackAndUsesCooldown();
	testAerialCharizardRequiresVerticalAttackAlignment();
	testTerritorialUmbreonDisengagesAndCanBeStartled();
	testStartleForcesAFieldPokemonToFlee();
	testHealthDamageFaintingAndRestore();
	testFleeStateReturnsToWanderAfterReachingSafety();
	testViewConeHearingAndAlertDecayDriveBehavior();
	testWorldOcclusionBlocksSightButKeepsHearing();
	testCompanionAlertMakesTimidPokemonFlee();
	testGroundPokemonNavigationResolvesVisibleBlockers();
	testInvestigationDestinationOverridesExpiredWanderTimer();
	testUmbreonPerceptionUsesTheWorldDaylightProfile();
	testEeveeWanderSpeedUsesTheWorldDaylightProfile();
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
