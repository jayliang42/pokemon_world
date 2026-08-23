#include "ResearchMission.h"

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

void testFreshMissionHasFourReadableObjectives()
{
	const ResearchMissionSnapshot snapshot = makeResearchMissionSnapshot(
		0, 0, ResearchMissionProgress(), 5);
	expectTrue(snapshot.objectives.size() == 4,
	           "research mission exposes four field objectives");
	expectTrue(snapshot.completedObjectives() == 0,
	           "fresh research mission has no completed objectives");
	expectTrue(snapshot.objectives[0].id ==
	               ResearchObjectiveId::SuperEffectiveHit &&
	               snapshot.objectives[0].title ==
	                   "Land a super-effective hit",
	           "first objective teaches type effectiveness");
	expectTrue(snapshot.objectives[3].primary &&
	               snapshot.objectives[3].target == 5,
	           "capture goal remains the primary assignment");
}

void testMissionReflectsBattleLandingAndCaptureProgress()
{
	ResearchMissionProgress progress;
	progress.superEffectiveHits = 2;
	progress.safeLandings = 1;
	const ResearchMissionSnapshot snapshot =
		makeResearchMissionSnapshot(3, 1, progress, 5);
	expectTrue(snapshot.objectives[0].current == 1 &&
	               snapshot.objectives[0].complete(),
	           "super-effective objective completes and caps at its target");
	expectTrue(snapshot.objectives[1].complete(),
	           "defeating a wild Pokemon completes the battle objective");
	expectTrue(snapshot.objectives[2].complete(),
	           "a safe landing completes the flight objective");
	expectTrue(snapshot.objectives[3].current == 3 &&
	               !snapshot.primaryObjectiveComplete(),
	           "partial captures update without prematurely completing research");
	expectTrue(snapshot.completedObjectives() == 3,
	           "snapshot reports the total number of completed objectives");
}

void testMissionSanitizesInvalidAndOverflowingCounters()
{
	ResearchMissionProgress invalid;
	invalid.superEffectiveHits = -8;
	invalid.safeLandings = -2;
	const ResearchMissionSnapshot fresh =
		makeResearchMissionSnapshot(-5, -1, invalid, 0);
	for (const ResearchObjectiveProgress &objective : fresh.objectives)
	{
		expectTrue(objective.current >= 0 && objective.current <= objective.target,
		           "objective display progress remains inside its valid range");
	}
	expectTrue(fresh.objectives[3].target == 1,
	           "invalid capture goal falls back to a safe positive target");

	ResearchMissionProgress complete;
	complete.superEffectiveHits = 999;
	complete.safeLandings = 999;
	const ResearchMissionSnapshot finished =
		makeResearchMissionSnapshot(999, 999, complete, 5);
	expectTrue(finished.completedObjectives() == 4 &&
	               finished.primaryObjectiveComplete() &&
	               finished.objectives[3].current == 5,
	           "overflowing runtime counters render as fully complete, not oversized");
}

void testMissionEventsIncrementSafely()
{
	ResearchMissionProgress progress;
	recordSuperEffectiveHit(progress);
	recordSafeLanding(progress);
	expectTrue(progress.superEffectiveHits == 1 && progress.safeLandings == 1,
	           "runtime mission events increment their matching counters");
	progress.superEffectiveHits = 999;
	progress.safeLandings = -20;
	recordSuperEffectiveHit(progress);
	recordSafeLanding(progress);
	expectTrue(progress.superEffectiveHits == 999 && progress.safeLandings == 1,
	           "mission events saturate high values and repair negative values");
}
}

int main()
{
	testFreshMissionHasFourReadableObjectives();
	testMissionReflectsBattleLandingAndCaptureProgress();
	testMissionSanitizesInvalidAndOverflowingCounters();
	testMissionEventsIncrementSafely();
	if (failures != 0)
	{
		std::cerr << failures << " research mission checks failed" << std::endl;
		return 1;
	}
	std::cout << "Research mission checks passed" << std::endl;
	return 0;
}
