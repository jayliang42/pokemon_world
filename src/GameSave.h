#pragma once

#include <cstddef>
#include <string>
#include <vector>

#include "ResearchMission.h"
#include "ResearchProgression.h"

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
	bool researchSubmitted = false;
	int researchLevel = RESEARCH_LEVEL_TRAINEE;
	int luresRemaining = 0;
	bool alphaNestResolved = false;
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

enum class GameSaveVersionStatus
{
	Malformed,
	Current,
	Migrated,
	UnsupportedOlder,
	UnsupportedNewer
};

struct GameSaveParseResult
{
	bool valid = false;
	int sourceVersion = -1;
	GameSaveVersionStatus versionStatus = GameSaveVersionStatus::Malformed;
	GameSaveData data;
	std::string error;
};

constexpr std::size_t MAX_GAME_SAVE_BYTES = 8192;
constexpr int CURRENT_GAME_SAVE_VERSION = 6;

std::string currentGameSaveHeader();
bool validateGameSave(const GameSaveData &data, const GameSaveLimits &limits,
	                  std::string *error = nullptr);
std::string encodeGameSave(const GameSaveData &data,
	                       const GameSaveLimits &limits);
GameSaveParseResult parseGameSave(const std::string &payload,
	                              const GameSaveLimits &limits);
