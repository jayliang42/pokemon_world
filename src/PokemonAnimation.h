#pragma once

#include <string>

struct PokemonAnimationInput
{
	bool flying = false;
	bool fleeing = false;
	float speedRatio = 0.0f;
	float verticalSpeedRatio = 0.0f;
	float turnRatio = 0.0f;
	float phase = 0.0f;
};

struct PokemonAnimationPose
{
	float bodyBob = 0.0f;
	float bodyPitch = 0.0f;
	float bodyRoll = 0.0f;
	float breathingScale = 1.0f;
	float strideAngle = 0.0f;
	float wingAngle = 0.0f;
	float tailAngle = 0.0f;
};

struct PokemonPartAnimation
{
	float pitch = 0.0f;
	float yaw = 0.0f;
	float roll = 0.0f;
};

float advancePokemonAnimationPhase(float phase, float deltaSeconds,
	                               bool flying, bool fleeing, float speedRatio);
PokemonAnimationPose samplePokemonAnimation(const PokemonAnimationInput &input);
PokemonPartAnimation samplePokemonPartAnimation(const std::string &partName,
	                                            const PokemonAnimationPose &pose);
