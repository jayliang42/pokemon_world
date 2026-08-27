#include "HudTelemetry.h"

#include <iostream>
#include <limits>
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

HudTelemetry validTelemetry()
{
	HudTelemetry telemetry;
	telemetry.summary = "Eevee \"Scout\"\nNearby";
	telemetry.player.health = 92;
	telemetry.player.maximumHealth = 118;
	telemetry.target.visible = true;
	telemetry.target.name = "Eevee";
	telemetry.target.health = 25;
	telemetry.target.maximumHealth = 82;
	telemetry.target.alertness = 0.42f;
	telemetry.target.backHitOpportunity = true;
	telemetry.dodge.remainingSeconds = 0.75;
	telemetry.dodge.cooldownFraction = 0.25f;
	telemetry.dodge.dodging = true;
	telemetry.dodge.counterWindowRemainingSeconds = 0.9;
	telemetry.threat.visible = true;
	telemetry.threat.name = "Umbreon";
	telemetry.threat.distance = 4.5f;
	telemetry.threat.pursuing = true;
	telemetry.radar.visible = true;
	telemetry.radar.name = "Bulbasaur";
	telemetry.radar.distance = 12.25f;
	telemetry.radar.bearingRadians = -0.5f;
	telemetry.camp.distance = 18.5f;
	telemetry.camp.bearingRadians = 0.75f;
	telemetry.camp.readyToSubmit = true;
	telemetry.regional.visible = true;
	telemetry.regional.name = "Moonshadow Tracks";
	telemetry.regional.prompt = "F · RECORD TRACKS";
	telemetry.regional.distance = 1.6f;
	telemetry.regional.bearingRadians = -0.2f;
	telemetry.regional.ready = true;
	telemetry.regional.alpha = true;
	telemetry.research.level = 1;
	telemetry.research.levelName = "Observer";
	telemetry.research.luresRemaining = 1;
	telemetry.research.lureActive = true;
	telemetry.research.lureRemainingSeconds = 8.5f;
	telemetry.moveInputBusy = false;
	for (std::size_t index = 0; index < telemetry.moves.size(); ++index)
	{
		telemetry.moves[index].name = "Move " + std::to_string(index + 1);
		telemetry.moves[index].shape = index == 2 ? "Cone" : "Narrow";
		telemetry.moves[index].type = 1;
		telemetry.moves[index].power = 20 + static_cast<int>(index);
		telemetry.moves[index].range = 14.0f + static_cast<float>(index);
		telemetry.moves[index].remainingSeconds = 0.0;
		telemetry.moves[index].cooldownFraction = 0.0f;
		telemetry.moves[index].selected = index == 1;
	}
	for (std::size_t index = 0; index < telemetry.missionObjectives.size(); ++index)
	{
		telemetry.missionObjectives[index].current = index < 2 ? 1 : 0;
		telemetry.missionObjectives[index].target = 1;
	}
	telemetry.completedMissionObjectives = 2;
	return telemetry;
}

void testValidSnapshotEncodesStableJsonContract()
{
	const HudTelemetry telemetry = validTelemetry();
	std::string error;
	expectTrue(validateHudTelemetry(telemetry, &error),
	           "a complete telemetry snapshot satisfies the contract");
	const std::string encoded = encodeHudTelemetryJson(telemetry);
	expectTrue(!encoded.empty() && encoded.front() == '{' && encoded.back() == '}',
	           "valid telemetry encodes as one JSON object");
	expectTrue(encoded.find("Eevee \\\"Scout\\\"\\nNearby") != std::string::npos,
	           "JSON encoding escapes quotes and control characters");
	expectTrue(encoded.find("\"player\":{\"health\":92,\"maximum\":118}") !=
	               std::string::npos,
	           "player health has stable JSON field names");
	expectTrue(encoded.find("\"counterRemaining\":0.9") != std::string::npos,
	           "perfect-counter time is published in the dodge HUD contract");
	expectTrue(encoded.find("\"alertness\":0.41999999,\"backHitOpportunity\":true") !=
	               std::string::npos,
	           "target awareness and back-hit window share the HUD contract");
	expectTrue(encoded.find("\"moves\":{\"busy\":false,\"slots\":[") !=
	               std::string::npos,
	           "move slots are grouped under one stable contract");
	expectTrue(encoded.find("\"shape\":\"Cone\"") != std::string::npos &&
	               encoded.find("\"range\":16") != std::string::npos,
	           "move shape and range are published for tactical HUDs");
	expectTrue(encoded.find("\"mission\":{\"completed\":2,\"objectives\":[") !=
	               std::string::npos,
	           "mission progress is grouped in the same snapshot");
	expectTrue(encoded.find("\"camp\":{\"distance\":18.5,\"bearing\":0.75") !=
	               std::string::npos &&
	               encoded.find("\"readyToSubmit\":true") != std::string::npos,
	           "camp direction and submission state share the HUD snapshot");
	expectTrue(encoded.find(
	               "\"regional\":{\"visible\":true,\"name\":"
	               "\"Moonshadow Tracks\",\"prompt\":\"F · RECORD TRACKS\","
	               "\"distance\":1.6,\"bearing\":-0.2,\"ready\":true,"
	               "\"recorded\":false,\"alpha\":true}") != std::string::npos,
	           "nearby regional research publishes its interaction prompt");
	expectTrue(encoded.find(
	               "\"research\":{\"level\":1,\"name\":\"Observer\","
	               "\"luresRemaining\":1,\"lureActive\":true,"
	               "\"lureRemaining\":8.5}") != std::string::npos,
	           "research rank and lure state share the HUD snapshot");
}

