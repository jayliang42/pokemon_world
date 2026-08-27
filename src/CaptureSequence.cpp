#include "CaptureSequence.h"

#include <algorithm>
#include <cmath>

namespace
{
constexpr float THROW_DURATION = 0.55f;
constexpr float ABSORB_DURATION = 0.30f;
constexpr float SHAKE_DURATION = 0.52f;
constexpr float RESOLVE_DURATION = 0.55f;

float clampValue(float value, float minimum, float maximum)
{
	return std::max(minimum, std::min(maximum, value));
}

int clampedShakeCount(const CaptureResult &result)
{
	return std::max(0, std::min(3, result.shakes));
}

float progressWithin(float elapsed, float duration)
{
	return clampValue(elapsed / duration, 0.0f, 1.0f);
}
}

float captureSequenceDuration(const CaptureResult &result)
{
	return THROW_DURATION + ABSORB_DURATION +
	       static_cast<float>(clampedShakeCount(result)) * SHAKE_DURATION +
	       RESOLVE_DURATION;
}

float captureThrowFlightPhaseDuration()
{
	return THROW_DURATION;
}

CaptureSequenceSample sampleCaptureSequence(const CaptureResult &result,
	                                         float elapsedSeconds)
{
	CaptureSequenceSample sample;
	if (elapsedSeconds < 0.0f)
	{
		return sample;
	}
	if (elapsedSeconds >= captureSequenceDuration(result))
	{
		sample.phase = CapturePhase::Finished;
		sample.phaseProgress = 1.0f;
		sample.pokemonVisible = !result.captured;
		sample.finished = true;
		return sample;
	}

	float remaining = elapsedSeconds;
	sample.ballVisible = true;
	if (remaining < THROW_DURATION)
	{
		sample.phase = CapturePhase::Throwing;
		sample.phaseProgress = progressWithin(remaining, THROW_DURATION);
		return sample;
	}
	remaining -= THROW_DURATION;

	sample.pokemonVisible = false;
	if (remaining < ABSORB_DURATION)
	{
		sample.phase = CapturePhase::Absorbing;
		sample.phaseProgress = progressWithin(remaining, ABSORB_DURATION);
		return sample;
	}
	remaining -= ABSORB_DURATION;

	const int shakeCount = clampedShakeCount(result);
	const float totalShakeDuration = static_cast<float>(shakeCount) * SHAKE_DURATION;
	if (remaining < totalShakeDuration)
	{
		sample.phase = CapturePhase::Shaking;
		sample.shakeIndex = std::min(
			shakeCount, static_cast<int>(std::floor(remaining / SHAKE_DURATION)) + 1);
		sample.phaseProgress = std::fmod(remaining, SHAKE_DURATION) / SHAKE_DURATION;
		return sample;
	}
	remaining -= totalShakeDuration;

	if (remaining < RESOLVE_DURATION)
	{
		sample.phase = result.captured ? CapturePhase::Succeeded
		                               : CapturePhase::BrokeFree;
		sample.phaseProgress = progressWithin(remaining, RESOLVE_DURATION);
		sample.ballVisible = result.captured;
		sample.pokemonVisible = !result.captured;
		return sample;
	}

	sample.phase = CapturePhase::Finished;
	sample.phaseProgress = 1.0f;
	sample.pokemonVisible = !result.captured;
	sample.ballVisible = false;
	sample.finished = true;
	return sample;
}
