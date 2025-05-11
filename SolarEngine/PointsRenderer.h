#pragma once
#include <osg/Geode>
#include <osg/Vec3d>
#include <osg/PagedLOD>
#include "Utils.h"

enum ActionTypeEnum { PUSH, POP };
struct Action : SolarRadiationPoint
{
	ActionTypeEnum actionType;
	Action() { actionType = ActionTypeEnum::PUSH; }
	Action(ActionTypeEnum type, SolarRadiationPoint point) { actionType = type; *((SolarRadiationPoint*)this) = point; }
};

class TextLOD : public osg::PagedLOD
{
public:
	TextLOD(const SolarRadiationPoint& point) { _point = point; };
	osg::BoundingSphere computeBound() const
	{
		return osg::BoundingSphere(_point.m_pos, 100);
	}
public:
	SolarRadiationPoint _point;
};

//Render points in world-space
//Use a depth map of the sceen to perform occlusion query
//Render a point only if at least one pixel in the point's screen bounding box is visible compared to the depth map.
//Avoid z fighting
class PointsRenderer : public osg::Group
{
public:
	PointsRenderer() { m_sceneDepthImage = nullptr; m_sceneCamera = nullptr; m_pointId = 0; m_toggleTextDisplay = true; };
	//Add a point label
	void pushPoint(SolarRadiationPoint& point, const osg::Image* img = nullptr);
	//Remove a point label
	void popPoint();
	void undo();
	void redo();
	//export points to a text file
	void exportPoints(const std::string& filename);
	void setSceneDepthImage(osg::Image* depthImg) { m_sceneDepthImage = depthImg; };
	void setSceneCamera(osg::Camera* sceneCamera) { m_sceneCamera = sceneCamera; };
	void postDrawUpdate();
	//query point at the cursor position
	bool queryPoint(const float& mouseX, const float& mouseY, SolarRadiationPoint& solarPoint);
	osg::Image* getFisheyeForPoint(const int& pointId);
	osg::Image* depthImage() { return m_sceneDepthImage; }
	bool toggleTextDisplay() { m_toggleTextDisplay = !m_toggleTextDisplay; return m_toggleTextDisplay; }

private:
	std::vector<Action> m_doStack;
	std::vector<Action> m_undoStack;
	osg::Image* m_sceneDepthImage;
	osg::Camera* m_sceneCamera;
	int m_pointId;
	bool m_toggleTextDisplay;
	std::map<int, osg::ref_ptr<osg::Image>> m_imagesMap;
private:
	void performAction(const Action& action);
	void pushPointInternal(const SolarRadiationPoint& point);
	void popPointInternal();
	//Compare the screen bounding box of a point against the depth map to determine its visibility
	bool isPointVisible(const osg::Matrixd& projInverse, const osg::Matrixd& viewInverse, const osg::Vec3d& eye, const osg::Vec2& uv, double distTocamera, int windowSize = 5);
};

