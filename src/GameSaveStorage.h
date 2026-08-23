#pragma once

#include <string>

enum class GameSaveStorageStatus
{
	NotFound,
	Success,
	Error,
};

struct GameSaveStorageReadResult
{
	GameSaveStorageStatus status = GameSaveStorageStatus::NotFound;
	std::string payload;
};

GameSaveStorageReadResult readGameSaveStorage(
	const std::string &nativePath = "pokemon_world.save");
bool writeGameSaveStorage(
	const std::string &payload,
	const std::string &nativePath = "pokemon_world.save");
bool clearGameSaveStorage(
	const std::string &nativePath = "pokemon_world.save");
