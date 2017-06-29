#ifdef _WIN32 || WIN32 
#include <Windows.h>
#endif

#include "PointRenderer.h"
#include <osg/Point>
#include <osg/BlendFunc>
#include <osg/Texture2D>
#include <osg/PointSprite>
#include <osg/PolygonMode>
#include <osg/Geometry>

PointRenderer::PointRenderer()
{

}


PointRenderer::~PointRenderer()
{

}
void PointRenderer::create()
{
	osg::ref_ptr<osg::Program> program = new osg::Program;

	char vertexShaderSource[] =
		"void main(void)\n"
		"{\n"
		"   gl_Position    = gl_ModelViewProjectionMatrix *  gl_Vertex;\n"
		"}\n";
	char fragmentShaderSource[] =
		"void main(void) \n"
		"{\n"
		"     gl_FragColor = vec4(1,0,0,1);\n"
		"}\n";

	program->addShader(new osg::Shader(osg::Shader::FRAGMENT, fragmentShaderSource));
	program->addShader(new osg::Shader(osg::Shader::VERTEX, vertexShaderSource));
	osg::StateSet* ss = this->getOrCreateStateSet();
	ss->setAttribute(program.get(), osg::StateAttribute::ON | osg::StateAttribute::OVERRIDE);
	this->setCullingActive(false);
	osg::ref_ptr<osg::Point> point = new osg::Point(5.0);
	point->setDistanceAttenuation(osg::Vec3(0.0, 0.0000, 0.05f));
	ss->setAttribute(point.get());
	ss->setRenderBinDetails(6000, "RenderBin");
	//ss->setMode(GL_BLEND, osg::StateAttribute::ON);
	//geode->getOrCreateStateSet()->setRenderingHint(osg::StateSet::TRANSPARENT_BIN);
	ss->setMode(GL_DEPTH_TEST, osg::StateAttribute::OFF);
}

void PointRenderer::setPoint(osg::Vec3d point)
{
	removeDrawables(0, getNumDrawables());
	osg::ref_ptr<osg::Vec3Array> vertices = new osg::Vec3Array;
	osg::ref_ptr<osg::Geometry> polyGeom = new osg::Geometry();
	vertices->push_back(point);
	polyGeom->setVertexArray(vertices.get());
	polyGeom->addPrimitiveSet(new osg::DrawArrays(osg::PrimitiveSet::POINTS, 0, vertices->size()));
	addDrawable(polyGeom.get());
}
