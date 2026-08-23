#include "GameSaveStorage.h"

#include "GameSave.h"

#include <cerrno>
#include <cstdio>
#include <fstream>
#include <vector>

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

GameSaveStorageReadResult readGameSaveStorage(const std::string &nativePath)
{
	GameSaveStorageReadResult result;
#ifdef __EMSCRIPTEN__
	std::vector<char> buffer(MAX_GAME_SAVE_BYTES + 1, '\0');
	const int readResult = EM_ASM_INT({
		try
		{
			const value = localStorage.getItem('pokemon-world-save-v1');
			if (value === null)
			{
				return 0;
			}
			const byteLength = lengthBytesUTF8(value);
			if (byteLength > $1 - 1)
			{
				return -1;
			}
			stringToUTF8(value, $0, $1);
			return byteLength + 1;
		}
		catch (error)
		{
			return -1;
		}
	}, buffer.data(), static_cast<int>(buffer.size()));
	if (readResult == 0)
	{
		return result;
	}
	if (readResult < 0)
	{
		result.status = GameSaveStorageStatus::Error;
		return result;
	}
	result.status = GameSaveStorageStatus::Success;
	result.payload.assign(buffer.data(), static_cast<std::size_t>(readResult - 1));
#else
	errno = 0;
	std::ifstream input(nativePath, std::ios::binary);
	if (!input.is_open())
	{
		result.status = errno == ENOENT ? GameSaveStorageStatus::NotFound
		                                : GameSaveStorageStatus::Error;
		return result;
	}
	input.seekg(0, std::ios::end);
	const std::streamoff size = input.tellg();
	if (size < 0 || size > static_cast<std::streamoff>(MAX_GAME_SAVE_BYTES))
	{
		result.status = GameSaveStorageStatus::Error;
		return result;
	}
	input.seekg(0, std::ios::beg);
	result.payload.resize(static_cast<std::size_t>(size));
	if (size > 0)
	{
		input.read(&result.payload[0], size);
	}
	if (!input && size > 0)
	{
		result.payload.clear();
		result.status = GameSaveStorageStatus::Error;
		return result;
	}
	result.status = GameSaveStorageStatus::Success;
#endif
	return result;
}

bool writeGameSaveStorage(const std::string &payload,
	                      const std::string &nativePath)
{
	if (payload.empty() || payload.size() > MAX_GAME_SAVE_BYTES)
	{
		return false;
	}
#ifdef __EMSCRIPTEN__
	return EM_ASM_INT({
		try
		{
			const value = UTF8ToString($0);
			if (lengthBytesUTF8(value) > $1)
			{
				return 0;
			}
			localStorage.setItem('pokemon-world-save-v1', value);
			return 1;
		}
		catch (error)
		{
			return 0;
		}
	}, payload.c_str(), static_cast<int>(MAX_GAME_SAVE_BYTES)) != 0;
#else
	const std::string temporaryPath = nativePath + ".tmp";
	{
		std::ofstream output(temporaryPath,
		                     std::ios::binary | std::ios::trunc);
		if (!output.is_open())
		{
			return false;
		}
		output.write(payload.data(), static_cast<std::streamsize>(payload.size()));
		output.flush();
		if (!output)
		{
			output.close();
			std::remove(temporaryPath.c_str());
			return false;
		}
	}
#ifdef _WIN32
	std::remove(nativePath.c_str());
#endif
	if (std::rename(temporaryPath.c_str(), nativePath.c_str()) != 0)
	{
		std::remove(temporaryPath.c_str());
		return false;
	}
	return true;
#endif
}

bool clearGameSaveStorage(const std::string &nativePath)
{
#ifdef __EMSCRIPTEN__
	return EM_ASM_INT({
		try
		{
			localStorage.removeItem('pokemon-world-save-v1');
			return 1;
		}
		catch (error)
		{
			return 0;
		}
	}) != 0;
#else
	if (std::remove(nativePath.c_str()) == 0)
	{
		return true;
	}
	return errno == ENOENT;
#endif
}
