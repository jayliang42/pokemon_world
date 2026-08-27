#pragma once

#include "CaptureMechanics.h"

enum class CapturePhase
{
	Inactive,
	Throwing,
	Absorbing,
	Shaking,
	Succeeded,
	BrokeFree,
	Finished,
};

struct CaptureSequenceSample
{
	CapturePhase phase = CapturePhase::Inactive;
	float phaseProgress = 0.0f;
	int shakeIndex = 0;
	bool pokemonVisible = true;
	bool ballVisible = false;
	bool finished = false;
};

float captureSequenceDuration(const CaptureResult &result);
float captureThrowFlightPhaseDuration();
CaptureSequenceSample sampleCaptureSequence(const CaptureResult &result,
	                                         float elapsedSeconds);
