#include "GameSave.h"

#include <algorithm>
#include <sstream>

namespace
{
constexpr int MAX_MISSION_EVENT_COUNT = 999;
constexpr std::size_t MAX_POKEMON_SLOTS = 256;

bool fail(std::string *error, const std::string &message)
{
	if (error)
	{
		*error = message;
	}
	return false;
}

bool validLimits(const GameSaveLimits &limits, std::string *error)
{
	const std::size_t totalSlots = limits.groundMaximumHealth.size() +
	                               limits.flyingMaximumHealth.size();
	if (limits.captureGoal <= 0 || limits.startingPokeballs <= 0 ||
	    limits.playerMaximumHealth <= 0 || totalSlots == 0 ||
	    totalSlots > MAX_POKEMON_SLOTS)
	{
		return fail(error, "invalid save limits");
	}
	for (int maximumHealth : limits.groundMaximumHealth)
	{
		if (maximumHealth <= 0)
		{
			return fail(error, "invalid ground Pokemon health limit");
		}
	}
	for (int maximumHealth : limits.flyingMaximumHealth)
	{
		if (maximumHealth <= 0)
		{
			return fail(error, "invalid flying Pokemon health limit");
		}
	}
	return true;
}

bool parseNonNegativeInt(const std::string &text, int maximum, int &value)
{
	if (text.empty() || text.size() > 10 || maximum < 0)
	{
		return false;
	}
	int parsed = 0;
	for (char character : text)
	{
		if (character < '0' || character > '9')
		{
			return false;
		}
		const int digit = character - '0';
		if (parsed > maximum / 10 ||
		    (parsed == maximum / 10 && digit > maximum % 10))
		{
			return false;
		}
		parsed = parsed * 10 + digit;
	}
	value = parsed;
	return true;
}

bool readField(const std::string &line, const std::string &name,
	           std::string &value)
{
	const std::string prefix = name + "=";
	if (line.compare(0, prefix.size(), prefix) != 0)
	{
		return false;
	}
	value = line.substr(prefix.size());
	return !value.empty();
}

bool parsePokemonStates(const std::string &encoded,
	                    const std::vector<int> &maximumHealth,
	                    std::vector<GamePokemonSaveState> &states)
{
	states.clear();
	states.reserve(maximumHealth.size());
	std::size_t start = 0;
	while (start <= encoded.size())
	{
		const std::size_t comma = encoded.find(',', start);
		const std::size_t end = comma == std::string::npos ? encoded.size() : comma;
		const std::string token = encoded.substr(start, end - start);
		const std::size_t separator = token.find(':');
		if (separator == std::string::npos ||
		    token.find(':', separator + 1) != std::string::npos ||
		    states.size() >= maximumHealth.size())
		{
			return false;
		}
		int health = 0;
		int caught = 0;
		if (!parseNonNegativeInt(token.substr(0, separator),
		                         maximumHealth[states.size()], health) ||
		    !parseNonNegativeInt(token.substr(separator + 1), 1, caught))
		{
			return false;
		}
		states.push_back({health, caught != 0});
		if (comma == std::string::npos)
		{
			break;
		}
		start = comma + 1;
	}
	return states.size() == maximumHealth.size();
}

std::string encodePokemonStates(
	const std::vector<GamePokemonSaveState> &states)
{
	std::ostringstream encoded;
	for (std::size_t index = 0; index < states.size(); ++index)
	{
		if (index != 0)
		{
			encoded << ',';
		}
		encoded << states[index].health << ':' << (states[index].caught ? 1 : 0);
	}
	return encoded.str();
}
}

