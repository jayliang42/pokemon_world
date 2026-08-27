#include "GameSession.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>

GameSession::GameSession(const GameSessionConfig &config) : config_(config)
{
	if (!std::isfinite(config_.fixedStepSeconds) ||
	    config_.fixedStepSeconds <= 0.0 ||
	    !std::isfinite(config_.maximumFrameSeconds) ||
	    config_.maximumFrameSeconds < config_.fixedStepSeconds ||
	    config_.maximumStepsPerFrame <= 0)
	{
		throw std::invalid_argument("GameSession requires a valid fixed-step configuration");
	}
}

GameSessionFrame GameSession::advance(double frameDeltaSeconds,
	                                  const StepCallback &simulateStep)
{
	GameSessionFrame frame;
	if (!std::isfinite(frameDeltaSeconds) || frameDeltaSeconds <= 0.0)
	{
		frame.interpolationAlpha = accumulatorSeconds_ / config_.fixedStepSeconds;
		return frame;
	}

	frame.acceptedFrameSeconds =
		std::min(frameDeltaSeconds, config_.maximumFrameSeconds);
	frame.droppedSeconds = frameDeltaSeconds - frame.acceptedFrameSeconds;
	accumulatorSeconds_ += frame.acceptedFrameSeconds;

	const double comparisonEpsilon = config_.fixedStepSeconds * 1e-9;
	while (accumulatorSeconds_ + comparisonEpsilon >= config_.fixedStepSeconds &&
	       frame.simulationSteps < config_.maximumStepsPerFrame)
	{
		accumulatorSeconds_ -= config_.fixedStepSeconds;
		if (accumulatorSeconds_ < 0.0)
		{
			accumulatorSeconds_ = 0.0;
		}
		simulationTimeSeconds_ += config_.fixedStepSeconds;
		++frame.simulationSteps;
		if (simulateStep)
		{
			simulateStep({config_.fixedStepSeconds, simulationTimeSeconds_});
		}
	}

	if (accumulatorSeconds_ + comparisonEpsilon >= config_.fixedStepSeconds)
	{
		const double wholeBacklogSteps = std::floor(
			(accumulatorSeconds_ + comparisonEpsilon) / config_.fixedStepSeconds);
		const double droppedBacklog =
			wholeBacklogSteps * config_.fixedStepSeconds;
		accumulatorSeconds_ = std::max(0.0, accumulatorSeconds_ - droppedBacklog);
		frame.droppedSeconds += droppedBacklog;
	}

	frame.interpolationAlpha = accumulatorSeconds_ / config_.fixedStepSeconds;
	return frame;
}

void GameSession::reset()
{
	accumulatorSeconds_ = 0.0;
	simulationTimeSeconds_ = 0.0;
}

double GameSession::fixedStepSeconds() const
{
	return config_.fixedStepSeconds;
}

double GameSession::simulationTimeSeconds() const
{
	return simulationTimeSeconds_;
}
