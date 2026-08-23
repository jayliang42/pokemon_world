#pragma once

enum class BattlePhase
{
	Inactive,
	PlayerWindup,
	PlayerProjectile,
	TargetImpact,
	WildWindup,
	WildProjectile,
	PlayerImpact,
	Recovery,
	Finished,
};

struct BattleSequencePlan
{
	bool counterEnabled = true;
};

struct BattleSequenceSample
{
	BattlePhase phase = BattlePhase::Inactive;
	float phaseProgress = 0.0f;
	bool showPlayerProjectile = false;
	bool showWildProjectile = false;
	bool targetImpact = false;
	bool playerImpact = false;
	bool finished = false;
};

float battleSequenceDuration(const BattleSequencePlan &plan);
BattleSequenceSample sampleBattleSequence(const BattleSequencePlan &plan,
	                                        float elapsedSeconds);

