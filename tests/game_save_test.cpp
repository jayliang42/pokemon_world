#include "GameSave.h"

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

GameSaveLimits testLimits()
{
	GameSaveLimits limits;
	limits.captureGoal = 5;
	limits.startingPokeballs = 10;
	limits.playerMaximumHealth = 118;
	limits.groundMaximumHealth = {96, 82, 96, 82};
	limits.flyingMaximumHealth = {118, 118};
	return limits;
}

GameSaveData validSave()
{
	GameSaveData data;
	data.caughtCount = 1;
	data.pokeballs = 8;
	data.defeatedCount = 1;
	data.playerHealth = 112;
	data.missionProgress.superEffectiveHits = 2;
	data.missionProgress.safeLandings = 1;
	data.missionProgress.healthyEeveeCaptures = 1;
	data.missionProgress.bulbasaurFleeObservations = 1;
	data.missionProgress.umbreonWarningObservations = 1;
	data.missionProgress.moonshadowTrackSurveys = 1;
	data.missionProgress.redrockLookoutSurveys = 1;
	data.researchLevel = RESEARCH_LEVEL_OBSERVER;
	data.luresRemaining = 1;
	data.alphaNestResolved = true;
	data.groundPokemon = {{70, false}, {25, true}, {0, false}, {82, false}};
	data.flyingPokemon = {{118, false}, {90, false}};
	return data;
}

std::string replaceLine(const std::string &payload, const std::string &prefix,
	                    const std::string &replacement)
{
	const std::size_t start = payload.find(prefix);
	if (start == std::string::npos)
	{
		return payload;
	}
	const std::size_t end = payload.find('\n', start);
	return payload.substr(0, start) + replacement +
	       (end == std::string::npos ? std::string() : payload.substr(end));
}

std::string removeLine(const std::string &payload, const std::string &prefix)
{
	const std::size_t start = payload.find(prefix);
	if (start == std::string::npos)
	{
		return payload;
	}
	const std::size_t end = payload.find('\n', start);
	return payload.substr(0, start) +
	       (end == std::string::npos ? std::string() : payload.substr(end + 1));
}

GameSaveData completedSave()
{
	GameSaveData data = validSave();
	data.caughtCount = 5;
	data.pokeballs = 3;
	data.defeatedCount = 0;
	data.groundPokemon = {{70, true}, {25, true}, {96, true}, {82, true}};
	data.flyingPokemon = {{118, true}, {90, false}};
	return data;
}

std::string asVersionFivePayload(const GameSaveData &data,
	                             const GameSaveLimits &limits)
{
	std::string payload = encodeGameSave(data, limits);
	payload = replaceLine(payload, "PW_SAVE_V", "PW_SAVE_V5");
	return removeLine(payload, "alpha_nest_resolved=");
}

std::string asVersionFourPayload(const GameSaveData &data,
	                             const GameSaveLimits &limits)
{
	std::string payload = asVersionFivePayload(data, limits);
	payload = replaceLine(payload, "PW_SAVE_V", "PW_SAVE_V4");
	payload = removeLine(payload, "moonshadow_tracks=");
	return removeLine(payload, "redrock_lookout=");
}

std::string asVersionThreePayload(const GameSaveData &data,
	                              const GameSaveLimits &limits)
{
	std::string payload = asVersionFourPayload(data, limits);
	payload = replaceLine(payload, "PW_SAVE_V", "PW_SAVE_V3");
	payload = removeLine(payload, "research_level=");
	return removeLine(payload, "lures=");
}

std::string asVersionTwoPayload(const GameSaveData &data,
	                            const GameSaveLimits &limits)
{
	std::string payload = asVersionThreePayload(data, limits);
	payload = replaceLine(payload, "PW_SAVE_V", "PW_SAVE_V2");
	payload = removeLine(payload, "eevee_healthy=");
	payload = removeLine(payload, "bulbasaur_flee=");
	return removeLine(payload, "umbreon_warning=");
}

std::string asVersionOnePayload(const GameSaveData &data,
	                            const GameSaveLimits &limits)
{
	std::string payload = asVersionTwoPayload(data, limits);
	payload = replaceLine(payload, "PW_SAVE_V", "PW_SAVE_V1");
	return removeLine(payload, "research_submitted=");
}

