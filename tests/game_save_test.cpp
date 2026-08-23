#include "GameSave.h"

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

GameSaveLimits testLimits()
{
	GameSaveLimits limits;
	limits.captureGoal = 5;
	limits.startingPokeballs = 10;
	limits.playerMaximumHealth = 118;
	limits.groundMaximumHealth = {96, 82, 96, 82};
	limits.flyingMaximumHealth = {118, 118};
	return limits;
}

GameSaveData validSave()
{
	GameSaveData data;
	data.caughtCount = 1;
	data.pokeballs = 8;
	data.defeatedCount = 1;
	data.playerHealth = 112;
	data.missionProgress.superEffectiveHits = 2;
	data.missionProgress.safeLandings = 1;
	data.groundPokemon = {{70, false}, {25, true}, {0, false}, {82, false}};
	data.flyingPokemon = {{118, false}, {90, false}};
	return data;
}

std::string replaceLine(const std::string &payload, const std::string &prefix,
	                    const std::string &replacement)
{
	const std::size_t start = payload.find(prefix);
	if (start == std::string::npos)
	{
		return payload;
	}
	const std::size_t end = payload.find('\n', start);
	return payload.substr(0, start) + replacement +
	       (end == std::string::npos ? std::string() : payload.substr(end));
}

void testValidSaveRoundTripsExactly()
{
	const GameSaveLimits limits = testLimits();
	const GameSaveData original = validSave();
	const std::string encoded = encodeGameSave(original, limits);
	expectTrue(!encoded.empty() && encoded.find("PW_SAVE_V1\n") == 0,
	           "valid progress encodes with an explicit version header");
	const GameSaveParseResult parsed = parseGameSave(encoded, limits);
	expectTrue(parsed.valid, "encoded progress parses successfully");
	expectTrue(parsed.data.caughtCount == original.caughtCount &&
	               parsed.data.pokeballs == original.pokeballs &&
	               parsed.data.defeatedCount == original.defeatedCount &&
	               parsed.data.playerHealth == original.playerHealth,
	           "top-level progress survives a round trip");
	expectTrue(parsed.data.missionProgress.superEffectiveHits == 2 &&
	               parsed.data.missionProgress.safeLandings == 1,
	           "research mission events survive a round trip");
	expectTrue(parsed.data.groundPokemon[1].caught &&
	               parsed.data.groundPokemon[1].health == 25 &&
	               parsed.data.groundPokemon[2].health == 0 &&
	               parsed.data.flyingPokemon[1].health == 90,
	           "per-Pokemon capture, fainting, and health survive a round trip");
}

void testRejectsMalformedVersionFieldsAndNumbers()
{
	const GameSaveLimits limits = testLimits();
	const std::string valid = encodeGameSave(validSave(), limits);
	expectTrue(!parseGameSave("PW_SAVE_V2\n" + valid.substr(11), limits).valid,
	           "unknown save versions fail closed");
	expectTrue(!parseGameSave(replaceLine(valid, "caught=", "caught=-1"),
	                          limits)
	                .valid,
	           "negative counters are rejected");
	expectTrue(!parseGameSave(replaceLine(valid, "balls=", "balls=7oops"),
	                          limits)
	                .valid,
	           "integer fields reject trailing text");
	expectTrue(!parseGameSave(replaceLine(valid, "player_hp=",
	                                     "player_hp=999999999999999999"),
	                          limits)
	                .valid,
	           "overflowing integers are rejected before conversion");
	expectTrue(!parseGameSave(replaceLine(valid, "safe_landings=",
	                                     "unknown=1"),
	                          limits)
	                .valid,
	           "unknown or reordered fields are rejected");
}

void testRejectsOversizedAndIncorrectPokemonState()
{
	const GameSaveLimits limits = testLimits();
	const std::string valid = encodeGameSave(validSave(), limits);
	expectTrue(!parseGameSave(std::string(MAX_GAME_SAVE_BYTES + 1, 'x'), limits)
	                .valid,
	           "oversized local storage payload is rejected before parsing");
	expectTrue(!parseGameSave(replaceLine(valid, "ground=",
	                                     "ground=70:0,25:1"),
	                          limits)
	                .valid,
	           "Pokemon slot count must match this game build");
	expectTrue(!parseGameSave(replaceLine(valid, "ground=",
	                                     "ground=97:0,25:1,0:0,82:0"),
	                          limits)
	                .valid,
	           "health above the slot species maximum is rejected");
	expectTrue(!parseGameSave(replaceLine(valid, "ground=",
	                                     "ground=70:0,0:1,0:0,82:0"),
	                          limits)
	                .valid,
	           "a captured Pokemon cannot also be fainted");
}

void testRejectsTamperedCrossFieldInvariants()
{
	const GameSaveLimits limits = testLimits();
	const std::string valid = encodeGameSave(validSave(), limits);
	expectTrue(!parseGameSave(replaceLine(valid, "caught=", "caught=2"),
	                          limits)
	                .valid,
	           "declared catches must match captured Pokemon slots");
	expectTrue(!parseGameSave(replaceLine(valid, "defeated=", "defeated=0"),
	                          limits)
	                .valid,
	           "declared defeats must match fainted Pokemon slots");
	expectTrue(!parseGameSave(replaceLine(valid, "balls=", "balls=10"), limits)
	                .valid,
	           "captures cannot exceed consumed Poke Balls");

	GameSaveData invalid = validSave();
	invalid.groundPokemon.pop_back();
	expectTrue(encodeGameSave(invalid, limits).empty(),
	           "encoder refuses invalid internal save data");
}
}

int main()
{
	testValidSaveRoundTripsExactly();
	testRejectsMalformedVersionFieldsAndNumbers();
	testRejectsOversizedAndIncorrectPokemonState();
	testRejectsTamperedCrossFieldInvariants();
	if (failures != 0)
	{
		std::cerr << failures << " game save checks failed" << std::endl;
		return 1;
	}
	std::cout << "Game save checks passed" << std::endl;
	return 0;
}
