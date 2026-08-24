#include "BattleSequence.h"

#include <algorithm>

namespace
{
constexpr float PLAYER_WINDUP_DURATION = 0.22f;
constexpr float PLAYER_PROJECTILE_DURATION = 0.46f;
constexpr float TARGET_IMPACT_DURATION = 0.34f;
constexpr float WILD_WINDUP_DURATION = 0.44f;
constexpr float WILD_PROJECTILE_DURATION = 0.42f;
constexpr float PLAYER_IMPACT_DURATION = 0.30f;
constexpr float RECOVERY_DURATION = 0.42f;

float progressWithin(float elapsed, float duration)
{
	return std::max(0.0f, std::min(1.0f, elapsed / duration));
}
}

float battleSequenceDuration(const BattleSequencePlan &plan)
{
	const float playerAttackDuration = plan.playerAttackEnabled
	                                       ? PLAYER_WINDUP_DURATION +
	                                             PLAYER_PROJECTILE_DURATION +
	                                             TARGET_IMPACT_DURATION
	                                       : 0.0f;
	const float counterDuration = plan.counterEnabled
	                                  ? WILD_WINDUP_DURATION +
	                                        WILD_PROJECTILE_DURATION +
	                                        PLAYER_IMPACT_DURATION
	                                  : 0.0f;
	return playerAttackDuration + counterDuration + RECOVERY_DURATION;
}

BattleSequenceSample sampleBattleSequence(const BattleSequencePlan &plan,
	                                        float elapsedSeconds)
{
	BattleSequenceSample sample;
	if (elapsedSeconds < 0.0f)
	{
		return sample;
	}
	if (elapsedSeconds >= battleSequenceDuration(plan))
	{
		sample.phase = BattlePhase::Finished;
		sample.phaseProgress = 1.0f;
		sample.finished = true;
		return sample;
	}

	float remaining = elapsedSeconds;
	if (plan.playerAttackEnabled)
	{
		if (remaining < PLAYER_WINDUP_DURATION)
		{
			sample.phase = BattlePhase::PlayerWindup;
			sample.phaseProgress = progressWithin(remaining, PLAYER_WINDUP_DURATION);
			return sample;
		}
		remaining -= PLAYER_WINDUP_DURATION;

		if (remaining < PLAYER_PROJECTILE_DURATION)
		{
			sample.phase = BattlePhase::PlayerProjectile;
			sample.phaseProgress =
			    progressWithin(remaining, PLAYER_PROJECTILE_DURATION);
			sample.showPlayerProjectile = true;
			return sample;
		}
		remaining -= PLAYER_PROJECTILE_DURATION;

		if (remaining < TARGET_IMPACT_DURATION)
		{
			sample.phase = BattlePhase::TargetImpact;
			sample.phaseProgress =
			    progressWithin(remaining, TARGET_IMPACT_DURATION);
			sample.targetImpact = true;
			return sample;
		}
		remaining -= TARGET_IMPACT_DURATION;
	}

	if (plan.counterEnabled)
	{
		if (remaining < WILD_WINDUP_DURATION)
		{
			sample.phase = BattlePhase::WildWindup;
			sample.phaseProgress = progressWithin(remaining, WILD_WINDUP_DURATION);
			return sample;
		}
		remaining -= WILD_WINDUP_DURATION;

		if (remaining < WILD_PROJECTILE_DURATION)
		{
			sample.phase = BattlePhase::WildProjectile;
			sample.phaseProgress = progressWithin(remaining, WILD_PROJECTILE_DURATION);
			sample.showWildProjectile = true;
			return sample;
		}
		remaining -= WILD_PROJECTILE_DURATION;

		if (remaining < PLAYER_IMPACT_DURATION)
		{
			sample.phase = BattlePhase::PlayerImpact;
			sample.phaseProgress = progressWithin(remaining, PLAYER_IMPACT_DURATION);
			sample.playerImpact = true;
			return sample;
		}
		remaining -= PLAYER_IMPACT_DURATION;
	}

	sample.phase = BattlePhase::Recovery;
	sample.phaseProgress = progressWithin(remaining, RECOVERY_DURATION);
	return sample;
}
