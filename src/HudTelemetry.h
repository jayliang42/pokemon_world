#pragma once

#include <array>
#include <cstddef>
#include <string>

#include "ResearchMission.h"

constexpr std::size_t HUD_MOVE_SLOT_COUNT = 3;
constexpr std::size_t HUD_MISSION_OBJECTIVE_COUNT = RESEARCH_OBJECTIVE_COUNT;

struct HudPlayerTelemetry
{
	int health = 0;
	int maximumHealth = 1;
};

struct HudTargetTelemetry
{
	bool visible = false;
	std::string name;
	int health = 0;
	int maximumHealth = 1;
	float alertness = 0.0f;
	bool backHitOpportunity = false;
};

struct HudDodgeTelemetry
{
	double remainingSeconds = 0.0;
	double counterWindowRemainingSeconds = 0.0;
	float cooldownFraction = 0.0f;
	bool dodging = false;
	bool invulnerable = false;
};

struct HudThreatTelemetry
{
	bool visible = false;
	std::string name;
	float distance = 0.0f;
	bool pursuing = false;
};

struct HudRadarTelemetry
{
	bool visible = false;
	std::string name;
	float distance = 0.0f;
	float bearingRadians = 0.0f;
};

struct HudCampTelemetry
{
	float distance = 0.0f;
	float bearingRadians = 0.0f;
	bool inside = false;
	bool grounded = false;
	bool readyToSubmit = false;
	bool submitted = false;
	int settlementScore = 0;
};

struct HudRegionalTelemetry
{
	bool visible = false;
	std::string name;
	std::string prompt;
	float distance = 0.0f;
	float bearingRadians = 0.0f;
	bool ready = false;
	bool recorded = false;
	bool alpha = false;
};

struct HudResearchTelemetry
{
	int level = 0;
	std::string levelName;
	int luresRemaining = 0;
	bool lureActive = false;
	float lureRemainingSeconds = 0.0f;
};

struct HudMoveTelemetry
{
	std::string name;
	std::string shape;
	int type = 0;
	int power = 1;
	float range = 1.0f;
	double remainingSeconds = 0.0;
	float cooldownFraction = 0.0f;
	bool selected = false;
};

struct HudMissionObjectiveTelemetry
{
	int current = 0;
	int target = 1;
};

struct HudTelemetry
{
	std::string summary;
	HudPlayerTelemetry player;
	HudTargetTelemetry target;
	HudDodgeTelemetry dodge;
	HudThreatTelemetry threat;
	HudRadarTelemetry radar;
	HudCampTelemetry camp;
	HudRegionalTelemetry regional;
	HudResearchTelemetry research;
	bool moveInputBusy = false;
	std::array<HudMoveTelemetry, HUD_MOVE_SLOT_COUNT> moves;
	std::array<HudMissionObjectiveTelemetry,
	           HUD_MISSION_OBJECTIVE_COUNT> missionObjectives;
	int completedMissionObjectives = 0;
};

bool validateHudTelemetry(const HudTelemetry &telemetry,
	                      std::string *error = nullptr);
std::string encodeHudTelemetryJson(const HudTelemetry &telemetry);
