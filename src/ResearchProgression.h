#pragma once

constexpr int RESEARCH_LEVEL_TRAINEE = 0;
constexpr int RESEARCH_LEVEL_OBSERVER = 1;
constexpr int OBSERVER_UNLOCK_SCORE = 700;
constexpr int OBSERVER_LURE_CAPACITY = 2;

struct ResearchProgressionResult
{
	int level = RESEARCH_LEVEL_TRAINEE;
	int lureCapacity = 0;
	bool observerUnlocked = false;
};

ResearchProgressionResult evaluateResearchProgression(
	int currentLevel, int settlementScore);
bool researchLevelAllowsLures(int level);
int lureCapacityForResearchLevel(int level);
const char *researchLevelName(int level);
