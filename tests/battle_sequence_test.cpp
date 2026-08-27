#include "BattleSequence.h"

#include <cmath>
#include <iostream>
#include <string>

namespace
{
int failures = 0;

BattleSequencePlan testPlan()
{
	BattleSequencePlan plan;
	plan.playerTiming = {0.20f, 0.30f, 0.40f, 0.10f, 0.25f};
	plan.counterTiming = {0.50f, 0.25f, 0.20f, 0.15f, 1.0f};
	return plan;
}

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

void testPlayerAttackPhasesExposeVisualEvents()
{
	const BattleSequencePlan plan = testPlan();
	const BattleSequenceSample inactive = sampleBattleSequence(plan, -0.1f);
	const BattleSequenceSample windup = sampleBattleSequence(plan, 0.10f);
	const BattleSequenceSample projectile = sampleBattleSequence(plan, 0.30f);
	const BattleSequenceSample impact = sampleBattleSequence(plan, 0.60f);
	const BattleSequenceSample recovery = sampleBattleSequence(plan, 0.90f);
	expectTrue(inactive.phase == BattlePhase::Inactive && !inactive.finished,
	           "negative elapsed time keeps battle sequence inactive");
	expectTrue(windup.phase == BattlePhase::PlayerWindup,
	           "battle starts with a player attack windup");
	expectTrue(projectile.phase == BattlePhase::PlayerProjectile &&
	               projectile.showPlayerProjectile,
	           "projectile phase requests the player attack visual");
	expectTrue(impact.phase == BattlePhase::TargetImpact && impact.targetImpact,
	           "target impact phase exposes hit feedback");
	expectTrue(recovery.phase == BattlePhase::PlayerRecovery,
	           "player move recovery is a distinct phase before a counterattack");
}

void testSurvivingTargetCountersBeforeRecovery()
{
	const BattleSequencePlan plan = testPlan();
	const BattleSequenceSample windup = sampleBattleSequence(plan, 1.30f);
	const BattleSequenceSample projectile = sampleBattleSequence(plan, 1.80f);
	const BattleSequenceSample impact = sampleBattleSequence(plan, 2.00f);
	expectTrue(windup.phase == BattlePhase::WildWindup,
	           "surviving target prepares a counterattack");
	expectTrue(projectile.phase == BattlePhase::WildProjectile &&
	               projectile.showWildProjectile &&
	               projectile.lockPlayerImpactPosition,
	           "a counterattack releases visually and retargets the player");
	expectTrue(impact.phase == BattlePhase::PlayerImpact && impact.playerImpact,
	           "counterattack reaches a player impact phase");
}

void testFaintedTargetSkipsCounterattack()
{
	BattleSequencePlan plan = testPlan();
	plan.counterEnabled = false;
	const BattleSequenceSample recovery = sampleBattleSequence(plan, 0.90f);
	expectTrue(recovery.phase == BattlePhase::PlayerRecovery,
	           "a fainted target still preserves player move recovery");
	expectTrue(battleSequenceDuration(plan) <
	               battleSequenceDuration(testPlan()),
	           "skipping a counterattack shortens the sequence");
}

void testWildInitiatedAttackSkipsPlayerPhases()
{
	BattleSequencePlan plan = testPlan();
	plan.playerAttackEnabled = false;
	const BattleSequenceSample windup = sampleBattleSequence(plan, 0.0f);
	const BattleSequenceSample projectile = sampleBattleSequence(plan, 0.60f);
	const BattleSequenceSample impact = sampleBattleSequence(plan, 0.90f);
	expectTrue(windup.phase == BattlePhase::WildWindup,
	           "a wild-initiated battle starts with the wild attack windup");
	expectTrue(projectile.phase == BattlePhase::WildProjectile &&
	               projectile.showWildProjectile &&
	               !projectile.showPlayerProjectile &&
	               !projectile.lockPlayerImpactPosition,
	           "a telegraphed wild opener keeps its original impact point");
	expectTrue(impact.phase == BattlePhase::PlayerImpact && impact.playerImpact &&
	               !impact.targetImpact,
	           "a wild-initiated battle reaches the player without a target hit");
	expectTrue(battleSequenceDuration(plan) <
	               battleSequenceDuration(testPlan()),
	           "skipping the player attack shortens the sequence");
}

void testTimingAndMovementLockComeFromMoveProfiles()
{
	BattleSequencePlan plan = testPlan();
	const float hitDuration = battleSequenceDuration(plan);
	plan.playerAttackHit = false;
	const float missDuration = battleSequenceDuration(plan);
	expectNear(hitDuration - missDuration, plan.playerTiming.staggerSeconds,
	           0.0001f,
	           "a successful player hit adds the configured target stagger");

	plan.playerAttackHit = true;
	expectNear(battleMovementScale(
	               plan, sampleBattleSequence(plan, 0.10f)),
	           0.75f, 0.0001f,
	           "movement lock reduces mobility during player startup");
	expectNear(battleMovementScale(
	               plan, sampleBattleSequence(plan, 0.90f)),
	           0.75f, 0.0001f,
	           "movement lock remains active through player recovery");
	expectNear(battleMovementScale(
	               plan, sampleBattleSequence(plan, 1.30f)),
	           1.0f, 0.0001f,
	           "movement returns for the enemy telegraph and dodge response");
}

void testExactDurationFinishesSequence()
{
	const BattleSequencePlan plan = testPlan();
	const float duration = battleSequenceDuration(plan);
	const BattleSequenceSample finished = sampleBattleSequence(plan, duration);
	expectTrue(finished.phase == BattlePhase::Finished && finished.finished,
	           "sampling at the exact duration finishes the sequence");
	expectNear(finished.phaseProgress, 1.0f, 0.0f,
	           "finished sequence reports complete progress");
}
}

int main()
{
	testPlayerAttackPhasesExposeVisualEvents();
	testSurvivingTargetCountersBeforeRecovery();
	testFaintedTargetSkipsCounterattack();
	testWildInitiatedAttackSkipsPlayerPhases();
	testTimingAndMovementLockComeFromMoveProfiles();
	testExactDurationFinishesSequence();

	if (failures != 0)
	{
		std::cerr << failures << " battle sequence test(s) failed" << std::endl;
		return 1;
	}

	std::cout << "All battle sequence tests passed" << std::endl;
	return 0;
}
