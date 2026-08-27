#include "FieldCamp.h"

FieldCampLayout defaultFieldCampLayout()
{
	return FieldCampLayout();
}

float horizontalDistanceToCamp(const glm::vec3 &position,
	                           const FieldCampLayout &layout)
{
	return glm::distance(glm::vec2(position.x, position.z), layout.center);
}

bool isInsideCampInteractionRange(const glm::vec3 &position,
	                              const FieldCampLayout &layout)
{
	return horizontalDistanceToCamp(position, layout) <=
	       layout.interactionRadius;
}
