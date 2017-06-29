

#ifdef _WIN32 || WIN32 
#include <Windows.h>
#endif
#include "SVFDatabasePager.h"
#include "SVFComputeTools.h"
#include "ShapeFile.h"
#include <osgDB/fstream> 
#ifdef WIN32 
#define ifstream osgDB::ifstream 
#define ofstream osgDB::ofstream 

#else 

#include <fstream> 
#define ifstream std::ifstream 
#define ofstream std::ofstream 

#endif

//--------------------------------------------------------------------------
int main(int argc, char **argv)
{
    // parse arguments
    osg::ArgumentParser arguments(&argc,argv);
	// construct the viewer.
	osgViewer::Viewer* viewer = new osgViewer::Viewer(arguments);
	//viewer->setUpViewInWindow(100, 100, 1024, 1024);
	viewer->setUpViewAcrossAllScreens();

	 osg::ref_ptr<osg::Group> root = new osg::Group;
	 osg::ref_ptr<osg::Node> city = osgDB::readNodeFile("./data/models/OAP3D/OAP3D.osgb");
	 root->addChild(city.get());
	 //create a node to render a cubemap from a 3D position
	 osg::Group* cubemapCameras = SVFComputeTools::createSVFCameras(city);
	 cubemapCameras->setNodeMask(false);
	 root->addChild(cubemapCameras);

	 //create a node to transform a cubemap into a fisheye view and then render onto the screen
	 root->addChild(SVFComputeTools::cubemap2hemispherical(cubemapCameras));

	 //create a node to transform a cubemap into a fisheye view and then render onto an off-screen image for svf calculation
	 std::vector<osg::GraphicsContext*> contexts;
	 viewer->getContexts(contexts);
	 osg::ref_ptr<CameraBuffer> cubemap2fisheyeCamera = CameraBuffer::createSlave(512, 512,contexts[0]);
	 viewer->addSlave(cubemap2fisheyeCamera.get(), false);
	 cubemap2fisheyeCamera->setCullingActive(false);
	 cubemap2fisheyeCamera->addChild(SVFComputeTools::cubemap2hemispherical(cubemapCameras));

	 //create text label for displaying svf value
	 osgText::Text* text;
	 osg::ref_ptr<osg::Camera> hudCamera = SVFComputeTools::createHUDText(text, osg::Vec4(0.0f, 0.0f, 1.0f, 1.0f));
	 root->addChild(hudCamera.get());

	 //create an image buffer for screen capture
	 osg::ref_ptr<osg::Image> screenshotImg = new osg::Image;
	 screenshotImg->allocateImage(1024, 1024, 1, GL_RGB, GL_UNSIGNED_BYTE);
	 viewer->getCamera()->attach(osg::Camera::COLOR_BUFFER, screenshotImg.get());


	 osg::ref_ptr<osgGA::CameraManipulator> manip = new osgGA::TrackballManipulator;
	 viewer->setCameraManipulator(manip.get());
	 root->getOrCreateStateSet()->setMode(GL_LIGHTING, osg::StateAttribute::OFF | osg::StateAttribute::OVERRIDE);
	 viewer->setSceneData(root.get());
	 viewer->setThreadingModel(osgViewer::ViewerBase::ThreadingModel::ThreadPerContext);

	// add the thread model handler
	 viewer->addEventHandler(new osgViewer::ThreadingHandler);

	// add the window size toggle handler
	 viewer->addEventHandler(new osgViewer::WindowSizeHandler);

	// add the stats handler
	 viewer->addEventHandler(new osgViewer::StatsHandler);

	// add the LOD Scale handler
	 viewer->addEventHandler(new osgViewer::LODScaleHandler);

	VGEDatabasePager* databasePager = new VGEDatabasePager;
	databasePager->resume();
	viewer->getScene()->setDatabasePager(databasePager);
	std::vector<osg::Vec3d> normals;
	std::vector<osg::Vec3d> points;

	GDALAllRegister();
	//read in a point set from a shapefile. The points were interactively collected in the 3D city model using the PointPicker tool.
	ShapeFile shp("./data/street_points.shp");
	OGRFeature *poFeature;
	shp.poLayer->ResetReading();
	int pointid = 0;
	//write out SVF results
	ofstream ofssvf;
	ofssvf.open("./street_points_svf.txt");

	while ((poFeature = shp.poLayer->GetNextFeature()) != NULL)
	{

		OGRPoint* ogrp = (OGRPoint*)poFeature->GetGeometryRef();
		double x = poFeature->GetFieldAsDouble("x");
		double y = poFeature->GetFieldAsDouble("y");
		double z = poFeature->GetFieldAsDouble("z");
		double nx = poFeature->GetFieldAsDouble("nx");
		double ny = poFeature->GetFieldAsDouble("ny");
		double nz = poFeature->GetFieldAsDouble("nz");

		osg::Vec3d pos(x, y, z);
		osg::Vec3d normal(nx, ny, nz);
		normals.push_back(normal);
		points.push_back(pos);
		OGRFeature::DestroyFeature(poFeature);
		viewer->setCameraManipulator(NULL, false);
		VGEDatabasePager* databasePager = (VGEDatabasePager*)viewer->getDatabasePager();
		databasePager->pause();

		osg::Vec3d observer = pos;
		osg::Vec3d observerNormal = normal;
		observer = observer + observerNormal * 5;
		/*	g_pManip->setHomePosition(eye, center, up);
		g_pManip->home(0);*/
		for (size_t i = 0; i < cubemapCameras->getNumChildren(); i++)
		{
			CameraBuffer* cameraBuffer = (CameraBuffer*)cubemapCameras->getChild(i);
			cameraBuffer->_pos = observer;
		}
		cubemapCameras->setNodeMask(true);
		viewer->frame();
		databasePager->frame();
		while (databasePager->getFileRequestListSize() > 0)
		{
			viewer->frame();
			databasePager->frame();
		}
		viewer->frame();

		cubemapCameras->setNodeMask(false);
		databasePager->resume();
		viewer->setCameraManipulator(manip.get(), false);

		double svf = SVFComputeTools::calSVF(cubemap2fisheyeCamera->_image, false);
		printf("%f\n", svf);
		ofssvf << svf << std::endl;

		//std::stringstream ssFile;
		//ssFile << "svf_" << pointid << ".png";
		//write out fisheye image
		//osgDB::writeImageFile(*cubemap2fisheyeCamera->image, ssFile.str());

		printf("SVF=%f\n", svf);
		std::stringstream ss;
		ss << std::setprecision(3) << "SVF = " << svf;
		text->setText(ss.str());

		pointid++;
	}
	ofssvf.close();

	return 0;
}



