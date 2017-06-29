#pragma once
#include "osg/Geode"
#include "osg/Vec3d"
//Render a point at a 3D position picked by mouse double-click 
class PointRenderer : public osg::Geode
{
public:
	PointRenderer();
	~PointRenderer();
	void create();
	//set the position of the point lable
	void setPoint(osg::Vec3d point);
};

