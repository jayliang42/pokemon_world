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

void testFreshMissionHasSpeciesAndFieldObjectives()
{
	const ResearchMissionSnapshot snapshot = makeResearchMissionSnapshot(
		0, 0, ResearchMissionProgress(), 5);
	expectTrue(snapshot.objectives.size() == RESEARCH_OBJECTIVE_COUNT &&
	               RESEARCH_OBJECTIVE_COUNT == 9,
	           "research mission exposes species, regional, and field objectives");
	expectTrue(snapshot.completedObjectives() == 0,
	           "fresh research mission has no completed objectives");
	expectTrue(snapshot.objectives[0].id ==
	               ResearchObjectiveId::HealthyEeveeCapture &&
	               snapshot.objectives[0].title ==
	                   "Catch an unhurt Eevee" &&
	               snapshot.objectives[1].id ==
	                   ResearchObjectiveId::BulbasaurFleeObservation &&
	               snapshot.objectives[2].id ==
	                   ResearchObjectiveId::UmbreonWarningObservation,
	           "the first three objectives teach distinct species strategies");
	expectTrue(snapshot.objectives[3].id ==
	               ResearchObjectiveId::MoonshadowTrackSurvey &&
	               snapshot.objectives[4].id ==
	                   ResearchObjectiveId::RedrockLookoutSurvey,
	           "the two route endpoints are represented as regional research tasks");
	expectTrue(snapshot.objectives[8].primary &&
	               snapshot.objectives[8].target == 5,
	           "capture goal remains the primary assignment");
}

void testMissionReflectsBattleLandingAndCaptureProgress()
{
	ResearchMissionProgress progress;
	progress.superEffectiveHits = 2;
	progress.safeLandings = 1;
	progress.healthyEeveeCaptures = 1;
	progress.bulbasaurFleeObservations = 1;
	progress.umbreonWarningObservations = 1;
	progress.moonshadowTrackSurveys = 1;
	progress.redrockLookoutSurveys = 1;
	const ResearchMissionSnapshot snapshot =
		makeResearchMissionSnapshot(3, 1, progress, 5);
	expectTrue(snapshot.objectives[5].current == 1 &&
	               snapshot.objectives[5].complete(),
	           "super-effective objective completes and caps at its target");
	expectTrue(snapshot.objectives[6].complete(),
	           "defeating a wild Pokemon completes the battle objective");
	expectTrue(snapshot.objectives[7].complete(),
	           "a safe landing completes the flight objective");
	expectTrue(snapshot.objectives[8].current == 3 &&
	               !snapshot.primaryObjectiveComplete(),
	           "partial captures update without prematurely completing research");
	expectTrue(snapshot.completedObjectives() == 8,
	           "snapshot reports the total number of completed objectives");
}

void testMissionSanitizesInvalidAndOverflowingCounters()
{
	ResearchMissionProgress invalid;
	invalid.superEffectiveHits = -8;
	invalid.safeLandings = -2;
	invalid.healthyEeveeCaptures = -3;
	invalid.bulbasaurFleeObservations = -4;
	invalid.umbreonWarningObservations = -5;
	invalid.moonshadowTrackSurveys = -6;
	invalid.redrockLookoutSurveys = -7;
	const ResearchMissionSnapshot fresh =
		makeResearchMissionSnapshot(-5, -1, invalid, 0);
	for (const ResearchObjectiveProgress &objective : fresh.objectives)
	{
		expectTrue(objective.current >= 0 && objective.current <= objective.target,
		           "objective display progress remains inside its valid range");
	}
	expectTrue(fresh.objectives[8].target == 1,
	           "invalid capture goal falls back to a safe positive target");

	ResearchMissionProgress complete;
	complete.superEffectiveHits = 999;
	complete.safeLandings = 999;
	complete.healthyEeveeCaptures = 999;
	complete.bulbasaurFleeObservations = 999;
	complete.umbreonWarningObservations = 999;
	complete.moonshadowTrackSurveys = 999;
	complete.redrockLookoutSurveys = 999;
	const ResearchMissionSnapshot finished =
		makeResearchMissionSnapshot(999, 999, complete, 5);
	expectTrue(finished.completedObjectives() == 9 &&
	               finished.primaryObjectiveComplete() &&
	               finished.objectives[8].current == 5,
	           "overflowing runtime counters render as fully complete, not oversized");
}

void testMissionEventsIncrementSafely()
{
	ResearchMissionProgress progress;
	recordSuperEffectiveHit(progress);
	recordSafeLanding(progress);
	recordHealthyEeveeCapture(progress);
	recordBulbasaurFleeObservation(progress);
	recordUmbreonWarningObservation(progress);
	recordMoonshadowTrackSurvey(progress);
	recordRedrockLookoutSurvey(progress);
	expectTrue(progress.superEffectiveHits == 1 && progress.safeLandings == 1 &&
	               progress.healthyEeveeCaptures == 1 &&
	               progress.bulbasaurFleeObservations == 1 &&
	               progress.umbreonWarningObservations == 1 &&
	               progress.moonshadowTrackSurveys == 1 &&
	               progress.redrockLookoutSurveys == 1,
	           "runtime mission events increment their matching counters");
	progress.superEffectiveHits = 999;
	progress.safeLandings = -20;
	recordSuperEffectiveHit(progress);
	recordSafeLanding(progress);
	progress.healthyEeveeCaptures = 999;
	progress.bulbasaurFleeObservations = -20;
	progress.umbreonWarningObservations = 999;
	progress.moonshadowTrackSurveys = -20;
	progress.redrockLookoutSurveys = 999;
	recordHealthyEeveeCapture(progress);
	recordBulbasaurFleeObservation(progress);
	recordUmbreonWarningObservation(progress);
	recordMoonshadowTrackSurvey(progress);
	recordRedrockLookoutSurvey(progress);
	expectTrue(progress.superEffectiveHits == 999 && progress.safeLandings == 1 &&
	               progress.healthyEeveeCaptures == 999 &&
	               progress.bulbasaurFleeObservations == 1 &&
	               progress.umbreonWarningObservations == 999 &&
	               progress.moonshadowTrackSurveys == 1 &&
	               progress.redrockLookoutSurveys == 1,
	           "mission events saturate high values and repair negative values");
}
}

int main()
{
	testFreshMissionHasSpeciesAndFieldObjectives();
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
