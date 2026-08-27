#include "GameSession.h"

#include <cmath>
#include <iostream>
#include <limits>
#include <string>
#include <vector>

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

void expectNear(double actual, double expected, double tolerance,
	            const std::string &message)
{
	if (std::fabs(actual - expected) > tolerance)
	{
		std::cerr << "FAIL: " << message << " (expected " << expected
		          << ", got " << actual << ')' << std::endl;
		++failures;
	}
}

void testFractionalFramesAccumulateIntoOneFixedStep()
{
	GameSession session;
	int callbackCount = 0;
	const double step = session.fixedStepSeconds();

	const GameSessionFrame first = session.advance(
		step * 0.4, [&](const GameSessionStep &) { ++callbackCount; });
	const GameSessionFrame second = session.advance(
		step * 0.6, [&](const GameSessionStep &) { ++callbackCount; });

	expectTrue(first.simulationSteps == 0,
	           "a partial frame does not advance the simulation");
	expectTrue(second.simulationSteps == 1,
	           "accumulated partial frames produce one fixed step");
	expectTrue(callbackCount == 1,
	           "the simulation callback runs exactly once per fixed step");
	expectNear(session.simulationTimeSeconds(), step, 1e-10,
	           "simulation time advances by the fixed step");
}

void testFrameChunkingProducesTheSameSimulationTimeline()
{
	GameSession oneFrame;
	GameSession twoFrames;
	std::vector<double> oneFrameTimes;
	std::vector<double> twoFrameTimes;
	const double step = oneFrame.fixedStepSeconds();

	oneFrame.advance(step * 2.0, [&](const GameSessionStep &simulationStep) {
		oneFrameTimes.push_back(simulationStep.simulationTimeSeconds);
	});
	twoFrames.advance(step, [&](const GameSessionStep &simulationStep) {
		twoFrameTimes.push_back(simulationStep.simulationTimeSeconds);
	});
	twoFrames.advance(step, [&](const GameSessionStep &simulationStep) {
		twoFrameTimes.push_back(simulationStep.simulationTimeSeconds);
	});

	expectTrue(oneFrameTimes == twoFrameTimes,
	           "equivalent elapsed time produces the same fixed-step timeline");
	expectNear(oneFrame.simulationTimeSeconds(), twoFrames.simulationTimeSeconds(),
	           1e-10, "frame chunking does not change simulation time");
}

void testCallbacksReceiveConstantDeltaAndIncreasingTime()
{
	GameSession session;
	std::vector<GameSessionStep> steps;
	const double step = session.fixedStepSeconds();

	session.advance(step * 3.25, [&](const GameSessionStep &simulationStep) {
		steps.push_back(simulationStep);
	});

	expectTrue(steps.size() == 3,
	           "three complete steps run and the remainder stays accumulated");
	for (std::size_t index = 0; index < steps.size(); ++index)
	{
		expectNear(steps[index].deltaSeconds, step, 1e-10,
		           "every callback receives the configured fixed delta");
		expectNear(steps[index].simulationTimeSeconds,
		           step * static_cast<double>(index + 1), 1e-10,
		           "simulation timestamps increase one fixed step at a time");
	}
}

void testLongFramesAreBoundedAndReportDroppedTime()
{
	GameSessionConfig config;
	config.fixedStepSeconds = 0.01;
	config.maximumFrameSeconds = 0.25;
	config.maximumStepsPerFrame = 4;
	GameSession session(config);

	const GameSessionFrame frame = session.advance(
		1.0, [](const GameSessionStep &) {});

	expectTrue(frame.simulationSteps == 4,
	           "a long frame cannot run more than the configured step budget");
	expectNear(frame.acceptedFrameSeconds, 0.25, 1e-10,
	           "a long frame is clamped before entering the accumulator");
	expectTrue(frame.droppedSeconds > 0.95,
	           "clamped and excess backlog time is reported as dropped");
	expectTrue(frame.interpolationAlpha >= 0.0 && frame.interpolationAlpha < 1.0,
	           "interpolation alpha remains normalized after a long frame");
}

void testInvalidDeltasDoNotChangeTheSession()
{
	GameSession session;
	int callbackCount = 0;

	session.advance(-1.0, [&](const GameSessionStep &) { ++callbackCount; });
	session.advance(std::numeric_limits<double>::quiet_NaN(),
	                [&](const GameSessionStep &) { ++callbackCount; });

	expectTrue(callbackCount == 0,
	           "negative and non-finite frame deltas never run simulation steps");
	expectNear(session.simulationTimeSeconds(), 0.0, 1e-10,
	           "invalid frame deltas leave simulation time unchanged");
}

void testResetClearsClockAndAccumulator()
{
	GameSession session;
	const double step = session.fixedStepSeconds();
	session.advance(step * 1.5, [](const GameSessionStep &) {});

	session.reset();
	const GameSessionFrame frame = session.advance(
		step * 0.5, [](const GameSessionStep &) {});

	expectNear(session.simulationTimeSeconds(), 0.0, 1e-10,
	           "reset clears elapsed simulation time");
	expectTrue(frame.simulationSteps == 0,
	           "reset clears the previously accumulated partial step");
}
}

int main()
{
	testFractionalFramesAccumulateIntoOneFixedStep();
	testFrameChunkingProducesTheSameSimulationTimeline();
	testCallbacksReceiveConstantDeltaAndIncreasingTime();
	testLongFramesAreBoundedAndReportDroppedTime();
	testInvalidDeltasDoNotChangeTheSession();
	testResetClearsClockAndAccumulator();
	if (failures != 0)
	{
		return 1;
	}
	std::cout << "Game session tests passed" << std::endl;
	return 0;
}
