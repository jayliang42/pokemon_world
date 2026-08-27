#include "BattleSequence.h"

#include <algorithm>
#include <cmath>

namespace
{
constexpr float TARGET_IMPACT_FEEDBACK_DURATION = 0.18f;
constexpr float PLAYER_IMPACT_FEEDBACK_DURATION = 0.18f;

float safeDuration(float duration)
{
	return std::isfinite(duration) && duration > 0.0f ? duration : 0.0f;
}

float progressWithin(float elapsed, float duration)
{
	return duration > 0.0f
	           ? std::max(0.0f, std::min(1.0f, elapsed / duration))
	           : 1.0f;
}
}

float battleSequenceDuration(const BattleSequencePlan &plan)
{
	const float playerAttackDuration =
		plan.playerAttackEnabled
		    ? safeDuration(plan.playerTiming.startupSeconds) +
		          safeDuration(plan.playerTiming.activeSeconds) +
		          TARGET_IMPACT_FEEDBACK_DURATION +
		          (plan.playerAttackHit
		               ? safeDuration(plan.playerTiming.staggerSeconds)
		               : 0.0f) +
		          safeDuration(plan.playerTiming.recoverySeconds)
		    : 0.0f;
	const float counterDuration =
		plan.counterEnabled
		    ? safeDuration(plan.counterTiming.startupSeconds) +
		          safeDuration(plan.counterTiming.activeSeconds) +
		          PLAYER_IMPACT_FEEDBACK_DURATION +
		          safeDuration(plan.counterTiming.staggerSeconds) +
		          safeDuration(plan.counterTiming.recoverySeconds)
		    : 0.0f;
	return playerAttackDuration + counterDuration;
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
		const float startupDuration = safeDuration(
			plan.playerTiming.startupSeconds);
		if (remaining < startupDuration)
		{
			sample.phase = BattlePhase::PlayerWindup;
			sample.phaseProgress = progressWithin(remaining, startupDuration);
			return sample;
		}
		remaining -= startupDuration;

		const float activeDuration = safeDuration(plan.playerTiming.activeSeconds);
		if (remaining < activeDuration)
		{
			sample.phase = BattlePhase::PlayerProjectile;
			sample.phaseProgress = progressWithin(remaining, activeDuration);
			sample.showPlayerProjectile = true;
			return sample;
		}
		remaining -= activeDuration;

		const float targetImpactDuration =
			TARGET_IMPACT_FEEDBACK_DURATION +
			(plan.playerAttackHit
			     ? safeDuration(plan.playerTiming.staggerSeconds)
			     : 0.0f);
		if (remaining < targetImpactDuration)
		{
			sample.phase = BattlePhase::TargetImpact;
			sample.phaseProgress =
			    progressWithin(remaining, targetImpactDuration);
			sample.targetImpact = true;
			return sample;
		}
		remaining -= targetImpactDuration;

		const float recoveryDuration = safeDuration(
			plan.playerTiming.recoverySeconds);
		if (remaining < recoveryDuration)
		{
			sample.phase = BattlePhase::PlayerRecovery;
			sample.phaseProgress = progressWithin(remaining, recoveryDuration);
			return sample;
		}
		remaining -= recoveryDuration;
	}

	if (plan.counterEnabled)
	{
		const float startupDuration = safeDuration(
			plan.counterTiming.startupSeconds);
		if (remaining < startupDuration)
		{
			sample.phase = BattlePhase::WildWindup;
			sample.phaseProgress = progressWithin(remaining, startupDuration);
			return sample;
		}
		remaining -= startupDuration;

		const float activeDuration = safeDuration(plan.counterTiming.activeSeconds);
		if (remaining < activeDuration)
		{
			sample.phase = BattlePhase::WildProjectile;
			sample.phaseProgress = progressWithin(remaining, activeDuration);
			sample.showWildProjectile = true;
			sample.lockPlayerImpactPosition = plan.playerAttackEnabled;
			return sample;
		}
		remaining -= activeDuration;

		const float playerImpactDuration =
			PLAYER_IMPACT_FEEDBACK_DURATION +
			safeDuration(plan.counterTiming.staggerSeconds);
		if (remaining < playerImpactDuration)
		{
			sample.phase = BattlePhase::PlayerImpact;
			sample.phaseProgress = progressWithin(remaining, playerImpactDuration);
			sample.playerImpact = true;
			return sample;
		}
		remaining -= playerImpactDuration;

		const float recoveryDuration = safeDuration(
			plan.counterTiming.recoverySeconds);
		if (remaining < recoveryDuration)
		{
			sample.phase = BattlePhase::Recovery;
			sample.phaseProgress = progressWithin(remaining, recoveryDuration);
			return sample;
		}
	}

	sample.phase = BattlePhase::Finished;
	sample.phaseProgress = 1.0f;
	sample.finished = true;
	return sample;
}

float battleMovementScale(const BattleSequencePlan &plan,
	                      const BattleSequenceSample &sample)
{
	const bool playerCommitted =
		plan.playerAttackEnabled &&
		(sample.phase == BattlePhase::PlayerWindup ||
		 sample.phase == BattlePhase::PlayerProjectile ||
		 sample.phase == BattlePhase::TargetImpact ||
		 sample.phase == BattlePhase::PlayerRecovery);
	if (!playerCommitted)
	{
		return 1.0f;
	}
	const float movementLock = std::isfinite(plan.playerTiming.movementLock)
	                               ? plan.playerTiming.movementLock
	                               : 1.0f;
	return 1.0f - std::max(0.0f, std::min(1.0f, movementLock));
}
