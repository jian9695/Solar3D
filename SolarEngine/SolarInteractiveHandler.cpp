#include "SolarInteractiveHandler.h"
#include <osg/ComputeBoundsVisitor>
#include <osgDB/WriteFile>
#include <ctime>

SolarInteractiveHandler::SolarInteractiveHandler(
	osg::Node* sceneNode,
	osg::Group* root,
	osgEarth::MapNode* mapNode,
	osgGA::CameraManipulator* manip,
	osgViewer::Viewer* viewer,
	SolarParam* solarParam,
	OnResultsUpdated resultsCallback)
{
	m_viewer = viewer;
	m_manip = manip;
	m_root = root;
	m_solarParam = solarParam;
	m_resultsCallback = resultsCallback;
	m_sceneNode = sceneNode;
	m_mapNode = mapNode;
	m_wgs84 = nullptr;

  if (m_mapNode)
		m_cubemap = Cubemap::create(512, mapNode);
	else
		m_cubemap = Cubemap::create(512, sceneNode);
	m_cubemap->setNodeMask(false);
	root->addChild(m_cubemap);

	for (int i = 0; i < m_cubemap->getNumChildren(); i++)
	{
		m_cubemap->getFace(i)->setRenderOrder(osg::Camera::PRE_RENDER, i);
	}

	//create a node to transform a cubemap into a fisheye view and then render onto an off-screen image for svf calculation
	m_cubemap2fisheyeCamera = m_cubemap->toHemisphericalSurface();
	m_cubemap2fisheyeCamera->setRenderOrder(osg::Camera::PRE_RENDER, m_cubemap->getNumChildren());
	root->addChild(m_cubemap2fisheyeCamera);

	//create a point labels renderer
	m_pointRenderer = new PointsRenderer;

	osg::ref_ptr<osg::Image> depthImage = new osg::Image;
	depthImage->allocateImage(512, 512, 1, GL_DEPTH_COMPONENT, GL_FLOAT);
	m_viewer->getCamera()->attach(osg::Camera::DEPTH_BUFFER, depthImage.get());
	m_pointRenderer->setSceneDepthImage(depthImage.get());
	m_pointRenderer->setSceneCamera(m_viewer->getCamera());
	m_root->addChild(m_pointRenderer);
}

SolarInteractiveHandler::~SolarInteractiveHandler()
{

}

void SolarInteractiveHandler::printfVec3d(osg::Vec3d v)
{
	printf("%f,%f,%f\n", v.x(), v.y(), v.z());
}

void SolarInteractiveHandler::printfVec3(osg::Vec3 v)
{
	printf("%f,%f,%f\n", v.x(), v.y(), v.z());
}

void SolarInteractiveHandler::processIntersection(osgUtil::LineSegmentIntersector* ray)
{

	if (ray->getIntersections().size() == 0)
		return;
	osg::Vec3d worldPos = ray->getFirstIntersection().getWorldIntersectPoint();
	osg::NodePath nodePath = ray->getFirstIntersection().nodePath;
	osg::Node* node = nodePath[0];
	if (!isEarth() && !m_sceneNode->getBound().contains(worldPos))
		return;
	bool hasSpatialRef = isEarth();
	osg::Matrixd local2world = osg::Matrixd::identity();
	osg::Vec3d geoPos = osg::Vec3d(m_solarParam->m_lon, m_solarParam->m_lat, m_solarParam->m_elev + worldPos.z());
	if (hasSpatialRef)
	{
		std::tie(hasSpatialRef, geoPos, local2world) = getGeoTransform(worldPos);
		if (geoPos.z() < -1000 || geoPos.z() > 10000)
			return;
	}

	osg::Vec3d orieye, oricenter, oriup;
	m_viewer->getCamera()->getViewMatrixAsLookAt(orieye, oricenter, oriup);
	osg::Vec3d _dir = orieye - oricenter;
	_dir.normalize();
	osg::Vec3d curcenter = ray->getFirstIntersection().getWorldIntersectPoint();
	osg::Vec3d cureye = ray->getFirstIntersection().getWorldIntersectPoint() + _dir * 50;
	osg::Vec3d surfaceNormal = ray->getFirstIntersection().getWorldIntersectNormal();
	surfaceNormal.normalize();
	worldPos = worldPos + surfaceNormal * 0.1; //offset from the surface
	if (hasSpatialRef)
	{
		std::tie(hasSpatialRef, geoPos, local2world) = getGeoTransform(worldPos);
		osg::Matrixd world2local = osg::Matrixd::inverse(local2world);
		surfaceNormal = surfaceNormal * world2local;
		surfaceNormal.normalize();
	}

	SolarParam param = *m_solarParam;
	param.m_lon = geoPos.x();
	param.m_lat = geoPos.y();
	param.m_elev = param.m_elev + max(geoPos.z(), 0);
	double surfaceTilt, surfaceAzimuth;
	Utils::computeSurfaceAngles(surfaceNormal, surfaceTilt, surfaceAzimuth);
	param.m_slope = surfaceTilt;
	param.m_aspect = surfaceAzimuth;

	for (int i = 0; i < m_cubemap->getNumChildren(); i++)
	{
		CubemapSurface* cameraBuffer = (CubemapSurface*)m_cubemap->getChild(i);
		cameraBuffer->m_pos = worldPos;
		cameraBuffer->m_local2World = local2world;
		cameraBuffer->update();
	}
	
	m_cubemap->setWallShaderOn(param.m_slope > 85 && isEarth());

	m_cubemap->setNodeMask(true);
	//_viewer->setCameraManipulator(nullptr, false);
	//_viewer->getCamera()->setNodeMask(false);
	m_viewer->frame();
	for (size_t i = 0; i < 2; i++)
	{
		m_viewer->frame();
	}
	m_cubemap->setNodeMask(false);
	//_viewer->setCameraManipulator(nullptr, false);
	//_viewer->getCamera()->setNodeMask(true);

	//Write out fisheye images for testing
	for (size_t i = 0; i < m_cubemap->getNumChildren(); i++)
	{
		CubemapSurface* face = m_cubemap->getFace(i);
		//osgDB::writeImageFile(*face->Image(), face->getName() + ".png");
	}
	//Write fisheye and cubemap to files for testing
	//osg::ref_ptr<osg::Image> fisheye = _cubemap->toHemisphericalImage(512, 512);
	//osgDB::writeImageFile(*fisheye, "fisheye2.png");
	//osgDB::writeImageFile(*m_cubemap2fisheyeCamera->Image(), "fisheye.png");
	double svf = Utils::calSVF(m_cubemap2fisheyeCamera->Image().get(), false);


	SolarRadiation solarRad = calSolar(param);
	solarRad.m_svf = svf;
	m_resultsCallback(svf, solarRad);
	SolarRadiationPoint radPoint(worldPos, param, solarRad);
	m_pointRenderer->pushPoint(radPoint, m_cubemap2fisheyeCamera->Image().get());
}

