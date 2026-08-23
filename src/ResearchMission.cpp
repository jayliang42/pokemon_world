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

ResearchMissionSnapshot makeResearchMissionSnapshot(
	int caughtCount, int defeatedCount,
	const ResearchMissionProgress &progress, int captureGoal)
{
	const int safeCaptureGoal = std::max(1, captureGoal);
	ResearchMissionSnapshot snapshot;
	snapshot.objectives = {{
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