void testValidSaveRoundTripsExactly()
{
	const GameSaveLimits limits = testLimits();
	const GameSaveData original = validSave();
	const std::string encoded = encodeGameSave(original, limits);
	expectTrue(CURRENT_GAME_SAVE_VERSION == 6 &&
	               currentGameSaveHeader() == "PW_SAVE_V6" &&
	               !encoded.empty() && encoded.find("PW_SAVE_V6\n") == 0 &&
	               encoded.find("eevee_healthy=1\n") != std::string::npos &&
	               encoded.find("bulbasaur_flee=1\n") != std::string::npos &&
	               encoded.find("umbreon_warning=1\n") != std::string::npos &&
	               encoded.find("research_submitted=0\n") != std::string::npos &&
	               encoded.find("research_level=1\n") != std::string::npos &&
	               encoded.find("lures=1\n") != std::string::npos &&
	               encoded.find("moonshadow_tracks=1\n") != std::string::npos &&
	               encoded.find("redrock_lookout=1\n") != std::string::npos &&
	               encoded.find("alpha_nest_resolved=1\n") != std::string::npos,
	           "valid progress encodes with an explicit version header");
	const GameSaveParseResult parsed = parseGameSave(encoded, limits);
	expectTrue(parsed.valid && parsed.sourceVersion == CURRENT_GAME_SAVE_VERSION &&
	               parsed.versionStatus == GameSaveVersionStatus::Current,
	           "encoded progress parses through the current version handler");
	expectTrue(parsed.data.caughtCount == original.caughtCount &&
	               parsed.data.pokeballs == original.pokeballs &&
	               parsed.data.defeatedCount == original.defeatedCount &&
	               parsed.data.playerHealth == original.playerHealth &&
	               !parsed.data.researchSubmitted &&
	               parsed.data.researchLevel == RESEARCH_LEVEL_OBSERVER &&
	               parsed.data.luresRemaining == 1 &&
	               parsed.data.alphaNestResolved,
	           "top-level progress survives a round trip");
	expectTrue(parsed.data.missionProgress.superEffectiveHits == 2 &&
	               parsed.data.missionProgress.safeLandings == 1 &&
	               parsed.data.missionProgress.healthyEeveeCaptures == 1 &&
	               parsed.data.missionProgress.bulbasaurFleeObservations == 1 &&
	               parsed.data.missionProgress.umbreonWarningObservations == 1 &&
	               parsed.data.missionProgress.moonshadowTrackSurveys == 1 &&
	               parsed.data.missionProgress.redrockLookoutSurveys == 1,
	           "research mission events survive a round trip");
	expectTrue(parsed.data.groundPokemon[1].caught &&
	               parsed.data.groundPokemon[1].health == 25 &&
	               parsed.data.groundPokemon[2].health == 0 &&
	               parsed.data.flyingPokemon[1].health == 90,
	           "per-Pokemon capture, fainting, and health survive a round trip");
}

