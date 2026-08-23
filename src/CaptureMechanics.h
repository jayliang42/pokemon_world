#pragma once

#include <cstdint>

#include "PokemonSpecies.h"

enum class CaptureActivity
{
	Idle,
	Moving,
	Fleeing,
};

struct CaptureAttempt
{
	PokemonSpecies species = PokemonSpecies::Bulbasaur;
	float distance = 0.0f;
	float maximumDistance = 5.0f;
	float alignment = 1.0f;
	CaptureActivity activity = CaptureActivity::Moving;
};

struct CaptureResult
{
	float probability = 0.0f;
	bool captured = false;
	int shakes = 0;
};

float calculateCaptureProbability(const CaptureAttempt &attempt);
CaptureResult resolveCaptureAttempt(const CaptureAttempt &attempt,
	                                float randomRoll);

class CaptureRandom
{
public:
	explicit CaptureRandom(std::uint32_t seed = 0xC0FFEEu);
	float nextUnit();

private:
	std::uint32_t state_;
};
