#pragma once
#ifdef _WIN32 || WIN32 
#include <Windows.h>
#endif
#include <osgViewer/Renderer>
#include <osgGA/TrackballManipulator>
#include <osgDB/WriteFile>
#include <osgViewer/ViewerEventHandlers>
using namespace osgDB;
using namespace OpenThreads;

class VGEDatabasePager : public osgDB::DatabasePager
{
public:
	VGEDatabasePager()
		:osgDB::DatabasePager(){}
	void frame();
	void pause();
	void resume();
};