bool SolarInteractiveHandler::handle(const osgGA::GUIEventAdapter& ea, osgGA::GUIActionAdapter& aa)
{
	if (ea.getEventType() == osgGA::GUIEventAdapter::KEYUP)
		return false;
	if (ea.getEventType() == osgGA::GUIEventAdapter::MOVE)
		return false;
	if (ea.getEventType() == osgGA::GUIEventAdapter::DOUBLECLICK)
	{
		if (ea.getModKeyMask() & ea.MODKEY_CTRL)
			return true;
		return false;
	}
	if (ea.getEventType() == osgGA::GUIEventAdapter::DRAG)
	{
		if (ea.getModKeyMask() & ea.MODKEY_CTRL)
			return true;
		return false;
	}

	if (ea.getEventType() == osgGA::GUIEventAdapter::FRAME)
		return false;
	osgViewer::Viewer* viewer = dynamic_cast<osgViewer::Viewer*>(&aa);
	if (!viewer)
		return false;

	if ((ea.getModKeyMask() & ea.MODKEY_CTRL) && !(ea.getModKeyMask() & ea.MODKEY_LEFT_SHIFT))
	{
		if (ea.getButtonMask() == osgGA::GUIEventAdapter::LEFT_MOUSE_BUTTON && ea.getEventType() == osgGA::GUIEventAdapter::PUSH)
		{
			osg::ref_ptr<osgUtil::LineSegmentIntersector> ray = new osgUtil::LineSegmentIntersector(osgUtil::Intersector::PROJECTION, ea.getXnormalized(), ea.getYnormalized());
			osgUtil::IntersectionVisitor visitor(ray);
			viewer->getCamera()->accept(visitor);
			processIntersection(ray.get());
			return true;
		}
		else if (osgGA::GUIEventAdapter::KEYDOWN)
		{
			int key = ea.getUnmodifiedKey();
			if (key == osgGA::GUIEventAdapter::KEY_Delete || key == osgGA::GUIEventAdapter::KEY_KP_Delete || key == osgGA::GUIEventAdapter::KEY_BackSpace || key == 65454)
			{
				m_pointRenderer->popPoint();
				return false;
			}
			else if (key == osgGA::GUIEventAdapter::KEY_Z)
			{
				m_pointRenderer->undo();
				return false;
			}
			else if (key == osgGA::GUIEventAdapter::KEY_Y)
			{
				m_pointRenderer->redo();
				return false;
			}
			else if (key == osgGA::GUIEventAdapter::KEY_E)
			{
				std::time_t t = std::time(0);   // get time now
				std::tm* now = std::localtime(&t);
				std::stringstream filenameSS;
				filenameSS << 1900 + now->tm_year << "-" << now->tm_mon + 1 << "-" << now->tm_mday
					<< "-" << now->tm_hour << "-" << now->tm_min << "-" << now->tm_sec << ".csv";
				m_pointRenderer->exportPoints(filenameSS.str());
				return false;
			}
			else if (key == osgGA::GUIEventAdapter::KEY_T)
			{
				m_pointRenderer->toggleTextDisplay();
				return false;
			}
			//printf("%d,%d,%d\n", key, osgGA::GUIEventAdapter::KEY_Z, osgGA::GUIEventAdapter::KEY_Y);
		}
	}
	return false;
}

