#include "FieldCamp.h"
#include "Pokemon.h"
#include "WorldLayout.h"
#include "WorldLighting.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <limits>
#include <string>
#include <vector>

namespace
{
constexpr int GROUND_POKEMON_COUNT = 48;
constexpr int FLYING_POKEMON_COUNT = 8;
constexpr int SIMULATION_STEPS = 10 * 60 * 60;
constexpr float FIXED_STEP_SECONDS = 1.0f / 60.0f;
constexpr float FIELD_LIMIT = 46.0f;
constexpr float MIN_FLIGHT_HEIGHT = 12.0f;
constexpr float MAX_FLIGHT_HEIGHT = 30.0f;

int failures = 0;

void expectTrue(bool condition, const std::string &message)
{
	if (!condition)
	{
		std::cerr << "FAIL: " << message << std::endl;
		++failures;
	}
}

void expectNear(double actual, double expected, double tolerance,
	            const std::string &message)
{
	if (std::fabs(actual - expected) > tolerance)
	{
		std::cerr << "FAIL: " << message << " (expected " << expected
		          << ", got " << actual << ')' << std::endl;
		++failures;
	}
}

bool finiteVector(const glm::vec3 &value)
{
	return std::isfinite(value.x) && std::isfinite(value.y) &&
	       std::isfinite(value.z);
}

float navigationRadius(PokemonSpecies species)
{
	if (species == PokemonSpecies::Umbreon)
	{
		return 0.60f;
	}
	return species == PokemonSpecies::Eevee ? 0.56f : 0.54f;
}

std::vector<PokemonNavigationBlocker> staticWorldBlockers()
{
	const std::array<WorldRockPlacement, 10> &rocks = worldRockPlacements();
	const std::array<WorldLandmarkPlacement, 9> &landmarks =
		worldLandmarkPlacements();
	std::vector<PokemonNavigationBlocker> blockers;
	blockers.reserve(rocks.size() + landmarks.size() + 4);
	for (std::size_t index = 0; index < rocks.size(); ++index)
	{
		blockers.push_back({
			-1000 - static_cast<int>(index), rocks[index].center,
			std::max(rocks[index].scale.x, rocks[index].scale.z) * 1.18f});
	}
	for (std::size_t index = 0; index < landmarks.size(); ++index)
	{
		blockers.push_back({
			-3000 - static_cast<int>(index), landmarks[index].center,
			landmarks[index].collisionRadius});
	}
	const FieldCampLayout camp = defaultFieldCampLayout();
	blockers.push_back({-2000, camp.tentCenter, 2.35f});
	blockers.push_back({-2001, camp.workbenchCenter, 1.65f});
	blockers.push_back({-2002, camp.supplyCrateCenter, 1.05f});
	blockers.push_back({-2003, camp.center, camp.wildExclusionRadius});
	return blockers;
}

struct SimulationSummary
{
	bool finite = true;
	bool insideBounds = true;
	float maximumGroundSpeed = 0.0f;
	float maximumFlyingSpeed = 0.0f;
	float minimumStaticClearance = std::numeric_limits<float>::max();
	float maximumPairOverlapSeconds = 0.0f;
	float maximumStationarySeconds = 0.0f;
	float minimumStaticClearanceTime = 0.0f;
	glm::vec3 minimumStaticPosition = glm::vec3(0.0f);
	float maximumStationaryTime = 0.0f;
	int minimumStaticPokemon = -1;
	int minimumStaticBlocker = -1;
	int maximumStationaryPokemon = -1;
	PokemonBehaviorState maximumStationaryState = PokemonBehaviorState::Idle;
	bool maximumStationaryFlying = false;
	int stateTransitions = 0;
	int alertEvents = 0;
	int fleeTransitions = 0;
	int umbreonAttacks = 0;
	int charizardAttacks = 0;
	int dayUmbreonPresent = 0;
	int nightUmbreonPresent = 0;
	int dayMeadowPresent = 0;
	int nightMeadowPresent = 0;
	double finalChecksum = 0.0;
};

SimulationSummary runTenMinuteSimulation(std::uint32_t seed)
{
	std::vector<Pokemon> ground;
	std::vector<Pokemon> flying;
	ground.reserve(GROUND_POKEMON_COUNT);
	flying.reserve(FLYING_POKEMON_COUNT);
	for (int index = 0; index < GROUND_POKEMON_COUNT; ++index)
	{
		ground.emplace_back(0, index, seed + static_cast<std::uint32_t>(index * 97));
	}
	for (int index = 0; index < FLYING_POKEMON_COUNT; ++index)
	{
		flying.emplace_back(
			1, index, seed + 10000u + static_cast<std::uint32_t>(index * 131));
	}
	ground[0].setPosition(glm::vec3(-12.0f, 0.0f, 0.0f));
	ground[1].setPosition(glm::vec3(-8.0f, 0.0f, 0.0f));
	flying[0].setPosition(glm::vec3(12.0f, 20.0f, 12.0f));

	const std::vector<PokemonNavigationBlocker> fixedBlockers =
		staticWorldBlockers();
	std::vector<PokemonBehaviorState> previousGroundState;
	std::vector<PokemonBehaviorState> previousFlyingState;
	std::vector<float> stationarySeconds(
		GROUND_POKEMON_COUNT + FLYING_POKEMON_COUNT, 0.0f);
	std::vector<float> pairOverlapSeconds(
		GROUND_POKEMON_COUNT * GROUND_POKEMON_COUNT, 0.0f);
	for (const Pokemon &pokemon : ground)
	{
		previousGroundState.push_back(pokemon.getBehaviorState());
	}
	for (const Pokemon &pokemon : flying)
	{
		previousFlyingState.push_back(pokemon.getBehaviorState());
	}

	SimulationSummary summary;
	for (int step = 0; step < SIMULATION_STEPS; ++step)
	{
		const float time = static_cast<float>(step) * FIXED_STEP_SECONDS;
		const float daylight =
			sampleWorldLighting(worldLightingCyclePhase(time)).daylight;
		int presentUmbreon = 0;
		int presentMeadowSpecies = 0;
		for (Pokemon &pokemon : ground)
		{
			pokemon.setEcologicallyPresent(pokemonEcologySlotPresent(
				pokemon.getSpecies(), pokemon.getID(), daylight));
			if (!pokemon.isEcologicallyPresent())
			{
				continue;
			}
			if (pokemon.getSpecies() == PokemonSpecies::Umbreon)
			{
				++presentUmbreon;
			}
			else
			{
				++presentMeadowSpecies;
			}
		}
		for (Pokemon &pokemon : flying)
		{
			pokemon.setEcologicallyPresent(pokemonEcologySlotPresent(
				pokemon.getSpecies(), pokemon.getID(), daylight));
		}
		if (daylight >= 0.99f)
		{
			summary.dayUmbreonPresent = std::max(
				summary.dayUmbreonPresent, presentUmbreon);
			summary.dayMeadowPresent = std::max(
				summary.dayMeadowPresent, presentMeadowSpecies);
		}
		if (daylight <= 0.01f)
		{
			summary.nightUmbreonPresent = std::max(
				summary.nightUmbreonPresent, presentUmbreon);
			summary.nightMeadowPresent = std::max(
				summary.nightMeadowPresent, presentMeadowSpecies);
		}
		glm::vec3 playerPosition;
		float playerNoise = 0.35f;
		if (time < 180.0f)
		{
			playerPosition = glm::vec3(
				-12.0f + std::sin(time * 0.17f) * 4.0f, 0.0f,
				std::cos(time * 0.17f) * 4.0f);
			playerNoise = 0.90f;
		}
		else if (time < 360.0f)
		{
			const Pokemon &lead = flying.front();
			const glm::vec3 forward(
				std::sin(lead.getHeading()), 0.0f, std::cos(lead.getHeading()));
			playerPosition = lead.getPos() + forward * 14.0f;
			playerPosition.y = lead.getPos().y;
			playerNoise = 0.20f;
		}
		else
		{
			playerPosition = glm::vec3(
				std::sin(time * 0.031f) * 32.0f,
				16.0f + std::sin(time * 0.019f) * 8.0f,
				std::cos(time * 0.027f) * 32.0f);
			playerNoise = 0.55f;
		}

		std::vector<PokemonNavigationBlocker> navigationBlockers = fixedBlockers;
		navigationBlockers.reserve(
			fixedBlockers.size() + GROUND_POKEMON_COUNT);
		for (int index = 0; index < GROUND_POKEMON_COUNT; ++index)
		{
			if (!ground[index].isEcologicallyPresent())
			{
				continue;
			}
			const glm::vec3 position = ground[index].getPos();
			navigationBlockers.push_back({
				index, glm::vec2(position.x, position.z),
				navigationRadius(ground[index].getSpecies())});
		}

		for (int index = 0; index < GROUND_POKEMON_COUNT; ++index)
		{
			Pokemon &pokemon = ground[index];
			const PokemonBehaviorEvents events = pokemon.update(
				FIXED_STEP_SECONDS, playerPosition, navigationBlockers,
				playerNoise, true, daylight);
			summary.alertEvents += events.alertStarted ? 1 : 0;
			if (events.attackReady)
			{
				++summary.umbreonAttacks;
				pokemon.coolDownAfterAttack();
			}
			if (pokemon.isEcologicallyPresent() &&
			    pokemon.getBehaviorState() != previousGroundState[index])
			{
				++summary.stateTransitions;
				if (pokemon.getBehaviorState() == PokemonBehaviorState::Flee)
				{
					++summary.fleeTransitions;
				}
				previousGroundState[index] = pokemon.getBehaviorState();
			}
		}
		for (int index = 0; index < FLYING_POKEMON_COUNT; ++index)
		{
			Pokemon &pokemon = flying[index];
			const PokemonBehaviorEvents events = pokemon.update(
				FIXED_STEP_SECONDS, playerPosition, {}, playerNoise, true,
				daylight);
			summary.alertEvents += events.alertStarted ? 1 : 0;
			if (events.attackReady)
			{
				++summary.charizardAttacks;
				pokemon.coolDownAfterAttack();
			}
			if (pokemon.getBehaviorState() != previousFlyingState[index])
			{
				++summary.stateTransitions;
				if (pokemon.getBehaviorState() == PokemonBehaviorState::Flee)
				{
					++summary.fleeTransitions;
				}
				previousFlyingState[index] = pokemon.getBehaviorState();
			}
		}

		for (int index = 0; index < GROUND_POKEMON_COUNT; ++index)
		{
			const Pokemon &pokemon = ground[index];
			const glm::vec3 position = pokemon.getPos();
			const glm::vec3 velocity = pokemon.getVelocity();
			summary.finite = summary.finite && finiteVector(position) &&
			                 finiteVector(velocity) &&
			                 std::isfinite(pokemon.getAlertness());
			summary.insideBounds =
				summary.insideBounds && std::fabs(position.x) <= FIELD_LIMIT &&
				std::fabs(position.z) <= FIELD_LIMIT &&
				std::fabs(position.y) <= 0.0001f;
			const float speed = glm::length(velocity);
			summary.maximumGroundSpeed =
				std::max(summary.maximumGroundSpeed, speed);
			const PokemonBehaviorState state = pokemon.getBehaviorState();
			const bool expectedToMove = state == PokemonBehaviorState::Wander ||
			                            state == PokemonBehaviorState::Flee;
			stationarySeconds[index] = expectedToMove && speed <= 0.05f
			                               ? stationarySeconds[index] +
			                                     FIXED_STEP_SECONDS
			                               : 0.0f;
			if (stationarySeconds[index] > summary.maximumStationarySeconds)
			{
				summary.maximumStationarySeconds = stationarySeconds[index];
				summary.maximumStationaryTime = time;
				summary.maximumStationaryPokemon = index;
				summary.maximumStationaryState = pokemon.getBehaviorState();
				summary.maximumStationaryFlying = false;
			}
			if (!pokemon.isEcologicallyPresent())
			{
				stationarySeconds[index] = 0.0f;
				continue;
			}
			for (const PokemonNavigationBlocker &blocker : fixedBlockers)
			{
				const float clearance = glm::distance(
					glm::vec2(position.x, position.z), blocker.center) -
					navigationRadius(pokemon.getSpecies()) - blocker.radius;
				if (clearance < summary.minimumStaticClearance)
				{
					summary.minimumStaticClearance = clearance;
					summary.minimumStaticClearanceTime = time;
					summary.minimumStaticPokemon = index;
					summary.minimumStaticBlocker = blocker.id;
					summary.minimumStaticPosition = position;
				}
			}
		}
		for (int first = 0; first < GROUND_POKEMON_COUNT; ++first)
		{
			for (int second = first + 1; second < GROUND_POKEMON_COUNT;
			     ++second)
			{
				const std::size_t pairIndex = static_cast<std::size_t>(
					first * GROUND_POKEMON_COUNT + second);
				if (!ground[first].isEcologicallyPresent() ||
				    !ground[second].isEcologicallyPresent())
				{
					pairOverlapSeconds[pairIndex] = 0.0f;
					continue;
				}
				const float separation = glm::distance(
					glm::vec2(ground[first].getPos().x, ground[first].getPos().z),
					glm::vec2(ground[second].getPos().x,
					          ground[second].getPos().z));
				const float minimumSeparation =
					navigationRadius(ground[first].getSpecies()) +
					navigationRadius(ground[second].getSpecies());
				pairOverlapSeconds[pairIndex] = separation < minimumSeparation
				                                      ? pairOverlapSeconds[pairIndex] +
				                                            FIXED_STEP_SECONDS
				                                      : 0.0f;
				summary.maximumPairOverlapSeconds = std::max(
					summary.maximumPairOverlapSeconds,
					pairOverlapSeconds[pairIndex]);
			}
		}
		for (int index = 0; index < FLYING_POKEMON_COUNT; ++index)
		{
			const Pokemon &pokemon = flying[index];
			const glm::vec3 position = pokemon.getPos();
			const glm::vec3 velocity = pokemon.getVelocity();
			summary.finite = summary.finite && finiteVector(position) &&
			                 finiteVector(velocity) &&
			                 std::isfinite(pokemon.getAlertness());
			summary.insideBounds =
				summary.insideBounds && std::fabs(position.x) <= FIELD_LIMIT &&
				std::fabs(position.z) <= FIELD_LIMIT &&
				position.y >= MIN_FLIGHT_HEIGHT && position.y <= MAX_FLIGHT_HEIGHT;
			const float speed = glm::length(velocity);
			summary.maximumFlyingSpeed =
				std::max(summary.maximumFlyingSpeed, speed);
			const int metricIndex = GROUND_POKEMON_COUNT + index;
			const PokemonBehaviorState state = pokemon.getBehaviorState();
			const bool expectedToMove = state == PokemonBehaviorState::Wander ||
			                            state == PokemonBehaviorState::Flee;
			stationarySeconds[metricIndex] =
				expectedToMove && speed <= 0.05f
					? stationarySeconds[metricIndex] + FIXED_STEP_SECONDS
					: 0.0f;
			if (stationarySeconds[metricIndex] >
			    summary.maximumStationarySeconds)
			{
				summary.maximumStationarySeconds =
					stationarySeconds[metricIndex];
				summary.maximumStationaryTime = time;
				summary.maximumStationaryPokemon = index;
				summary.maximumStationaryState = pokemon.getBehaviorState();
				summary.maximumStationaryFlying = true;
			}
		}
	}

	for (const Pokemon &pokemon : ground)
	{
		const glm::vec3 position = pokemon.getPos();
		summary.finalChecksum += position.x * 0.73 + position.z * 1.37 +
		                         pokemon.getAlertness() * 2.11 +
		                         static_cast<int>(pokemon.getBehaviorState()) * 3.17;
	}
	for (const Pokemon &pokemon : flying)
	{
		const glm::vec3 position = pokemon.getPos();
		summary.finalChecksum += position.x * 0.41 + position.y * 0.83 +
		                         position.z * 1.19 +
		                         pokemon.getAlertness() * 2.53 +
		                         static_cast<int>(pokemon.getBehaviorState()) * 4.07;
	}
	return summary;
}

void testMultiSeedTenMinuteSimulationIsStableAndDeterministic()
{
	const std::array<std::uint32_t, 3> seeds = {
		0xC0FFEEu, 0xA11CEu, 0x5EED5u};
	std::vector<double> checksums;
	checksums.reserve(seeds.size());
	for (const std::uint32_t seed : seeds)
	{
		const SimulationSummary first = runTenMinuteSimulation(seed);
		const SimulationSummary second = runTenMinuteSimulation(seed);
		const std::string context = "seed " + std::to_string(seed) + ": ";

		expectTrue(first.finite,
		           context + "positions, velocities, and alerts remain finite");
		expectTrue(first.insideBounds,
		           context + "ground and flying Pokemon remain inside bounds");
		expectTrue(first.maximumGroundSpeed <= 4.2001f,
		           context + "ground Pokemon respect the flee-speed cap");
		expectTrue(first.maximumFlyingSpeed <= 4.8001f,
		           context + "flying Pokemon respect the flee-speed cap");
		expectTrue(first.minimumStaticClearance >= -0.001f,
		           context + "navigation stays outside rocks and camp props");
		expectTrue(first.maximumPairOverlapSeconds < 1.0f,
		           context + "no ground pair overlaps for one second");
		expectTrue(first.maximumStationarySeconds < 30.0f,
		           context + "no active Pokemon stalls for 30 seconds");
		expectTrue(first.stateTransitions > 100,
		           context + "population behavior continues changing");
		expectTrue(first.alertEvents > 0 && first.fleeTransitions > 0,
		           context + "warning and timid flee behavior are exercised");
		expectTrue(first.umbreonAttacks > 0 && first.charizardAttacks > 0,
		           context + "ground and aerial attack loops are exercised");
		expectTrue(first.nightUmbreonPresent > first.dayUmbreonPresent &&
		               first.dayMeadowPresent > first.nightMeadowPresent,
		           context + "day and night expose different encounter pools");
		expectTrue(first.stateTransitions == second.stateTransitions &&
		               first.alertEvents == second.alertEvents &&
		               first.fleeTransitions == second.fleeTransitions &&
		               first.umbreonAttacks == second.umbreonAttacks &&
		               first.charizardAttacks == second.charizardAttacks &&
		               first.dayUmbreonPresent == second.dayUmbreonPresent &&
		               first.nightUmbreonPresent == second.nightUmbreonPresent &&
		               first.dayMeadowPresent == second.dayMeadowPresent &&
		               first.nightMeadowPresent == second.nightMeadowPresent,
		           context + "repeated runs produce identical event totals");
		expectNear(first.finalChecksum, second.finalChecksum, 0.000001,
		           context + "repeated runs produce the same final state");
		checksums.push_back(first.finalChecksum);

		std::cout << "10-minute seed " << seed
		          << " metrics: transitions=" << first.stateTransitions
		          << ", alerts=" << first.alertEvents
		          << ", flees=" << first.fleeTransitions
		          << ", Umbreon attacks=" << first.umbreonAttacks
		          << ", Charizard attacks=" << first.charizardAttacks
		          << ", day/night Umbreon=" << first.dayUmbreonPresent << "/"
		          << first.nightUmbreonPresent
		          << ", day/night meadow=" << first.dayMeadowPresent << "/"
		          << first.nightMeadowPresent
		          << ", min static clearance=" << first.minimumStaticClearance
		          << ", max pair overlap=" << first.maximumPairOverlapSeconds
		          << "s, max stationary=" << first.maximumStationarySeconds
		          << "s, checksum=" << first.finalChecksum << std::endl;
	}
	expectTrue(checksums[0] != checksums[1] && checksums[0] != checksums[2] &&
	               checksums[1] != checksums[2],
	           "different seeds produce distinct final population states");
}
}

int main()
{
	testMultiSeedTenMinuteSimulationIsStableAndDeterministic();
	if (failures != 0)
	{
		return 1;
	}
	std::cout << "World simulation tests passed" << std::endl;
	return 0;
}
