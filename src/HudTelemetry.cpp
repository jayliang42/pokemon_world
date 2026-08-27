#include "HudTelemetry.h"

#include <cmath>
#include <iomanip>
#include <sstream>

namespace
{
bool fail(std::string *error, const std::string &message)
{
	if (error)
	{
		*error = message;
	}
	return false;
}

bool normalized(float value)
{
	return std::isfinite(value) && value >= 0.0f && value <= 1.0f;
}

bool nonNegative(double value)
{
	return std::isfinite(value) && value >= 0.0;
}

std::string jsonString(const std::string &value)
{
	std::ostringstream encoded;
	encoded << '"';
	for (unsigned char character : value)
	{
		switch (character)
		{
		case '"': encoded << "\\\""; break;
		case '\\': encoded << "\\\\"; break;
		case '\b': encoded << "\\b"; break;
		case '\f': encoded << "\\f"; break;
		case '\n': encoded << "\\n"; break;
		case '\r': encoded << "\\r"; break;
		case '\t': encoded << "\\t"; break;
		default:
			if (character < 0x20)
			{
				encoded << "\\u00" << std::hex << std::setw(2)
				        << std::setfill('0') << static_cast<int>(character)
				        << std::dec;
			}
			else
			{
				encoded << character;
			}
		}
	}
	encoded << '"';
	return encoded.str();
}

const char *jsonBool(bool value)
{
	return value ? "true" : "false";
}
}

bool validateHudTelemetry(const HudTelemetry &telemetry, std::string *error)
{
	if (telemetry.summary.empty())
	{
		return fail(error, "HUD summary is empty");
	}
	if (telemetry.player.maximumHealth <= 0 || telemetry.player.health < 0 ||
	    telemetry.player.health > telemetry.player.maximumHealth)
	{
		return fail(error, "player health is out of range");
	}
	if (telemetry.target.maximumHealth <= 0 || telemetry.target.health < 0 ||
	    telemetry.target.health > telemetry.target.maximumHealth ||
	    !normalized(telemetry.target.alertness) ||
	    (telemetry.target.visible && telemetry.target.name.empty()))
	{
		return fail(error, "target telemetry is invalid");
	}
	if (!nonNegative(telemetry.dodge.remainingSeconds) ||
	    !nonNegative(telemetry.dodge.counterWindowRemainingSeconds) ||
	    !normalized(telemetry.dodge.cooldownFraction))
	{
		return fail(error, "dodge telemetry is invalid");
	}
	if (!nonNegative(telemetry.threat.distance) ||
	    (telemetry.threat.visible && telemetry.threat.name.empty()))
	{
		return fail(error, "threat telemetry is invalid");
	}
	if (!nonNegative(telemetry.radar.distance) ||
	    !std::isfinite(telemetry.radar.bearingRadians) ||
	    (telemetry.radar.visible && telemetry.radar.name.empty()))
	{
		return fail(error, "radar telemetry is invalid");
	}
	if (!nonNegative(telemetry.camp.distance) ||
	    !std::isfinite(telemetry.camp.bearingRadians) ||
	    telemetry.camp.settlementScore < 0 ||
	    (telemetry.camp.readyToSubmit && telemetry.camp.submitted))
	{
		return fail(error, "camp telemetry is invalid");
	}
	if (!nonNegative(telemetry.regional.distance) ||
	    !std::isfinite(telemetry.regional.bearingRadians) ||
	    (telemetry.regional.visible &&
	     (telemetry.regional.name.empty() || telemetry.regional.prompt.empty())) ||
	    (telemetry.regional.ready && telemetry.regional.recorded))
	{
		return fail(error, "regional telemetry is invalid");
	}
	const int lureCapacity = telemetry.research.level == 1 ? 2 : 0;
	if (telemetry.research.level < 0 || telemetry.research.level > 1 ||
	    telemetry.research.levelName.empty() ||
	    telemetry.research.luresRemaining < 0 ||
	    telemetry.research.luresRemaining > lureCapacity ||
	    !nonNegative(telemetry.research.lureRemainingSeconds) ||
	    (telemetry.research.lureActive
	         ? telemetry.research.lureRemainingSeconds <= 0.0f
	         : telemetry.research.lureRemainingSeconds != 0.0f))
	{
		return fail(error, "research telemetry is invalid");
	}

	int selectedMoves = 0;
	for (const HudMoveTelemetry &move : telemetry.moves)
	{
		if (move.name.empty() || move.shape.empty() || move.type < 0 ||
		    move.power <= 0 || !std::isfinite(move.range) || move.range <= 0.0f ||
		    !nonNegative(move.remainingSeconds) ||
		    !normalized(move.cooldownFraction))
		{
			return fail(error, "move telemetry is invalid");
		}
		selectedMoves += move.selected ? 1 : 0;
	}
	if (selectedMoves != 1)
	{
		return fail(error, "exactly one move must be selected");
	}

	int completedObjectives = 0;
	for (const HudMissionObjectiveTelemetry &objective :
	     telemetry.missionObjectives)
	{
		if (objective.target <= 0 || objective.current < 0 ||
		    objective.current > objective.target)
		{
			return fail(error, "mission telemetry is invalid");
		}
		completedObjectives += objective.current >= objective.target ? 1 : 0;
	}
	if (telemetry.completedMissionObjectives != completedObjectives)
	{
		return fail(error, "mission completion summary is inconsistent");
	}
	return true;
}

