#include "PokemonGroupAlert.h"

#include <algorithm>
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

PokemonGroupAlertCandidate candidate(
	int id, PokemonSpecies species, float x, float z)
{
	PokemonGroupAlertCandidate value;
	value.id = id;
	value.species = species;
	value.position = glm::vec3(x, 0.0f, z);
	value.eligible = true;
	value.sightlineClear = true;
	return value;
}

bool includes(const PokemonGroupAlertResult &result, int id)
{
	return std::find(result.recipientIds.begin(), result.recipientIds.end(), id) !=
	       result.recipientIds.end();
}

void testOnlyNearbyVisibleSameSpeciesReceiveAlert()
{
	PokemonGroupAlertState state;
	auto nearby = candidate(2, PokemonSpecies::Eevee, 8.0f, 0.0f);
	auto otherSpecies = candidate(3, PokemonSpecies::Bulbasaur, 4.0f, 0.0f);
	auto distant = candidate(4, PokemonSpecies::Eevee, 12.0f, 0.0f);
	auto occluded = candidate(5, PokemonSpecies::Eevee, 6.0f, 0.0f);
	occluded.sightlineClear = false;
	auto alreadyAlert = candidate(6, PokemonSpecies::Eevee, 5.0f, 0.0f);
	alreadyAlert.alertness = POKEMON_GROUP_ALERT_MAX_RECIPIENT_ALERTNESS;
	const PokemonGroupAlertResult result = propagatePokemonGroupAlert(
		state, 10.0, 1, PokemonSpecies::Eevee, glm::vec3(0.0f),
		{nearby, otherSpecies, distant, occluded, alreadyAlert});
	expectTrue(result.propagated && result.recipientIds.size() == 1 &&
	               includes(result, nearby.id),
	           "only a calm nearby same-species companion with clear sight receives the alert");
	expectTrue(state.nextPropagationTime ==
	               10.0 + POKEMON_GROUP_ALERT_COOLDOWN_SECONDS,
	           "a successful propagation starts the group cooldown");
}

void testCooldownStopsImmediateAlertChains()
{
	PokemonGroupAlertState state;
	const auto firstTarget = candidate(2, PokemonSpecies::Bulbasaur, 4.0f, 0.0f);
	const PokemonGroupAlertResult first = propagatePokemonGroupAlert(
		state, 4.0, 1, PokemonSpecies::Bulbasaur, glm::vec3(0.0f), {firstTarget});
	const auto chainedTarget = candidate(3, PokemonSpecies::Bulbasaur, 8.0f, 0.0f);
	const PokemonGroupAlertResult chained = propagatePokemonGroupAlert(
		state, 4.1, 2, PokemonSpecies::Bulbasaur, glm::vec3(4.0f, 0.0f, 0.0f),
		{chainedTarget});
	const PokemonGroupAlertResult afterCooldown = propagatePokemonGroupAlert(
		state, 6.5, 2, PokemonSpecies::Bulbasaur, glm::vec3(4.0f, 0.0f, 0.0f),
		{chainedTarget});
	expectTrue(first.propagated && !chained.propagated &&
	               afterCooldown.propagated && includes(afterCooldown, 3),
	           "the cooldown stops same-frame chains and reopens at its exact boundary");
}

void testNoRecipientDoesNotSpendCooldownAndInvalidInputFailsClosed()
{
	PokemonGroupAlertState state;
	const auto wrongSpecies = candidate(2, PokemonSpecies::Umbreon, 2.0f, 0.0f);
	const PokemonGroupAlertResult empty = propagatePokemonGroupAlert(
		state, 2.0, 1, PokemonSpecies::Eevee, glm::vec3(0.0f), {wrongSpecies});
	expectTrue(!empty.propagated && state.nextPropagationTime == 0.0,
	           "an alert with no recipient preserves the next propagation opportunity");
	const PokemonGroupAlertResult invalid = propagatePokemonGroupAlert(
		state, std::numeric_limits<double>::quiet_NaN(), 1,
		PokemonSpecies::Eevee, glm::vec3(0.0f),
		{candidate(3, PokemonSpecies::Eevee, 2.0f, 0.0f)});
	expectTrue(!invalid.propagated,
	           "non-finite alert time fails closed");
}
}

int main()
{
	testOnlyNearbyVisibleSameSpeciesReceiveAlert();
	testCooldownStopsImmediateAlertChains();
	testNoRecipientDoesNotSpendCooldownAndInvalidInputFailsClosed();
	if (failures != 0)
	{
		return 1;
	}
	std::cout << "Pokemon group alert tests passed" << std::endl;
	return 0;
}
