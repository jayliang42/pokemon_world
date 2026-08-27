#include "ResearchProgression.h"

#include <algorithm>

namespace
{
int supportedLevel(int level)
{
	return std::max(RESEARCH_LEVEL_TRAINEE,
	                std::min(RESEARCH_LEVEL_OBSERVER, level));
}
}

ResearchProgressionResult evaluateResearchProgression(
	int currentLevel, int settlementScore)
{
	ResearchProgressionResult result;
	result.level = supportedLevel(currentLevel);
	if (result.level == RESEARCH_LEVEL_TRAINEE &&
	    settlementScore >= OBSERVER_UNLOCK_SCORE)
	{
		result.level = RESEARCH_LEVEL_OBSERVER;
		result.observerUnlocked = true;
	}
	result.lureCapacity = lureCapacityForResearchLevel(result.level);
	return result;
}

bool researchLevelAllowsLures(int level)
{
	return supportedLevel(level) >= RESEARCH_LEVEL_OBSERVER;
}

int lureCapacityForResearchLevel(int level)
{
	return researchLevelAllowsLures(level) ? OBSERVER_LURE_CAPACITY : 0;
}

const char *researchLevelName(int level)
{
	return supportedLevel(level) >= RESEARCH_LEVEL_OBSERVER
	           ? "Observer"
	           : "Trainee";
}
