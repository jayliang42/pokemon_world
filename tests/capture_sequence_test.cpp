#include "CaptureSequence.h"

#include <cmath>
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

void expectNear(float actual, float expected, float tolerance,
	            const std::string &message)
{
	if (std::fabs(actual - expected) > tolerance)
	{
		std::cerr << "FAIL: " << message << " (expected " << expected
		          << ", got " << actual << ")" << std::endl;
		++failures;
	}
}

CaptureResult result(bool captured, int shakes)
{
	CaptureResult value;
	value.captured = captured;
	value.shakes = shakes;
	value.probability = 0.5f;
	return value;
}

void testThrowAndAbsorbPhasesControlVisibility()
{
	const CaptureResult success = result(true, 3);
	expectNear(captureThrowFlightPhaseDuration(), 0.55f, 0.0001f,
	           "the physical projectile can hand off at the absorb boundary");
	const CaptureSequenceSample inactive = sampleCaptureSequence(success, -0.1f);
	const CaptureSequenceSample throwMidpoint = sampleCaptureSequence(success, 0.275f);
	const CaptureSequenceSample absorbing = sampleCaptureSequence(success, 0.60f);
	expectTrue(inactive.phase == CapturePhase::Inactive && !inactive.ballVisible,
	           "negative elapsed time keeps the sequence inactive");
	expectTrue(throwMidpoint.phase == CapturePhase::Throwing &&
	               throwMidpoint.pokemonVisible && throwMidpoint.ballVisible,
	           "the target remains visible while the ball travels");
	expectNear(throwMidpoint.phaseProgress, 0.5f, 0.001f,
	           "throw progress is normalized");
	expectTrue(absorbing.phase == CapturePhase::Absorbing &&
	               !absorbing.pokemonVisible && absorbing.ballVisible,
	           "the target hides when absorption begins");
}

void testSuccessfulAttemptCompletesThreeShakes()
{
	const CaptureResult success = result(true, 3);
	const CaptureSequenceSample firstShake = sampleCaptureSequence(success, 0.90f);
	const CaptureSequenceSample thirdShake = sampleCaptureSequence(success, 1.95f);
	const CaptureSequenceSample resolved = sampleCaptureSequence(success, 2.50f);
	const CaptureSequenceSample finished =
		sampleCaptureSequence(success, captureSequenceDuration(success));
	expectTrue(firstShake.phase == CapturePhase::Shaking && firstShake.shakeIndex == 1,
	           "successful sequence enters its first shake");
	expectTrue(thirdShake.phase == CapturePhase::Shaking && thirdShake.shakeIndex == 3,
	           "successful sequence reaches its third shake");
	expectTrue(resolved.phase == CapturePhase::Succeeded && resolved.ballVisible,
	           "successful resolution keeps the closed ball visible briefly");
	expectTrue(finished.phase == CapturePhase::Finished && finished.finished &&
	               !finished.pokemonVisible && !finished.ballVisible,
	           "successful sequence finishes with the Pokemon captured");
}

void testFailedAttemptBreaksAfterConfiguredShakes()
{
	const CaptureResult failure = result(false, 1);
	const CaptureSequenceSample shaking = sampleCaptureSequence(failure, 1.00f);
	const CaptureSequenceSample brokeFree = sampleCaptureSequence(failure, 1.40f);
	const CaptureSequenceSample finished =
		sampleCaptureSequence(failure, captureSequenceDuration(failure));
	expectTrue(shaking.phase == CapturePhase::Shaking && shaking.shakeIndex == 1,
	           "failed sequence performs its configured shake");
	expectTrue(brokeFree.phase == CapturePhase::BrokeFree &&
	               brokeFree.pokemonVisible && !brokeFree.ballVisible,
	           "failed resolution reveals the Pokemon and hides the ball");
	expectTrue(finished.finished && finished.pokemonVisible,
	           "failed sequence leaves the Pokemon in the field");
}

void testShakeCountAndDurationAreClamped()
{
	const CaptureResult excessive = result(true, 99);
	const CaptureResult negative = result(false, -4);
	expectNear(captureSequenceDuration(excessive), 2.96f, 0.001f,
	           "sequence duration clamps to three shakes");
	expectNear(captureSequenceDuration(negative), 1.40f, 0.001f,
	           "sequence duration clamps negative shakes to zero");
}
}

int main()
{
	testThrowAndAbsorbPhasesControlVisibility();
	testSuccessfulAttemptCompletesThreeShakes();
	testFailedAttemptBreaksAfterConfiguredShakes();
	testShakeCountAndDurationAreClamped();

	if (failures != 0)
	{
		std::cerr << failures << " capture sequence test(s) failed" << std::endl;
		return 1;
	}

	std::cout << "All capture sequence tests passed" << std::endl;
	return 0;
}
