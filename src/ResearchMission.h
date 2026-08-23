#pragma once

#include <array>
#include <string>

enum class ResearchObjectiveId
{
	SuperEffectiveHit,
	DefeatWildPokemon,
	SafeLanding,
	CaptureSamples,
};

struct ResearchMissionProgress
{
	int superEffectiveHits = 0;
	int safeLandings = 0;
};

struct ResearchObjectiveProgress
{
	ResearchObjectiveId id = ResearchObjectiveId::SuperEffectiveHit;
	std::string title;
	int current = 0;
	int target = 1;
	bool primary = false;

	bool complete() const;
};

struct ResearchMissionSnapshot
{
	std::array<ResearchObjectiveProgress, 4> objectives;

	int completedObjectives() const;
	bool primaryObjectiveComplete() const;
};

void recordSuperEffectiveHit(ResearchMissionProgress &progress);
void recordSafeLanding(ResearchMissionProgress &progress);
ResearchMissionSnapshot makeResearchMissionSnapshot(
	int caughtCount, int defeatedCount,
	const ResearchMissionProgress &progress, int captureGoal);