void testInvalidSnapshotsFailClosed()
{
	HudTelemetry invalidHealth = validTelemetry();
	invalidHealth.player.health = invalidHealth.player.maximumHealth + 1;
	expectTrue(!validateHudTelemetry(invalidHealth) &&
	               encodeHudTelemetryJson(invalidHealth).empty(),
	           "out-of-range player health is not published");

	HudTelemetry invalidFraction = validTelemetry();
	invalidFraction.moves[0].cooldownFraction = 1.5f;
	expectTrue(!validateHudTelemetry(invalidFraction),
	           "cooldown fractions must stay normalized");

	HudTelemetry invalidCounterWindow = validTelemetry();
	invalidCounterWindow.dodge.counterWindowRemainingSeconds =
		std::numeric_limits<double>::quiet_NaN();
	expectTrue(!validateHudTelemetry(invalidCounterWindow),
	           "counter-window time must be finite and non-negative");

	HudTelemetry invalidAlertness = validTelemetry();
	invalidAlertness.target.alertness = -0.01f;
	expectTrue(!validateHudTelemetry(invalidAlertness),
	           "target alertness must stay normalized");

	HudTelemetry invalidDistance = validTelemetry();
	invalidDistance.radar.distance =
		std::numeric_limits<float>::quiet_NaN();
	expectTrue(!validateHudTelemetry(invalidDistance),
	           "non-finite spatial telemetry is rejected");

	HudTelemetry invalidCamp = validTelemetry();
	invalidCamp.camp.settlementScore = -1;
	expectTrue(!validateHudTelemetry(invalidCamp),
	           "camp settlement telemetry rejects negative scores");

	HudTelemetry invalidRegional = validTelemetry();
	invalidRegional.regional.prompt.clear();
	expectTrue(!validateHudTelemetry(invalidRegional),
	           "a visible regional site requires a usable prompt");
	invalidRegional = validTelemetry();
	invalidRegional.regional.recorded = true;
	expectTrue(!validateHudTelemetry(invalidRegional),
	           "a recorded regional site cannot also claim to be ready");

	HudTelemetry invalidResearchRank = validTelemetry();
	invalidResearchRank.research.level = 2;
	expectTrue(!validateHudTelemetry(invalidResearchRank),
	           "unsupported research ranks are rejected");

	HudTelemetry invalidLureInventory = validTelemetry();
	invalidLureInventory.research.luresRemaining = 3;
	expectTrue(!validateHudTelemetry(invalidLureInventory),
	           "lure inventory cannot exceed Observer capacity");

	HudTelemetry invalidActiveLure = validTelemetry();
	invalidActiveLure.research.lureRemainingSeconds = 0.0f;
	expectTrue(!validateHudTelemetry(invalidActiveLure),
	           "active lure telemetry requires positive remaining time");

	HudTelemetry invalidSelection = validTelemetry();
	invalidSelection.moves[0].selected = true;
	expectTrue(!validateHudTelemetry(invalidSelection),
	           "exactly one move slot must be selected");

	HudTelemetry invalidShape = validTelemetry();
	invalidShape.moves[0].shape.clear();
	expectTrue(!validateHudTelemetry(invalidShape),
	           "move shape labels are required");

	HudTelemetry invalidRange = validTelemetry();
	invalidRange.moves[0].range =
		std::numeric_limits<float>::quiet_NaN();
	expectTrue(!validateHudTelemetry(invalidRange),
	           "move ranges must be finite and positive");
}

void testMissionCompletionMustMatchObjectives()
{
	HudTelemetry telemetry = validTelemetry();
	telemetry.completedMissionObjectives = 3;
	expectTrue(!validateHudTelemetry(telemetry),
	           "mission summary cannot disagree with objective completion");
}
}

int main()
{
	testValidSnapshotEncodesStableJsonContract();
	testInvalidSnapshotsFailClosed();
	testMissionCompletionMustMatchObjectives();
	if (failures != 0)
	{
		return 1;
	}
	std::cout << "HUD telemetry tests passed" << std::endl;
	return 0;
}