void testVersionDispatcherDefinesMigrationBoundary()
{
	const GameSaveLimits limits = testLimits();
	const std::string valid = encodeGameSave(validSave(), limits);
	const GameSaveParseResult versionFive =
		parseGameSave(asVersionFivePayload(validSave(), limits), limits);
	expectTrue(versionFive.valid && versionFive.sourceVersion == 5 &&
	               versionFive.versionStatus == GameSaveVersionStatus::Migrated &&
	               versionFive.data.missionProgress.moonshadowTrackSurveys == 1 &&
	               versionFive.data.missionProgress.redrockLookoutSurveys == 1 &&
	               !versionFive.data.alphaNestResolved,
	           "a V5 save preserves regional surveys and starts the Alpha event unresolved");
	const GameSaveParseResult versionFour =
		parseGameSave(asVersionFourPayload(validSave(), limits), limits);
	expectTrue(versionFour.valid && versionFour.sourceVersion == 4 &&
	               versionFour.versionStatus == GameSaveVersionStatus::Migrated &&
	               versionFour.data.researchLevel == RESEARCH_LEVEL_OBSERVER &&
	               versionFour.data.luresRemaining == 1 &&
	               versionFour.data.missionProgress.moonshadowTrackSurveys == 0 &&
	               versionFour.data.missionProgress.redrockLookoutSurveys == 0,
	           "a V4 save preserves progression and starts regional tasks incomplete");
	const GameSaveParseResult versionThree =
		parseGameSave(asVersionThreePayload(validSave(), limits), limits);
	expectTrue(versionThree.valid && versionThree.sourceVersion == 3 &&
	               versionThree.versionStatus == GameSaveVersionStatus::Migrated &&
	               versionThree.data.missionProgress.healthyEeveeCaptures == 1 &&
	               versionThree.data.researchLevel == RESEARCH_LEVEL_TRAINEE &&
	               versionThree.data.luresRemaining == 0 &&
	               versionThree.data.missionProgress.moonshadowTrackSurveys == 0 &&
	               versionThree.data.missionProgress.redrockLookoutSurveys == 0,
	           "a V3 save preserves species tasks and starts new progression locked");
	const GameSaveParseResult versionTwo =
		parseGameSave(asVersionTwoPayload(validSave(), limits), limits);
	expectTrue(versionTwo.valid && versionTwo.sourceVersion == 2 &&
	               versionTwo.versionStatus == GameSaveVersionStatus::Migrated &&
	               !versionTwo.data.researchSubmitted &&
	               versionTwo.data.missionProgress.healthyEeveeCaptures == 0 &&
	               versionTwo.data.missionProgress.bulbasaurFleeObservations == 0 &&
	               versionTwo.data.missionProgress.umbreonWarningObservations == 0,
	           "a V2 save migrates with new species research counters at zero");
	const GameSaveParseResult versionOne =
		parseGameSave(asVersionOnePayload(validSave(), limits), limits);
	expectTrue(versionOne.valid && versionOne.sourceVersion == 1 &&
	               versionOne.versionStatus == GameSaveVersionStatus::Migrated &&
	               !versionOne.data.researchSubmitted,
	           "an active V1 save migrates into an unsubmitted current research run");

	const GameSaveParseResult completedVersionOne =
		parseGameSave(asVersionOnePayload(completedSave(), limits), limits);
	expectTrue(completedVersionOne.valid &&
	               completedVersionOne.data.researchSubmitted,
	           "a completed V1 save migrates without reopening its finished run");

	const GameSaveParseResult older =
		parseGameSave("PW_SAVE_V0\n" + valid.substr(11), limits);
	expectTrue(!older.valid && older.sourceVersion == 0 &&
	               older.versionStatus == GameSaveVersionStatus::UnsupportedOlder &&
	               older.error.find("migration") != std::string::npos,
	           "older saves fail closed when no historical migrator exists");

	const GameSaveParseResult newer =
		parseGameSave("PW_SAVE_V7\n" + valid.substr(11) + "\nweather=clear",
		              limits);
	expectTrue(!newer.valid && newer.sourceVersion == 7 &&
	               newer.versionStatus == GameSaveVersionStatus::UnsupportedNewer &&
	               newer.error.find("newer") != std::string::npos,
	           "future saves are distinguished from unsupported legacy saves");

	const GameSaveParseResult malformed =
		parseGameSave("PW_SAVE_Vx\n" + valid.substr(11), limits);
	expectTrue(!malformed.valid && malformed.sourceVersion == -1 &&
	               malformed.versionStatus == GameSaveVersionStatus::Malformed,
	           "malformed version headers are rejected before field parsing");
}

void testRejectsMalformedFieldsAndNumbers()
{
	const GameSaveLimits limits = testLimits();
	const std::string valid = encodeGameSave(validSave(), limits);
	expectTrue(!parseGameSave(replaceLine(valid, "caught=", "caught=-1"),
	                          limits)
	                .valid,
	           "negative counters are rejected");
	expectTrue(!parseGameSave(replaceLine(valid, "balls=", "balls=7oops"),
	                          limits)
	                .valid,
	           "integer fields reject trailing text");
	expectTrue(!parseGameSave(replaceLine(valid, "player_hp=",
	                                     "player_hp=999999999999999999"),
	                          limits)
	                .valid,
	           "overflowing integers are rejected before conversion");
	expectTrue(!parseGameSave(replaceLine(valid, "safe_landings=",
	                                     "unknown=1"),
	                          limits)
	                .valid,
	           "unknown or reordered fields are rejected");
	expectTrue(!parseGameSave(replaceLine(valid, "umbreon_warning=",
	                                     "umbreon_warning=1000"),
	                          limits)
	                .valid,
	           "species research counters reject values above their limit");
	expectTrue(!parseGameSave(replaceLine(valid, "moonshadow_tracks=",
	                                     "moonshadow_tracks=2"),
	                          limits)
	                .valid &&
	               !parseGameSave(replaceLine(valid, "redrock_lookout=",
	                                          "redrock_lookout=-1"),
	                              limits)
	                    .valid,
	           "one-time regional observations reject tampered values");
	expectTrue(!parseGameSave(replaceLine(valid, "alpha_nest_resolved=",
	                                     "alpha_nest_resolved=2"),
	                          limits)
	                .valid &&
	               !parseGameSave(replaceLine(valid, "alpha_nest_resolved=",
	                                          "alpha_nest_resolved=-1"),
	                              limits)
	                    .valid,
	           "the persisted Alpha result rejects values outside a boolean");
	expectTrue(!parseGameSave(replaceLine(valid, "research_level=",
	                                     "research_level=2"),
	                          limits)
	                .valid &&
	               !parseGameSave(replaceLine(valid, "lures=", "lures=3"),
	                              limits)
	                    .valid,
	           "progression and lure inventory reject unsupported values");
}