std::string encodeHudTelemetryJson(const HudTelemetry &telemetry)
{
	if (!validateHudTelemetry(telemetry))
	{
		return std::string();
	}

	std::ostringstream encoded;
	encoded << std::setprecision(8)
	        << "{\"summary\":" << jsonString(telemetry.summary)
	        << ",\"player\":{\"health\":" << telemetry.player.health
	        << ",\"maximum\":" << telemetry.player.maximumHealth << '}'
	        << ",\"target\":{\"visible\":" << jsonBool(telemetry.target.visible)
	        << ",\"name\":" << jsonString(telemetry.target.name)
	        << ",\"health\":" << telemetry.target.health
	        << ",\"maximum\":" << telemetry.target.maximumHealth
	        << ",\"alertness\":" << telemetry.target.alertness
	        << ",\"backHitOpportunity\":"
	        << jsonBool(telemetry.target.backHitOpportunity) << '}'
	        << ",\"dodge\":{\"remaining\":" << telemetry.dodge.remainingSeconds
	        << ",\"counterRemaining\":"
	        << telemetry.dodge.counterWindowRemainingSeconds
	        << ",\"fraction\":" << telemetry.dodge.cooldownFraction
	        << ",\"dodging\":" << jsonBool(telemetry.dodge.dodging)
	        << ",\"invulnerable\":" << jsonBool(telemetry.dodge.invulnerable)
	        << '}'
	        << ",\"threat\":{\"visible\":" << jsonBool(telemetry.threat.visible)
	        << ",\"name\":" << jsonString(telemetry.threat.name)
	        << ",\"distance\":" << telemetry.threat.distance
	        << ",\"pursuing\":" << jsonBool(telemetry.threat.pursuing) << '}'
	        << ",\"radar\":{\"visible\":" << jsonBool(telemetry.radar.visible)
	        << ",\"name\":" << jsonString(telemetry.radar.name)
	        << ",\"distance\":" << telemetry.radar.distance
	        << ",\"bearing\":" << telemetry.radar.bearingRadians << '}'
	        << ",\"camp\":{\"distance\":" << telemetry.camp.distance
	        << ",\"bearing\":" << telemetry.camp.bearingRadians
	        << ",\"inside\":" << jsonBool(telemetry.camp.inside)
	        << ",\"grounded\":" << jsonBool(telemetry.camp.grounded)
	        << ",\"readyToSubmit\":"
	        << jsonBool(telemetry.camp.readyToSubmit)
	        << ",\"submitted\":" << jsonBool(telemetry.camp.submitted)
	        << ",\"settlementScore\":"
	        << telemetry.camp.settlementScore << '}'
	        << ",\"regional\":{\"visible\":"
	        << jsonBool(telemetry.regional.visible)
	        << ",\"name\":" << jsonString(telemetry.regional.name)
	        << ",\"prompt\":" << jsonString(telemetry.regional.prompt)
	        << ",\"distance\":" << telemetry.regional.distance
	        << ",\"bearing\":" << telemetry.regional.bearingRadians
	        << ",\"ready\":" << jsonBool(telemetry.regional.ready)
	        << ",\"recorded\":" << jsonBool(telemetry.regional.recorded)
	        << ",\"alpha\":" << jsonBool(telemetry.regional.alpha) << '}'
	        << ",\"research\":{\"level\":" << telemetry.research.level
	        << ",\"name\":" << jsonString(telemetry.research.levelName)
	        << ",\"luresRemaining\":"
	        << telemetry.research.luresRemaining
	        << ",\"lureActive\":" << jsonBool(telemetry.research.lureActive)
	        << ",\"lureRemaining\":"
	        << telemetry.research.lureRemainingSeconds << '}'
	        << ",\"moves\":{\"busy\":" << jsonBool(telemetry.moveInputBusy)
	        << ",\"slots\":[";
	for (std::size_t index = 0; index < telemetry.moves.size(); ++index)
	{
		if (index != 0) encoded << ',';
		const HudMoveTelemetry &move = telemetry.moves[index];
		encoded << "{\"name\":" << jsonString(move.name)
		        << ",\"shape\":" << jsonString(move.shape)
		        << ",\"type\":" << move.type
		        << ",\"power\":" << move.power
		        << ",\"range\":" << move.range
		        << ",\"remaining\":" << move.remainingSeconds
		        << ",\"fraction\":" << move.cooldownFraction
		        << ",\"selected\":" << jsonBool(move.selected) << '}';
	}
	encoded << "]},\"mission\":{\"completed\":"
	        << telemetry.completedMissionObjectives << ",\"objectives\":[";
	for (std::size_t index = 0; index < telemetry.missionObjectives.size(); ++index)
	{
		if (index != 0) encoded << ',';
		const HudMissionObjectiveTelemetry &objective =
			telemetry.missionObjectives[index];
		encoded << "{\"current\":" << objective.current
		        << ",\"target\":" << objective.target << '}';
	}
	encoded << "]}}";
	return encoded.str();
}
