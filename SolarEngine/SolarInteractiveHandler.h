#pragma once

#ifdef _WIN32 || WIN32 
#include <Windows.h>
#endif

#include <osg/Camera>
#include <osg/MatrixTransform>
#include <osgGA/TrackballManipulator>
#include <osgViewer/Viewer>
#include <osgEarth/MapNode>
#include <iomanip>
#include "PointsRenderer.h"
#include "RenderSurface.h"
#include "Cubemap.h"

typedef void(*OnResultsUpdated)(float, SolarRadiation);

class SolarInteractiveHandler : public osgGA::GUIEventHandler
{
public:
	SolarInteractiveHandler(
		osg::Node* sceneNode,
		osg::Group* root,
		osgEarth::MapNode* mapNode,
		osgGA::CameraManipulator* manip,
		osgViewer::Viewer* viewer,
		SolarParam* solarParam,
		OnResultsUpdated resultsCallback);
	~SolarInteractiveHandler();
	bool handle(const osgGA::GUIEventAdapter& ea, osgGA::GUIActionAdapter& aa);
	RenderSurface* fisheyeSurface() { return m_cubemap2fisheyeCamera; }
	void postDrawUpdate();
	bool queryPoint(const float& mouseX, const float& mouseY, SolarRadiationPoint& solarPoint);
	std::tuple<osg::Vec3d, osg::Vec3d> SolarInteractiveHandler::queryCoordinatesAtMouse(const float& mouseX, const float& mouseY);
	osg::Image* getFisheyeForPoint(const int& pointId);

private:
	void processIntersection(osgUtil::LineSegmentIntersector* ray);
	SolarRadiation calSolar(SolarParam& solarParam);
	bool isEarth();
	std::tuple<bool, osg::Vec3d, osg::Matrixd> getGeoTransform(osg::Vec3d worldPos);
	void printfVec3d(osg::Vec3d v);
	void printfVec3(osg::Vec3 v);

private:
	Cubemap* m_cubemap;
	osgGA::CameraManipulator* m_manip;
	osg::Group* m_root;
	osg::Node* m_sceneNode;
	osgEarth::MapNode* m_mapNode;
	PointsRenderer* m_pointRenderer;
	osgViewer::Viewer* m_viewer;
	OnResultsUpdated m_resultsCallback;
	SolarParam* m_solarParam;
	RenderSurface* m_cubemap2fisheyeCamera;
	osgEarth::SpatialReference* m_wgs84;
};

