#include "BattleMechanics.h"
#include "BattleSequence.h"
#include "CaptureMechanics.h"
#include "CaptureSequence.h"
#include "ResearchMission.h"
#include "ResearchProgression.h"
#include "ResearchRunState.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <string>

namespace
{
constexpr float FIXED_STEP_SECONDS = 1.0f / 60.0f;
constexpr float ENCOUNTER_TRAVEL_SECONDS = 65.0f;

int failures = 0;

void expectTrue(bool condition, const std::string &message)
{
	if (!condition)
	{
		std::cerr << "FAIL: " << message << std::endl;
		++failures;
	}
}

struct SurveyState
{
	int pokeballs = RESEARCH_STARTING_POKEBALLS;
	int caught = 0;
	int defeated = 0;
	int playerHealth = battleStatsFor(PokemonSpecies::Charizard).maximumHealth;
	int captureSequences = 0;
	int battleSequences = 0;
	float elapsedSeconds = 0.0f;
	ResearchMissionProgress mission;
};

bool captureSequenceExits(const CaptureResult &result)
{
	const float duration = captureSequenceDuration(result);
	const int steps = static_cast<int>(std::ceil(
		duration / FIXED_STEP_SECONDS)) + 2;
	bool sawThrow = false;
	bool sawAbsorb = false;
	bool sawResolution = false;
	bool finished = false;
	for (int step = 0; step <= steps; ++step)
	{
		const CaptureSequenceSample sample = sampleCaptureSequence(
			result, static_cast<float>(step) * FIXED_STEP_SECONDS);
		sawThrow = sawThrow || sample.phase == CapturePhase::Throwing;
		sawAbsorb = sawAbsorb || sample.phase == CapturePhase::Absorbing;
		sawResolution = sawResolution ||
		                sample.phase == CapturePhase::Succeeded ||
		                sample.phase == CapturePhase::BrokeFree;
		if (sample.finished)
		{
			finished = sample.phase == CapturePhase::Finished;
			break;
		}
	}
	return sawThrow && sawAbsorb && sawResolution && finished;
}

bool battleSequenceExits(const BattleSequencePlan &plan)
{
	const float duration = battleSequenceDuration(plan);
	const int steps = static_cast<int>(std::ceil(
		duration / FIXED_STEP_SECONDS)) + 2;
	bool sawPlayerActive = !plan.playerAttackEnabled;
	bool sawTargetImpact = !plan.playerAttackEnabled;
	bool sawWildActive = !plan.counterEnabled;
	bool sawPlayerImpact = !plan.counterEnabled;
	bool finished = false;
	for (int step = 0; step <= steps; ++step)
	{
		const BattleSequenceSample sample = sampleBattleSequence(
			plan, static_cast<float>(step) * FIXED_STEP_SECONDS);
		sawPlayerActive = sawPlayerActive ||
		                  sample.phase == BattlePhase::PlayerProjectile;
		sawTargetImpact = sawTargetImpact ||
		                  sample.phase == BattlePhase::TargetImpact;
		sawWildActive = sawWildActive ||
		                sample.phase == BattlePhase::WildProjectile;
		sawPlayerImpact = sawPlayerImpact ||
		                  sample.phase == BattlePhase::PlayerImpact;
		if (sample.finished)
		{
			finished = sample.phase == BattlePhase::Finished;
			break;
		}
	}
	return sawPlayerActive && sawTargetImpact && sawWildActive &&
	       sawPlayerImpact && finished;
}

CaptureAttempt fieldCaptureAttempt(
	PokemonSpecies species, float healthRatio, bool backHit, bool lured)
{
	CaptureAttempt attempt;
	attempt.species = species;
	attempt.distance = 2.2f;
	attempt.maximumDistance = 5.0f;
	attempt.alignment = 0.96f;
	attempt.healthRatio = healthRatio;
	attempt.alertness = backHit ? 0.04f : 0.20f;
	attempt.backHit = backHit;
	attempt.lured = lured;
	attempt.activity = backHit ? CaptureActivity::Idle
	                           : CaptureActivity::Moving;
	return attempt;
}

bool attemptCapture(
	SurveyState &state, const CaptureAttempt &attempt, float randomRoll)
{
	if (state.pokeballs <= 0)
	{
		return false;
	}
	--state.pokeballs;
	const CaptureResult result = resolveCaptureAttempt(attempt, randomRoll);
	expectTrue(captureSequenceExits(result),
	           "every capture encounter reaches its Finished phase");
	++state.captureSequences;
	state.elapsedSeconds += captureSequenceDuration(result);
	if (!result.captured)
	{
		return false;
	}
	++state.caught;
	if (attempt.species == PokemonSpecies::Eevee &&
	    attempt.healthRatio >= 0.999f)
	{
		recordHealthyEeveeCapture(state.mission);
	}
	return true;
}

bool captureTargetUntilResolved(
	SurveyState &state, CaptureRandom &random,
	const CaptureAttempt &attempt)
{
	while (state.pokeballs > 0)
	{
		if (attemptCapture(state, attempt, random.nextUnit()))
		{
			return true;
		}
	}
	return false;
}

int playBattleExchange(
	SurveyState &state, PokemonSpecies targetSpecies, int targetHealth,
	const BattleMove &playerMove)
{
	const BattleDamageResult playerDamage = resolveBattleDamage(
		PokemonSpecies::Charizard, targetSpecies, playerMove);
	const int remainingTargetHealth = std::max(
		0, targetHealth - playerDamage.amount);
	const BattleMove counterMove = wildBattleMoveFor(targetSpecies);
	BattleSequencePlan plan;
	plan.playerAttackEnabled = true;
	plan.counterEnabled = remainingTargetHealth > 0;
	plan.playerAttackHit = true;
	plan.playerTiming = playerMove.timing;
	plan.counterTiming = counterMove.timing;
	expectTrue(battleSequenceExits(plan),
	           "every player-initiated battle exchange reaches Finished");
	++state.battleSequences;
	state.elapsedSeconds += battleSequenceDuration(plan);
	if (playerDamage.effectiveness > 1.01f)
	{
		recordSuperEffectiveHit(state.mission);
	}
	if (remainingTargetHealth <= 0)
	{
		++state.defeated;
		return 0;
	}
	const BattleDamageResult counterDamage = resolveBattleDamage(
		targetSpecies, PokemonSpecies::Charizard, counterMove);
	state.playerHealth = resolvePlayerHit(
		state.playerHealth, counterDamage.amount, false).remainingHealth;
	return remainingTargetHealth;
}

void playWildAttackUntilFainted(SurveyState &state, PokemonSpecies species)
{
	const BattleMove wildMove = wildBattleMoveFor(species);
	const BattleDamageResult damage = resolveBattleDamage(
		species, PokemonSpecies::Charizard, wildMove);
	while (state.playerHealth > 0)
	{
		BattleSequencePlan plan;
		plan.playerAttackEnabled = false;
		plan.counterEnabled = true;
		plan.counterTiming = wildMove.timing;
		expectTrue(battleSequenceExits(plan),
		           "every wild-initiated attack reaches Finished");
		++state.battleSequences;
		state.elapsedSeconds += battleSequenceDuration(plan);
		state.playerHealth = resolvePlayerHit(
			state.playerHealth, damage.amount, false).remainingHealth;
	}
}

CampSettlementSummary settleAtCamp(const SurveyState &state)
{
	const ResearchMissionSnapshot mission = makeResearchMissionSnapshot(
		state.caught, state.defeated, state.mission, RESEARCH_CAPTURE_GOAL);
	CampSettlementInput input;
	input.atCamp = true;
	input.caughtCount = state.caught;
	input.captureGoal = RESEARCH_CAPTURE_GOAL;
	input.defeatedCount = state.defeated;
	input.completedObjectives = mission.completedObjectives();
	input.playerMaximumHealth =
		battleStatsFor(PokemonSpecies::Charizard).maximumHealth;
	input.startingPokeballs = RESEARCH_STARTING_POKEBALLS;
	return makeCampSettlement(input);
}

void testStealthRoutesCompleteWithinTenMinutesAndUnlockObserver()
{
	const std::array<std::uint32_t, 4> seeds = {
		0x1234u, 0xBEEFu, 0xCAFEu, 0xD00Du};
	const std::array<PokemonSpecies, RESEARCH_CAPTURE_GOAL> targets = {
		PokemonSpecies::Eevee, PokemonSpecies::Bulbasaur,
		PokemonSpecies::Umbreon, PokemonSpecies::Eevee,
		PokemonSpecies::Bulbasaur};
	for (const std::uint32_t seed : seeds)
	{
		SurveyState state;
		CaptureRandom random(seed);
		recordSafeLanding(state.mission);
		recordBulbasaurFleeObservation(state.mission);
		recordUmbreonWarningObservation(state.mission);
		for (const PokemonSpecies species : targets)
		{
			state.elapsedSeconds += ENCOUNTER_TRAVEL_SECONDS;
			const bool captured = captureTargetUntilResolved(
				state, random, fieldCaptureAttempt(species, 1.0f, true, false));
			expectTrue(captured,
			           "stealth route captures each target before supplies end");
			if (!captured)
			{
				break;
			}
		}
		state.elapsedSeconds += 45.0f;
		expectTrue(evaluateResearchRunOutcome(
		               state.caught, RESEARCH_CAPTURE_GOAL, state.pokeballs,
		               state.playerHealth) == ResearchRunOutcome::ResearchComplete,
		           "stealth route reaches the completed-return state");
		expectTrue(state.battleSequences == 0 && state.pokeballs > 0,
		           "stealth route completes without battle and keeps supply margin");
		expectTrue(state.elapsedSeconds < 600.0f,
		           "stealth route completes within the Phase 1 ten-minute limit");
		const CampSettlementSummary settlement = settleAtCamp(state);
		const ResearchProgressionResult progression =
			evaluateResearchProgression(RESEARCH_LEVEL_TRAINEE,
			                            settlement.researchScore);
		expectTrue(settlement.eligible &&
		               settlement.replenishedPokeballs ==
		                   RESEARCH_STARTING_POKEBALLS,
		           "completed stealth research can be submitted and restocked");
		expectTrue(progression.level == RESEARCH_LEVEL_OBSERVER &&
		               progression.lureCapacity == OBSERVER_LURE_CAPACITY,
		           "the complete non-combat route unlocks Observer and lures");
		std::cout << "Stealth route seed " << seed << ": "
		          << state.captureSequences << " throws, " << state.pokeballs
		          << " balls remaining, " << state.elapsedSeconds
		          << "s, score " << settlement.researchScore << std::endl;
	}
}

void testBattleWeakenRouteCompletesWithHealthAndSupplyMargin()
{
	SurveyState state;
	CaptureRandom random(0xF17E5u);
	const BattleMove ember = playerBattleMoves()[0];
	const int maximumTargetHealth =
		battleStatsFor(PokemonSpecies::Bulbasaur).maximumHealth;
	for (int target = 0; target < RESEARCH_CAPTURE_GOAL; ++target)
	{
		state.elapsedSeconds += ENCOUNTER_TRAVEL_SECONDS;
		const int remainingHealth = playBattleExchange(
			state, PokemonSpecies::Bulbasaur, maximumTargetHealth, ember);
		expectTrue(remainingHealth > 0,
		           "one Ember weakens Bulbasaur without defeating it");
		const float healthRatio = static_cast<float>(remainingHealth) /
		                          static_cast<float>(maximumTargetHealth);
		const bool captured = captureTargetUntilResolved(
			state, random,
			fieldCaptureAttempt(
				PokemonSpecies::Bulbasaur, healthRatio, false, false));
		expectTrue(captured,
		           "battle-weaken route captures the surviving target");
		if (!captured)
		{
			break;
		}
	}
	state.elapsedSeconds += 45.0f;
	expectTrue(evaluateResearchRunOutcome(
	               state.caught, RESEARCH_CAPTURE_GOAL, state.pokeballs,
	               state.playerHealth) == ResearchRunOutcome::ResearchComplete,
	           "battle-weaken route reaches research completion");
	expectTrue(state.battleSequences == RESEARCH_CAPTURE_GOAL &&
	               state.playerHealth > 0 && state.pokeballs > 0,
	           "battle-weaken route keeps health and Poke Ball margin");
	expectTrue(state.elapsedSeconds < 600.0f && settleAtCamp(state).eligible,
	           "mixed route finishes and submits inside ten minutes");
}

void testFaintRecoveryCanResumeAndCompleteResearch()
{
	SurveyState state;
	playWildAttackUntilFainted(state, PokemonSpecies::Umbreon);
	const ResearchRunOutcome fainted = evaluateResearchRunOutcome(
		state.caught, RESEARCH_CAPTURE_GOAL, state.pokeballs,
		state.playerHealth);
	expectTrue(fainted == ResearchRunOutcome::PlayerFainted &&
	               canRecoverAtCamp(fainted, state.pokeballs),
	           "a faint with supplies exits into the recoverable camp state");
	state.playerHealth = campRecoveryHealth(
		battleStatsFor(PokemonSpecies::Charizard).maximumHealth);
	expectTrue(evaluateResearchRunOutcome(
	               state.caught, RESEARCH_CAPTURE_GOAL, state.pokeballs,
	               state.playerHealth) == ResearchRunOutcome::Active,
	           "camp recovery returns the unfinished survey to Active");

	const CaptureAttempt attempt = fieldCaptureAttempt(
		PokemonSpecies::Eevee, 1.0f, true, false);
	for (int target = 0; target < RESEARCH_CAPTURE_GOAL; ++target)
	{
		expectTrue(attemptCapture(state, attempt, 0.0f),
		           "post-recovery capture encounter succeeds and exits");
	}
	expectTrue(evaluateResearchRunOutcome(
	               state.caught, RESEARCH_CAPTURE_GOAL, state.pokeballs,
	               state.playerHealth) == ResearchRunOutcome::ResearchComplete &&
	               settleAtCamp(state).eligible,
	           "the recovered survey can still complete and submit");
}

void testInventoryExhaustionHasAnExplicitTerminalState()
{
	SurveyState state;
	CaptureAttempt attempt = fieldCaptureAttempt(
		PokemonSpecies::Charizard, 1.0f, false, false);
	attempt.distance = attempt.maximumDistance;
	attempt.alignment = 0.0f;
	attempt.alertness = 1.0f;
	attempt.activity = CaptureActivity::Fleeing;
	while (state.pokeballs > 0)
	{
		expectTrue(!attemptCapture(state, attempt, 1.0f),
		           "worst-case throw fails without stalling its sequence");
	}
	const ResearchRunOutcome outcome = evaluateResearchRunOutcome(
		state.caught, RESEARCH_CAPTURE_GOAL, state.pokeballs,
		state.playerHealth);
	expectTrue(outcome == ResearchRunOutcome::OutOfPokeBalls &&
	               !canRecoverAtCamp(outcome, state.pokeballs),
	           "empty inventory exits through the explicit non-recoverable state");
}
}

int main()
{
	testStealthRoutesCompleteWithinTenMinutesAndUnlockObserver();
	testBattleWeakenRouteCompletesWithHealthAndSupplyMargin();
	testFaintRecoveryCanResumeAndCompleteResearch();
	testInventoryExhaustionHasAnExplicitTerminalState();
	if (failures != 0)
	{
		return 1;
	}
	std::cout << "Research run simulation tests passed" << std::endl;
	return 0;
}
