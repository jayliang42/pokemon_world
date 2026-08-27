#include "ResearchMission.h"

#include <algorithm>

namespace
{
constexpr int MAX_RECORDED_EVENTS = 999;

int boundedProgress(int value, int target)
{
	return std::max(0, std::min(std::max(1, target), value));
}

void incrementBounded(int &value)
{
	value = std::min(MAX_RECORDED_EVENTS, std::max(0, value) + 1);
}

void recordOnce(int &value)
{
	value = 1;
}
}

bool ResearchObjectiveProgress::complete() const
{
	return current >= target;
}

int ResearchMissionSnapshot::completedObjectives() const
{
	int completed = 0;
	for (const ResearchObjectiveProgress &objective : objectives)
	{
		if (objective.complete())
		{
			++completed;
		}
	}
	return completed;
}

bool ResearchMissionSnapshot::primaryObjectiveComplete() const
{
	for (const ResearchObjectiveProgress &objective : objectives)
	{
		if (objective.primary)
		{
			return objective.complete();
		}
	}
	return false;
}

void recordSuperEffectiveHit(ResearchMissionProgress &progress)
{
	incrementBounded(progress.superEffectiveHits);
}

void recordSafeLanding(ResearchMissionProgress &progress)
{
	incrementBounded(progress.safeLandings);
}

void recordHealthyEeveeCapture(ResearchMissionProgress &progress)
{
	incrementBounded(progress.healthyEeveeCaptures);
}

void recordBulbasaurFleeObservation(ResearchMissionProgress &progress)
{
	incrementBounded(progress.bulbasaurFleeObservations);
}

void recordUmbreonWarningObservation(ResearchMissionProgress &progress)
{
	incrementBounded(progress.umbreonWarningObservations);
}

void recordMoonshadowTrackSurvey(ResearchMissionProgress &progress)
{
	recordOnce(progress.moonshadowTrackSurveys);
}

void recordRedrockLookoutSurvey(ResearchMissionProgress &progress)
{
	recordOnce(progress.redrockLookoutSurveys);
}

ResearchMissionSnapshot makeResearchMissionSnapshot(
	int caughtCount, int defeatedCount,
	const ResearchMissionProgress &progress, int captureGoal)
{
	const int safeCaptureGoal = std::max(1, captureGoal);
	ResearchMissionSnapshot snapshot;
	snapshot.objectives = {{
		{ResearchObjectiveId::HealthyEeveeCapture, "Catch an unhurt Eevee",
		 boundedProgress(progress.healthyEeveeCaptures, 1), 1, false},
		{ResearchObjectiveId::BulbasaurFleeObservation,
		 "Observe Bulbasaur flee",
		 boundedProgress(progress.bulbasaurFleeObservations, 1), 1, false},
		{ResearchObjectiveId::UmbreonWarningObservation,
		 "Observe Umbreon's warning",
		 boundedProgress(progress.umbreonWarningObservations, 1), 1, false},
		{ResearchObjectiveId::MoonshadowTrackSurvey,
		 "Record Moonshadow tracks at night",
		 boundedProgress(progress.moonshadowTrackSurveys, 1), 1, false},
		{ResearchObjectiveId::RedrockLookoutSurvey,
		 "Survey the Redrock lookout",
		 boundedProgress(progress.redrockLookoutSurveys, 1), 1, false},
		{ResearchObjectiveId::SuperEffectiveHit, "Land a super-effective hit",
		 boundedProgress(progress.superEffectiveHits, 1), 1, false},
		{ResearchObjectiveId::DefeatWildPokemon, "Defeat a wild Pokemon",
		 boundedProgress(defeatedCount, 1), 1, false},
		{ResearchObjectiveId::SafeLanding, "Complete a gravity-assisted landing",
		 boundedProgress(progress.safeLandings, 1), 1, false},
		{ResearchObjectiveId::CaptureSamples, "Catch research samples",
		 boundedProgress(caughtCount, safeCaptureGoal), safeCaptureGoal, true},
	}};
	return snapshot;
}
