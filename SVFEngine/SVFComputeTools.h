#pragma once
#ifdef _WIN32 || WIN32 
#include <Windows.h>
#endif
#include "osg/Camera"
#include "osg/Texture2D"
#include "osg/Geode"
#include <osg/MatrixTransform>
#include <osg/TextureCubeMap>
#include <osgGA/TrackballManipulator>
#include <osgGA/FlightManipulator>
#include <osgGA/DriveManipulator>
#include <osgGA/KeySwitchMatrixManipulator>
#include <osgGA/StateSetManipulator>
#include <osgGA/AnimationPathManipulator>
#include <osgGA/TerrainManipulator>
#include <osgGA/SphericalManipulator>
#include <osg/GLExtensions>
#include <osgViewer/Renderer>
#include <osgGA/TrackballManipulator>
#include <osgDB/WriteFile>
#include <osgViewer/ViewerEventHandlers>
#include <osg/ClampColor>
#include <osgDB/ReadFile>
#include "osg/LineWidth"
#include <osgSim/LineOfSight>
#include "SVFDatabasePager.h"
#include "PointRenderer.h"
#include <osgText/Font>
#include <osgText/Text>
#include <iomanip>

//a cubemap camera
class CameraBuffer : public osg::Camera
{
public:
	osg::ref_ptr<osg::Texture2D> _texture;
	osg::ref_ptr<osg::Image> _image;
	std::string _name;
	osg::Vec3d _pos;//position of camera
	osg::Vec3d _dir;//viewing direction of camera
	osg::Vec3d _up;//the _up vector for use in building a view matrix
	CameraBuffer();
	void setupBuffer(int w, int h, osg::Vec3d _dir, osg::Vec3d _up, std::string _name);
	void update();
	//w: width of camera buffer
	//h: height of camera buffer
	static CameraBuffer* create(int w, int h, osg::Vec3d dir, osg::Vec3d up, std::string name);
	static CameraBuffer* createSlave(int w, int h, osg::GraphicsContext* context = NULL);
};

//callback for updating cubemap camera matrix before rendering
class CameraCB : public osg::NodeCallback
{
public:
	osg::Group* _cubemapCameras;
	//_cubemapCameras is a group of cubemap cameras created from SVFComputeTools
	CameraCB(osg::Group* svfCameraBuffers);
	virtual void operator()(osg::Node* node, osg::NodeVisitor* nv);
};

class SVFComputeTools
{
public:
	SVFComputeTools();
	~SVFComputeTools();
	//create a group of cameras to render a cubemap set from a 3D position
	//it contains six cameras looking respectively in the six cubemap directions
	static osg::Group* createSVFCameras(osg::Node* city);
	//create node to convert a cubemap set into a fisheye view and then render onto an off-screen _image
	static osg::Node* cubemap2hemispherical(osg::Group* _cubemapCameras);
	//create node to convert a cubemap set into a fisheye view and then render onto the screen
	static osg::Node* cubemap2hemisphericalHUD(osg::Group* _cubemapCameras);
	static osg::Node* createTextureRect(std::string texfile);
	//calculate SVF from a fisheye _image
	//Lambert's cosine law will be applied when applyLambert = true
	static double calSVF(osg::Image* img, bool applyLambert = false);
	//create a _text node to display SVF value
	static osg::Camera* createHUDText(osgText::Text*& _text,osg::Vec4 color = osg::Vec4(0,0,0,1));
};
//class for handling interactive 3D picking, SVF calculation and result displaying
class SkyViewFactorEventHandler : public osgGA::GUIEventHandler
{
public:
	//_cubemapCameras: a group of cubemap cameras created from SVFComputeTools
	//_root: child nodes for showing the point lable and the fisheye HUD are inserted into this parent node
	SkyViewFactorEventHandler(osg::Node* threeDModel, osg::Group* root, osg::ref_ptr<osgGA::CameraManipulator> manip, osgViewer::Viewer* viewer);
	~SkyViewFactorEventHandler();
	bool handle(const osgGA::GUIEventAdapter& ea, osgGA::GUIActionAdapter& aa);
private:
	//compute the mouse-model intersection point; compute SVF at this point; update the fisheye HUD and _text labels
	void computeMouseIntersection(osgUtil::LineSegmentIntersector* ray);
	osg::ref_ptr<osg::Group> _cubemapCameras;
	osg::ref_ptr<osgGA::CameraManipulator> _manip;
	osg::Group* _root;
	osg::ref_ptr<PointRenderer> _pointRenderer;
	osg::ref_ptr<CameraBuffer> _cubemap2fisheyeCamera;
	osg::ref_ptr<osg::Image> _screenshotTextImg;
	osgViewer::Viewer* _viewer;
	osgText::Text* _text;
	void printfVec3d(osg::Vec3d v);
	void printfVec3(osg::Vec3 v);
	osg::ref_ptr<osg::Group> _renderGroup;
};

