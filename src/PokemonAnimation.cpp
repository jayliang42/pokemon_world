#include "PokemonAnimation.h"

#include <algorithm>
#include <cmath>

namespace
{
constexpr float TWO_PI = 6.28318530717958647692f;

float clampValue(float value, float minimum, float maximum)
{
	return std::max(minimum, std::min(maximum, value));
}
}

float advancePokemonAnimationPhase(float phase, float deltaSeconds,
	                               bool flying, bool fleeing, float speedRatio)
{
	const float speed = clampValue(speedRatio, 0.0f, 1.0f);
	const float step = clampValue(deltaSeconds, 0.0f, 0.05f);
	float frequency;
	if (flying)
	{
		frequency = 3.2f + speed * 3.8f + (fleeing ? 1.0f : 0.0f);
	}
	else
	{
		frequency = 0.8f + speed * (fleeing ? 9.0f : 6.0f);
	}
	phase = std::fmod(phase + frequency * step, TWO_PI);
	return phase < 0.0f ? phase + TWO_PI : phase;
}

PokemonAnimationPose samplePokemonAnimation(const PokemonAnimationInput &input)
{
	const float speed = clampValue(input.speedRatio, 0.0f, 1.0f);
	const float vertical = clampValue(input.verticalSpeedRatio, -1.0f, 1.0f);
	const float turn = clampValue(input.turnRatio, -1.0f, 1.0f);
	const float cycle = std::sin(input.phase);
	PokemonAnimationPose pose;

	pose.breathingScale = 1.0f + cycle * 0.012f * (1.0f - speed);
	if (input.flying)
	{
		pose.bodyBob = std::sin(input.phase * 2.0f) * (0.025f + speed * 0.025f);
		pose.bodyPitch = vertical * 0.16f - speed * 0.035f;
		pose.bodyRoll = -turn * (0.10f + speed * 0.20f);
		pose.wingAngle = cycle * (0.42f + (input.fleeing ? 0.16f : 0.0f));
		pose.tailAngle = std::sin(input.phase * 0.5f + 0.7f) * (0.10f + speed * 0.10f);
	}
	else
	{
		pose.bodyBob = std::fabs(cycle) * speed * (input.fleeing ? 0.060f : 0.035f);
		pose.bodyPitch = -speed * (input.fleeing ? 0.13f : 0.07f);
		pose.bodyRoll = -turn * speed * 0.08f;
		pose.strideAngle = cycle * speed * (input.fleeing ? 0.68f : 0.46f);
		pose.tailAngle = std::sin(input.phase * 0.5f + 0.7f) * (0.06f + speed * 0.12f);
	}
	return pose;
}

PokemonPartAnimation samplePokemonPartAnimation(const std::string &partName,
	                                            const PokemonAnimationPose &pose)
{
	PokemonPartAnimation part;
	if (partName.find("leg-") != std::string::npos)
	{
		const bool forwardPair =
			partName.find("front-left") != std::string::npos ||
			partName.find("back-right") != std::string::npos;
		part.pitch = pose.strideAngle * (forwardPair ? 1.0f : -1.0f);
	}
	else if (partName.find("wing-left") != std::string::npos)
	{
		const float flap = std::fabs(pose.wingAngle) > 0.0001f
		                       ? pose.wingAngle
		                       : pose.strideAngle * 0.24f;
		part.roll = flap;
	}
	else if (partName.find("wing-right") != std::string::npos)
	{
		const float flap = std::fabs(pose.wingAngle) > 0.0001f
		                       ? pose.wingAngle
		                       : pose.strideAngle * 0.24f;
		part.roll = -flap;
	}
	else if (partName.find("tail") != std::string::npos)
	{
		part.yaw = pose.tailAngle;
	}
	return part;
}
