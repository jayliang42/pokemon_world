#include "BattleSequence.h"

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

void testPlayerAttackPhasesExposeVisualEvents()
{
	BattleSequencePlan plan;
	const BattleSequenceSample inactive = sampleBattleSequence(plan, -0.1f);
	const BattleSequenceSample windup = sampleBattleSequence(plan, 0.10f);
	const BattleSequenceSample projectile = sampleBattleSequence(plan, 0.40f);
	const BattleSequenceSample impact = sampleBattleSequence(plan, 0.75f);
	expectTrue(inactive.phase == BattlePhase::Inactive && !inactive.finished,
	           "negative elapsed time keeps battle sequence inactive");
	expectTrue(windup.phase == BattlePhase::PlayerWindup,
	           "battle starts with a player attack windup");
	expectTrue(projectile.phase == BattlePhase::PlayerProjectile &&
	               projectile.showPlayerProjectile,
	           "projectile phase requests the player attack visual");
	expectTrue(impact.phase == BattlePhase::TargetImpact && impact.targetImpact,
	           "target impact phase exposes hit feedback");
}

void testSurvivingTargetCountersBeforeRecovery()
{
	BattleSequencePlan plan;
	const BattleSequenceSample windup = sampleBattleSequence(plan, 1.20f);
	const BattleSequenceSample projectile = sampleBattleSequence(plan, 1.62f);
	const BattleSequenceSample impact = sampleBattleSequence(plan, 2.00f);
	expectTrue(windup.phase == BattlePhase::WildWindup,
	           "surviving target prepares a counterattack");
	expectTrue(projectile.phase == BattlePhase::WildProjectile &&
	               projectile.showWildProjectile,
	           "counterattack includes a visible travel phase");
	expectTrue(impact.phase == BattlePhase::PlayerImpact && impact.playerImpact,
	           "counterattack reaches a player impact phase");
}

void testFaintedTargetSkipsCounterattack()
{
	BattleSequencePlan plan;
	plan.counterEnabled = false;
	const BattleSequenceSample recovery = sampleBattleSequence(plan, 1.10f);
	expectTrue(recovery.phase == BattlePhase::Recovery,
	           "a fainted target skips directly to battle recovery");
	expectTrue(battleSequenceDuration(plan) <
	               battleSequenceDuration(BattleSequencePlan()),
	           "skipping a counterattack shortens the sequence");
}

void testWildInitiatedAttackSkipsPlayerPhases()
{
	BattleSequencePlan plan;
	plan.playerAttackEnabled = false;
	const BattleSequenceSample windup = sampleBattleSequence(plan, 0.0f);
	const BattleSequenceSample projectile = sampleBattleSequence(plan, 0.50f);
	const BattleSequenceSample impact = sampleBattleSequence(plan, 0.90f);
	expectTrue(windup.phase == BattlePhase::WildWindup,
	           "a wild-initiated battle starts with the wild attack windup");
	expectTrue(projectile.phase == BattlePhase::WildProjectile &&
	               projectile.showWildProjectile &&
	               !projectile.showPlayerProjectile,
	           "a wild-initiated battle only exposes the wild projectile");
	expectTrue(impact.phase == BattlePhase::PlayerImpact && impact.playerImpact &&
	               !impact.targetImpact,
	           "a wild-initiated battle reaches the player without a target hit");
	expectTrue(battleSequenceDuration(plan) <
	               battleSequenceDuration(BattleSequencePlan()),
	           "skipping the player attack shortens the sequence");
}

void testExactDurationFinishesSequence()
{
	BattleSequencePlan plan;
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
	testExactDurationFinishesSequence();

	if (failures != 0)
	{
		std::cerr << failures << " battle sequence test(s) failed" << std::endl;
		return 1;
	}

	std::cout << "All battle sequence tests passed" << std::endl;
	return 0;
}
