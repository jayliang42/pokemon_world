#ifndef GAME_SESSION_H
#define GAME_SESSION_H

#include <functional>

struct GameSessionConfig
{
	double fixedStepSeconds = 1.0 / 60.0;
	double maximumFrameSeconds = 0.25;
	int maximumStepsPerFrame = 8;
};

struct GameSessionStep
{
	double deltaSeconds = 0.0;
	double simulationTimeSeconds = 0.0;
};

struct GameSessionFrame
{
	int simulationSteps = 0;
	double acceptedFrameSeconds = 0.0;
	double droppedSeconds = 0.0;
	double interpolationAlpha = 0.0;
};

class GameSession
{
public:
	using StepCallback = std::function<void(const GameSessionStep &)>;

	explicit GameSession(const GameSessionConfig &config = GameSessionConfig());

	GameSessionFrame advance(double frameDeltaSeconds,
	                         const StepCallback &simulateStep);
	void reset();

	double fixedStepSeconds() const;
	double simulationTimeSeconds() const;

private:
	GameSessionConfig config_;
	double accumulatorSeconds_ = 0.0;
	double simulationTimeSeconds_ = 0.0;
};

#endif