bool validateGameSave(const GameSaveData &data, const GameSaveLimits &limits,
	                  std::string *error)
{
	if (!validLimits(limits, error))
	{
		return false;
	}
	if (data.groundPokemon.size() != limits.groundMaximumHealth.size() ||
	    data.flyingPokemon.size() != limits.flyingMaximumHealth.size())
	{
		return fail(error, "Pokemon slot count does not match this game version");
	}
	if (data.caughtCount < 0 || data.caughtCount > limits.captureGoal ||
	    data.pokeballs < 0 || data.pokeballs > limits.startingPokeballs ||
	    data.defeatedCount < 0 ||
	    data.defeatedCount > static_cast<int>(data.groundPokemon.size() +
	                                           data.flyingPokemon.size()) ||
	    data.playerHealth < 0 ||
	    data.playerHealth > limits.playerMaximumHealth ||
	    data.missionProgress.superEffectiveHits < 0 ||
	    data.missionProgress.superEffectiveHits > MAX_MISSION_EVENT_COUNT ||
	    data.missionProgress.safeLandings < 0 ||
	    data.missionProgress.safeLandings > MAX_MISSION_EVENT_COUNT)
	{
		return fail(error, "save counters are out of range");
	}

	int countedCaught = 0;
	int countedFainted = 0;
	auto validateStates = [&](const std::vector<GamePokemonSaveState> &states,
	                          const std::vector<int> &maximumHealth) {
		for (std::size_t index = 0; index < states.size(); ++index)
		{
			const GamePokemonSaveState &state = states[index];
			if (state.health < 0 || state.health > maximumHealth[index] ||
			    (state.caught && state.health == 0))
			{
				return false;
			}
			if (state.caught)
			{
				++countedCaught;
			}
			else if (state.health == 0)
			{
				++countedFainted;
			}
		}
		return true;
	};
	if (!validateStates(data.groundPokemon, limits.groundMaximumHealth) ||
	    !validateStates(data.flyingPokemon, limits.flyingMaximumHealth))
	{
		return fail(error, "Pokemon health or capture state is invalid");
	}
	if (countedCaught != data.caughtCount ||
	    countedFainted != data.defeatedCount)
	{
		return fail(error, "save counters do not match Pokemon state");
	}
	const int usedPokeballs = limits.startingPokeballs - data.pokeballs;
	if (usedPokeballs < data.caughtCount)
	{
		return fail(error, "captured count exceeds used Poke Balls");
	}
	return true;
}

std::string encodeGameSave(const GameSaveData &data,
	                       const GameSaveLimits &limits)
{
	if (!validateGameSave(data, limits))
	{
		return std::string();
	}
	std::ostringstream payload;
	payload << "PW_SAVE_V1\n"
	        << "caught=" << data.caughtCount << '\n'
	        << "balls=" << data.pokeballs << '\n'
	        << "defeated=" << data.defeatedCount << '\n'
	        << "player_hp=" << data.playerHealth << '\n'
	        << "super_hits=" << data.missionProgress.superEffectiveHits << '\n'
	        << "safe_landings=" << data.missionProgress.safeLandings << '\n'
	        << "ground=" << encodePokemonStates(data.groundPokemon) << '\n'
	        << "flying=" << encodePokemonStates(data.flyingPokemon);
	const std::string encoded = payload.str();
	return encoded.size() <= MAX_GAME_SAVE_BYTES ? encoded : std::string();
}

GameSaveParseResult parseGameSave(const std::string &payload,
	                              const GameSaveLimits &limits)
{
	GameSaveParseResult result;
	if (payload.empty() || payload.size() > MAX_GAME_SAVE_BYTES)
	{
		result.error = "save payload is empty or too large";
		return result;
	}
	if (!validLimits(limits, &result.error))
	{
		return result;
	}

	std::vector<std::string> lines;
	std::istringstream stream(payload);
	std::string line;
	while (std::getline(stream, line))
	{
		lines.push_back(line);
		if (lines.size() > 9)
		{
			result.error = "save payload has unexpected fields";
			return result;
		}
	}
	if (lines.size() != 9 || lines[0] != "PW_SAVE_V1")
	{
		result.error = "unsupported or malformed save version";
		return result;
	}

	std::string value;
	if (!readField(lines[1], "caught", value) ||
	    !parseNonNegativeInt(value, limits.captureGoal, result.data.caughtCount) ||
	    !readField(lines[2], "balls", value) ||
	    !parseNonNegativeInt(value, limits.startingPokeballs,
	                         result.data.pokeballs) ||
	    !readField(lines[3], "defeated", value) ||
	    !parseNonNegativeInt(
		    value,
		    static_cast<int>(limits.groundMaximumHealth.size() +
		                     limits.flyingMaximumHealth.size()),
		    result.data.defeatedCount) ||
	    !readField(lines[4], "player_hp", value) ||
	    !parseNonNegativeInt(value, limits.playerMaximumHealth,
	                         result.data.playerHealth) ||
	    !readField(lines[5], "super_hits", value) ||
	    !parseNonNegativeInt(value, MAX_MISSION_EVENT_COUNT,
	                         result.data.missionProgress.superEffectiveHits) ||
	    !readField(lines[6], "safe_landings", value) ||
	    !parseNonNegativeInt(value, MAX_MISSION_EVENT_COUNT,
	                         result.data.missionProgress.safeLandings) ||
	    !readField(lines[7], "ground", value) ||
	    !parsePokemonStates(value, limits.groundMaximumHealth,
	                        result.data.groundPokemon) ||
	    !readField(lines[8], "flying", value) ||
	    !parsePokemonStates(value, limits.flyingMaximumHealth,
	                        result.data.flyingPokemon))
	{
		result.error = "save fields are malformed or out of range";
		return result;
	}
	if (!validateGameSave(result.data, limits, &result.error))
	{
		return result;
	}
	result.valid = true;
	return result;
}
