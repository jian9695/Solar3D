
#ifdef _WIN32 || WIN32 
#include <Windows.h>
#endif
#include "osgDB/WriteFile"
#include "osgDB/WriteFile"
#include "osg/ComputeBoundsVisitor"
#include "osgGA/StateSetManipulator"
#include "osgViewer/ViewerEventHandlers"
#include "osgUtil/Optimizer"
#include "osg/MatrixTransform"
#include "osgGA/TrackballManipulator"
#include "osg/Texture2D"
#include "osg/Image"
#include "osgDB/writeFile"
#include "osgDB/readFile"
#include "osg/ClampColor"
#include "osg/Depth"
#include "osgUtil/SmoothingVisitor"
#include "PointPicker.h"
#include <osg/ShapeDrawable>
#include "ShapeFile.h"
using namespace OpenThreads;

//create a ground plane in case a 3D city scene does not come with one
osg::Node* createRect(osg::BoundingBox bb, double baseHeight)
{
	osg::ref_ptr<osg::Geode> geode = new osg::Geode;
	osg::ref_ptr<osg::Program> program = new osg::Program;

	char vertexShaderSource[] =
		"void main(void)\n"
		"{\n"
		"   gl_Position    = gl_ModelViewProjectionMatrix *  gl_Vertex;\n"
		"}\n";
	char fragmentShaderSource[] =
		"void main(void) \n"
		"{\n"
		"     gl_FragColor = vec4(0.5,0.5,0.5,1);\n"
		"}\n";

	program->addShader(new osg::Shader(osg::Shader::FRAGMENT, fragmentShaderSource));
	program->addShader(new osg::Shader(osg::Shader::VERTEX, vertexShaderSource));
	osg::StateSet* ss = geode->getOrCreateStateSet();

	ss->setAttribute(program.get(), osg::StateAttribute::ON | osg::StateAttribute::OVERRIDE);

	geode->addDrawable(new osg::ShapeDrawable(new osg::Box(osg::Vec3(0, 0, 0), 1)));

	osg::MatrixTransform* mat = new osg::MatrixTransform;
	mat->setMatrix(
		osg::Matrix::scale(bb.xMax() - bb.xMin(), bb.yMax() - bb.yMin(), 0.00001)
		* osg::Matrix::translate(bb.center()));
	mat->addChild(geode.get());
	return mat;
}
//arguments: 
//1. path of 3D model file
//2. path of Shapefile to create or update
//3. base height of groud plane to create (optional)

//how to operate the tool:
//1. right mouse click to add a point
//2. press key DELETE to delete the last point
//3. press key B to update the Shapefile (remember to do this regularly, since results will not be saved in case of abnormal termination)
int main(int argc, char** argv)
{

	osg::ArgumentParser arguments(&argc, argv);
	osgViewer::Viewer viewer;
	//viewer.getScene()->setDatabasePager()
	// add the state manipulator
	viewer.addEventHandler(new osgGA::StateSetManipulator(viewer.getCamera()->getOrCreateStateSet()));

	// add the thread model handler
	viewer.addEventHandler(new osgViewer::ThreadingHandler);

	// add the window size toggle handler
	viewer.addEventHandler(new osgViewer::WindowSizeHandler);

	// add the stats handler
	viewer.addEventHandler(new osgViewer::StatsHandler);
	PointPicker* pointPicker = new PointPicker;
	viewer.addEventHandler(pointPicker);
	osg::ref_ptr<osg::Group> root = new osg::Group;

	std::string infile = argv[1];
	std::string inshpfile = argv[2];

	if (pointPicker->loadFromShapeFile(inshpfile.data())) {
		printf("file exists: %s\n", inshpfile.data());
	}
	pointPicker->m_filename = inshpfile;
	printf("%s\n", inshpfile.data());
	osg::ref_ptr<osg::Node> node;
	if (argc > 3)
	{
		double baseHeight = atof(argv[3]);
		osg::BoundingBox bb;
		std::string bbfile = infile + ".bb";
	
		node = osgDB::readNodeFile(infile);
		osg::ComputeBoundsVisitor cbv;
		node->accept(cbv);
		bb = cbv.getBoundingBox();
		
		bb.zMin() = bb.zMax() = baseHeight;
		osg::ref_ptr<osg::Node> ground = createRect(bb, baseHeight);
		root->addChild(ground.get());
	}
	if(!node.valid())
		node = osgDB::readNodeFile(infile);

	if (!node || !node.valid())
	{
		return 1;
	}


	root->addChild(node.get());


	root->addChild(pointPicker->PointGeode().get());
	viewer.setSceneData(root.get());

    viewer.run();
	exit(0);
	return 0;
}