void testRejectsOversizedAndIncorrectPokemonState()
{
	const GameSaveLimits limits = testLimits();
	const std::string valid = encodeGameSave(validSave(), limits);
	expectTrue(!parseGameSave(std::string(MAX_GAME_SAVE_BYTES + 1, 'x'), limits)
	                .valid,
	           "oversized local storage payload is rejected before parsing");
	expectTrue(!parseGameSave(replaceLine(valid, "ground=",
	                                     "ground=70:0,25:1"),
	                          limits)
	                .valid,
	           "Pokemon slot count must match this game build");
	expectTrue(!parseGameSave(replaceLine(valid, "ground=",
	                                     "ground=97:0,25:1,0:0,82:0"),
	                          limits)
	                .valid,
	           "health above the slot species maximum is rejected");
	expectTrue(!parseGameSave(replaceLine(valid, "ground=",
	                                     "ground=70:0,0:1,0:0,82:0"),
	                          limits)
	                .valid,
	           "a captured Pokemon cannot also be fainted");
}

void testRejectsTamperedCrossFieldInvariants()
{
	const GameSaveLimits limits = testLimits();
	const std::string valid = encodeGameSave(validSave(), limits);
	expectTrue(!parseGameSave(replaceLine(valid, "caught=", "caught=2"),
	                          limits)
	                .valid,
	           "declared catches must match captured Pokemon slots");
	expectTrue(!parseGameSave(replaceLine(valid, "defeated=", "defeated=0"),
	                          limits)
	                .valid,
	           "declared defeats must match fainted Pokemon slots");
	expectTrue(!parseGameSave(replaceLine(valid, "balls=", "balls=10"), limits)
	                .valid,
	           "captures cannot exceed consumed Poke Balls");

	GameSaveData invalid = validSave();
	invalid.groundPokemon.pop_back();
	expectTrue(encodeGameSave(invalid, limits).empty(),
	           "encoder refuses invalid internal save data");

	GameSaveData prematureSubmission = validSave();
	prematureSubmission.researchSubmitted = true;
	expectTrue(encodeGameSave(prematureSubmission, limits).empty(),
	           "research cannot be submitted before the capture objective is complete");

	GameSaveData lockedLures = validSave();
	lockedLures.researchLevel = RESEARCH_LEVEL_TRAINEE;
	expectTrue(encodeGameSave(lockedLures, limits).empty(),
	           "a Trainee save cannot contain locked lure inventory");

	GameSaveData duplicatedRegionalCredit = validSave();
	duplicatedRegionalCredit.missionProgress.moonshadowTrackSurveys = 2;
	expectTrue(encodeGameSave(duplicatedRegionalCredit, limits).empty(),
	           "the encoder refuses duplicated one-time regional credit");

	GameSaveData forgedAlphaCredit = validSave();
	forgedAlphaCredit.missionProgress.moonshadowTrackSurveys = 0;
	expectTrue(encodeGameSave(forgedAlphaCredit, limits).empty() &&
	               !parseGameSave(replaceLine(valid, "moonshadow_tracks=",
	                                          "moonshadow_tracks=0"),
	                              limits)
	                    .valid,
	           "Alpha completion cannot be forged before both prerequisite surveys");
}
}

int main()
{
	testValidSaveRoundTripsExactly();
	testVersionDispatcherDefinesMigrationBoundary();
	testRejectsMalformedFieldsAndNumbers();
	testRejectsOversizedAndIncorrectPokemonState();
	testRejectsTamperedCrossFieldInvariants();
	if (failures != 0)
	{
		std::cerr << failures << " game save checks failed" << std::endl;
		return 1;
	}
	std::cout << "Game save checks passed" << std::endl;
	return 0;
}
