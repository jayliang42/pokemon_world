#pragma once

#include <cstddef>
#include <string>
#include <vector>

#include "ResearchMission.h"

struct GamePokemonSaveState
{
	int health = 1;
	bool caught = false;
};

struct GameSaveData
{
	int caughtCount = 0;
	int pokeballs = 0;
	int defeatedCount = 0;
	int playerHealth = 1;
	ResearchMissionProgress missionProgress;
	std::vector<GamePokemonSaveState> groundPokemon;
	std::vector<GamePokemonSaveState> flyingPokemon;
};

struct GameSaveLimits
{
	int captureGoal = 1;
	int startingPokeballs = 1;
	int playerMaximumHealth = 1;
	std::vector<int> groundMaximumHealth;
	std::vector<int> flyingMaximumHealth;
};

struct GameSaveParseResult
{
	bool valid = false;
	GameSaveData data;
	std::string error;
};

constexpr std::size_t MAX_GAME_SAVE_BYTES = 8192;

bool validateGameSave(const GameSaveData &data, const GameSaveLimits &limits,
	                  std::string *error = nullptr);
std::string encodeGameSave(const GameSaveData &data,
	                       const GameSaveLimits &limits);
GameSaveParseResult parseGameSave(const std::string &payload,
	                              const GameSaveLimits &limits);
