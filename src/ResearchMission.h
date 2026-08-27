#pragma once

#include <array>
#include <cstddef>
#include <string>

constexpr std::size_t RESEARCH_OBJECTIVE_COUNT = 9;

enum class ResearchObjectiveId
{
	HealthyEeveeCapture,
	BulbasaurFleeObservation,
	UmbreonWarningObservation,
	MoonshadowTrackSurvey,
	RedrockLookoutSurvey,
	SuperEffectiveHit,
	DefeatWildPokemon,
	SafeLanding,
	CaptureSamples,
};

struct ResearchMissionProgress
{
	int healthyEeveeCaptures = 0;
	int bulbasaurFleeObservations = 0;
	int umbreonWarningObservations = 0;
	int moonshadowTrackSurveys = 0;
	int redrockLookoutSurveys = 0;
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
	std::array<ResearchObjectiveProgress, RESEARCH_OBJECTIVE_COUNT> objectives;

	int completedObjectives() const;
	bool primaryObjectiveComplete() const;
};

void recordSuperEffectiveHit(ResearchMissionProgress &progress);
void recordSafeLanding(ResearchMissionProgress &progress);
void recordHealthyEeveeCapture(ResearchMissionProgress &progress);
void recordBulbasaurFleeObservation(ResearchMissionProgress &progress);
void recordUmbreonWarningObservation(ResearchMissionProgress &progress);
void recordMoonshadowTrackSurvey(ResearchMissionProgress &progress);
void recordRedrockLookoutSurvey(ResearchMissionProgress &progress);
ResearchMissionSnapshot makeResearchMissionSnapshot(
	int caughtCount, int defeatedCount,
	const ResearchMissionProgress &progress, int captureGoal);
