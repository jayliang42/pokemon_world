

#pragma once

#ifndef LAB471_SHAPE_H_INCLUDED
#define LAB471_SHAPE_H_INCLUDED

#include <string>
#include <vector>
#include <memory>

#include <glm/glm.hpp>

class Program;

class Shape
{

public:
	struct PartInfo
	{
		std::string name;
		glm::vec3 minimum = glm::vec3(0.0f);
		glm::vec3 maximum = glm::vec3(0.0f);
	};

	// stbi_load(char const *filename, int *x, int *y, int *comp, int req_comp)
	void loadMesh(const std::string &meshName, const std::string *mtlName = NULL, unsigned char *(loadimage)(char const *, int *, int *, int *, int) = NULL);
	void init();
	void resize();
	void draw(const std::shared_ptr<Program> prog, bool use_extern_texures) const;
	void drawPart(const std::shared_ptr<Program> prog, int partIndex,
	              bool useExternalTextures) const;
	int partCount() const;
	const PartInfo &partInfo(int partIndex) const;
	unsigned int *textureIDs = NULL;

private:
	int obj_count = 0;
	std::vector<unsigned int> *eleBuf = NULL;
	std::vector<float> *posBuf = NULL;
	std::vector<float> *norBuf = NULL;
	std::vector<float> *texBuf = NULL;
	unsigned int *materialIDs = NULL;

	unsigned int *eleBufID = 0;
	unsigned int *posBufID = 0;
	unsigned int *norBufID = 0;
	unsigned int *texBufID = 0;
	unsigned int *vaoID = 0;
	std::vector<PartInfo> parts_;
};

#endif // LAB471_SHAPE_H_INCLUDED
