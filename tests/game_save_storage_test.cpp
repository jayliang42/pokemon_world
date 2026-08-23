#include "GameSaveStorage.h"

#include "GameSave.h"

#include <cstdio>
#include <fstream>
#include <iostream>
#include <string>

namespace
{
int failures = 0;
const std::string TEST_PATH = "pokemon_world_storage_test.save";

void expectTrue(bool condition, const std::string &message)
{
	if (!condition)
	{
		std::cerr << "FAIL: " << message << std::endl;
		++failures;
	}
}

void testNativeStorageLifecycle()
{
	clearGameSaveStorage(TEST_PATH);
	GameSaveStorageReadResult read = readGameSaveStorage(TEST_PATH);
	expectTrue(read.status == GameSaveStorageStatus::NotFound,
	           "missing native save reports NotFound");

	expectTrue(writeGameSaveStorage("PW_SAVE_V1\nfirst", TEST_PATH),
	           "native storage writes a bounded payload");
	read = readGameSaveStorage(TEST_PATH);
	expectTrue(read.status == GameSaveStorageStatus::Success &&
	               read.payload == "PW_SAVE_V1\nfirst",
	           "native storage reads back the complete payload");

	expectTrue(writeGameSaveStorage("PW_SAVE_V1\nsecond", TEST_PATH),
	           "native storage atomically replaces an existing save");
	read = readGameSaveStorage(TEST_PATH);
	expectTrue(read.status == GameSaveStorageStatus::Success &&
	               read.payload == "PW_SAVE_V1\nsecond",
	           "replacement save becomes the authoritative payload");

	expectTrue(clearGameSaveStorage(TEST_PATH),
	           "native save can be cleared for an explicit new game");
	expectTrue(readGameSaveStorage(TEST_PATH).status ==
	               GameSaveStorageStatus::NotFound,
	           "cleared save no longer loads");
}

void testStorageRejectsOversizedPayloads()
{
	expectTrue(!writeGameSaveStorage(
	               std::string(MAX_GAME_SAVE_BYTES + 1, 'x'), TEST_PATH),
	           "storage refuses oversized writes");
	{
		std::ofstream output(TEST_PATH, std::ios::binary | std::ios::trunc);
		output << std::string(MAX_GAME_SAVE_BYTES + 1, 'x');
	}
	const GameSaveStorageReadResult read = readGameSaveStorage(TEST_PATH);
	expectTrue(read.status == GameSaveStorageStatus::Error && read.payload.empty(),
	           "storage refuses oversized data already present on disk");
	clearGameSaveStorage(TEST_PATH);
}
}

int main()
{
	testNativeStorageLifecycle();
	testStorageRejectsOversizedPayloads();
	std::remove((TEST_PATH + ".tmp").c_str());
	std::remove(TEST_PATH.c_str());
	if (failures != 0)
	{
		std::cerr << failures << " game save storage checks failed" << std::endl;
		return 1;
	}
	std::cout << "Game save storage checks passed" << std::endl;
	return 0;
}