SolarRadiation SolarInteractiveHandler::calSolar(SolarParam& solarParam)
{
	osg::Image* img = m_cubemap2fisheyeCamera->Image().get();
	SolarRadiation annualRad;
	annualRad.Zero();
	std::vector<SolarRadiation> dailyRads;
	int startDay = solarParam.m_startDay;
	int endDay = solarParam.m_endDay;
	if (solarParam.m_isSingleDay && endDay > startDay)
		endDay = startDay;
	int lastSteps = 0;
	double linke = solarParam.m_linke;
	double elevation = solarParam.m_elev;
	double slope = solarParam.m_slope;
	double aspect = solarParam.m_aspect;
	double lon = solarParam.m_lon;
	double lat = solarParam.m_lat;
	for (int day = startDay; day <= endDay; day++)
	{
		solarParam.m_day = day;
		std::vector<SunVector> sunVecs = Utils::getSunVectors(solarParam);
		std::string shadowInfo = "";
		solarParam.m_day = day;
		SolarRadiation dailyRad;
		dailyRad.Zero();
		for (int n = 0; n < sunVecs.size(); n++)
		{
			SunVector sunVec = sunVecs[n];
			bool isShadowed = m_cubemap->isShadowed((double)sunVec.m_alt, (double)sunVec.m_azimuth);
			SolarRadiation rad = Utils::calSolar(day, linke, elevation, sunVec.m_azimuth, 90.0 - sunVec.m_alt, aspect, slope, isShadowed);
			dailyRad = dailyRad + (rad * solarParam.m_time_step);
		}

		dailyRads.push_back(dailyRad);
		annualRad = annualRad + dailyRad;
	}
	if (solarParam.m_shadowInfo)
		delete[] solarParam.m_shadowInfo;
	return annualRad;
}

std::tuple<bool, osg::Vec3d, osg::Matrixd> SolarInteractiveHandler::getGeoTransform(osg::Vec3d worldPos)
{
	osg::Matrixd local2World = osg::Matrixd::identity();
	if (!m_mapNode)
		return std::make_tuple(false, worldPos, local2World);
	osg::Vec3d geoPos;
	if (!m_mapNode->getMapSRS()->isGeographic()) 
	{
		if(!m_wgs84)
			m_wgs84 = osgEarth::SpatialReference::create("wgs84");
		m_mapNode->getMapSRS()->transform(worldPos, m_wgs84, geoPos);
		return std::make_tuple(true, geoPos, local2World);
	}
	m_mapNode->getMapSRS()->transformFromWorld(worldPos, geoPos);
	m_mapNode->getMapSRS()->createLocalToWorld(geoPos, local2World);
	local2World = local2World * osg::Matrixd::translate(-local2World.getTrans());
	return std::make_tuple(true, geoPos, local2World);
}

bool SolarInteractiveHandler::isEarth() { return m_mapNode; }

void SolarInteractiveHandler::postDrawUpdate()
{
	m_pointRenderer->postDrawUpdate();
}

bool SolarInteractiveHandler::queryPoint(const float& mouseX, const float& mouseY, SolarRadiationPoint& solarPoint)
{
	return m_pointRenderer->queryPoint(mouseX, mouseY, solarPoint);
}

osg::Image* SolarInteractiveHandler::getFisheyeForPoint(const int& pointId)
{
	return m_pointRenderer->getFisheyeForPoint(pointId);
}

//Get the world/geo coordinates at the mouse cursor position
std::tuple<osg::Vec3d, osg::Vec3d> SolarInteractiveHandler::queryCoordinatesAtMouse(const float& mouseX, const float& mouseY)
{
	osg::Image* depthImage = m_pointRenderer->depthImage();
	osg::Vec2 normalizedScreenPos(mouseX, mouseY);
	osg::Vec2 uv(normalizedScreenPos.x() * 0.5 + 0.5, normalizedScreenPos.y() * 0.5 + 0.5);
	osg::Vec2 mousePos(normalizedScreenPos.x() * depthImage->s(), normalizedScreenPos.y() * depthImage->t());
	float sceneDepth = depthImage->getColor(uv).r();
	osg::Matrixd projInverse = osg::Matrixd::inverse(m_viewer->getCamera()->getProjectionMatrix());
	osg::Matrixd viewInverse = osg::Matrixd::inverse(m_viewer->getCamera()->getViewMatrix());
	osg::Vec3d worldPos = Utils::worldPosFromDepth(sceneDepth, projInverse, viewInverse, uv);
	bool hasSpatialRef = isEarth();
	osg::Matrixd local2world = osg::Matrixd::identity();
	osg::Vec3d geoPos = worldPos;
	if (hasSpatialRef)
	{
		std::tie(hasSpatialRef, geoPos, local2world) = getGeoTransform(worldPos);
	}
	return std::make_tuple(worldPos, geoPos);
}