#pragma once

#ifdef _WIN32 || WIN32 
#include <Windows.h>
#endif

#include <osg/Vec3>
#include <osg/Vec3d>
#include <vector>
#include <osg/Image>
#include <string>
#include "TypeDef.h"
#include "CustomControls.h"

class Utils
{
public:
	static double calculateAspect(osg::Vec3d normal);
	static double calculateSlope(osg::Vec3d normal);
	static double calAzimuthAngle(double x, double y);
	static osg::Vec3d solarAngle2Vector(double alt, double azimuth);
	static void computeSurfaceAngles(const osg::Vec3d normal, double& tilt, double& azimuth);
	static std::vector<osg::Vec3d> sunVector2LightDir(std::vector<SunVector>& sunvectors);
	static double calSVF(osg::Image* img, bool applyLambert);
	static SolarRadiation calSolar(osg::Image* img, SolarParam* solarParam, osg::Vec3d pos, osg::Vec3d normal, osg::Node* sceneNode);
	static std::string value2String(float value, int precision);
	static osg::Vec3 worldPosFromDepth(float depth, const osg::Matrixd& projMatrixInv, const osg::Matrixd& viewMatrixInv, const osg::Vec2& uv);
	static std::string padRight(std::string str, const size_t num, const char paddingChar = ' ');
	static bool nodeHasNormals(osg::Node* node);
	static CustomControls::CustomImageControl* createCompass(CustomControls::ControlCanvas* cs, int viewWidth, int viewHeight);
	static SolarParam createDefaultSolarParam();

	static std::vector<SunVector> getSunVectors(SolarParam& sparam);
	static void solar_azimuth_angle(int day_of_year, double latitude, double longitude, double local_time, double& zenith, double& azimuth);
	static void Utils::getDaylightHours(int dayofyear, double lat, double lon, double& startHour, double& endHour);
	static SolarRadiation calSolar(int dayofyear, double linke, double elevation, double solarAzimuth, double solarZenith, double surfaceAzimuth, double surfaceTilt, bool isShadowed = false);
};