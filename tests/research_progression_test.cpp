#include "ResearchProgression.h"

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

void testObserverUnlockRequiresAHighQualitySettlement()
{
	const ResearchProgressionResult below =
		evaluateResearchProgression(RESEARCH_LEVEL_TRAINEE, 699);
	expectTrue(below.level == RESEARCH_LEVEL_TRAINEE &&
	               !below.observerUnlocked && below.lureCapacity == 0,
	           "a low-breadth survey does not unlock Observer tools");

	const ResearchProgressionResult unlocked =
		evaluateResearchProgression(RESEARCH_LEVEL_TRAINEE, 700);
	expectTrue(unlocked.level == RESEARCH_LEVEL_OBSERVER &&
	               unlocked.observerUnlocked &&
	               unlocked.lureCapacity == OBSERVER_LURE_CAPACITY,
	           "a 700-point settlement unlocks Observer and two lures");
}

void testObserverProgressPersistsAcrossLaterSettlements()
{
	const ResearchProgressionResult retained =
		evaluateResearchProgression(RESEARCH_LEVEL_OBSERVER, 0);
	expectTrue(retained.level == RESEARCH_LEVEL_OBSERVER &&
	               !retained.observerUnlocked &&
	               retained.lureCapacity == OBSERVER_LURE_CAPACITY &&
	               researchLevelAllowsLures(retained.level) &&
	               std::string(researchLevelName(retained.level)) == "Observer",
	           "unlocked progression cannot be lost by a later weak survey");
}

void testInvalidProgressionInputsFailToSafeBounds()
{
	const ResearchProgressionResult invalid =
		evaluateResearchProgression(-8, -50);
	expectTrue(invalid.level == RESEARCH_LEVEL_TRAINEE &&
	               invalid.lureCapacity == 0 &&
	               !researchLevelAllowsLures(-1) &&
	               std::string(researchLevelName(99)) == "Observer",
	           "progression values clamp to the supported 0-to-1 range");
}
}

int main()
{
	testObserverUnlockRequiresAHighQualitySettlement();
	testObserverProgressPersistsAcrossLaterSettlements();
	testInvalidProgressionInputsFailToSafeBounds();
	if (failures != 0)
	{
		return 1;
	}
	std::cout << "Research progression tests passed" << std::endl;
	return 0;
}
