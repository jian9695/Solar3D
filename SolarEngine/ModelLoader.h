#pragma once

#ifdef _WIN32 || WIN32
#include<Windows.h>
#endif

#include <string>
#include <osg/Node>
#include <osg/BoundingBox>

//Load OAP3Ds (integrated meshes)
class ModelLoader
{
public:
	ModelLoader();
	~ModelLoader();
	static osg::Node* LoadModel(std::string path, bool& isIntegratedMesh);
	static osg::Node* LoadIntegratedMesh(std::string path, bool translateToOrigin = false);
	static osg::Node* Load3DTiles(std::string indir);
	static osg::Node* Load3DTiles(std::string indir, osg::BoundingBox mask, bool intersects);
	static osg::Node* Load3DTiles(std::string indir, std::vector<std::string> maskTiles, bool include);
};