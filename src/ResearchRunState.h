#pragma once

enum class ResearchRunOutcome
{
	Active,
	ResearchComplete,
	PlayerFainted,
	OutOfPokeBalls,
};

ResearchRunOutcome evaluateResearchRunOutcome(
	int caughtCount, int captureGoal, int pokeballs, int playerHealth);
bool canRecoverAtCamp(ResearchRunOutcome outcome, int pokeballs);
int campRecoveryHealth(int maximumHealth);
