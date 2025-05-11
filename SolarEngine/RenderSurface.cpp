#include "RenderSurface.h"
#include <osgDB/ReadFile>

ScreenOverlay::ScreenOverlay(const char* vertexShader, const char* fragmentShader)
{
	osg::ref_ptr<osg::Geometry> geom = new osg::Geometry();
	osg::Vec3Array* vertices = new osg::Vec3Array();
	osg::Vec3Array* normals = new osg::Vec3Array();
	vertices->push_back(osg::Vec3(-1, -1, 1));
	vertices->push_back(osg::Vec3(-1, 1, 1));
	vertices->push_back(osg::Vec3(1, -1, 1));
	vertices->push_back(osg::Vec3(1, -1, 1));
	vertices->push_back(osg::Vec3(-1, 1, 1));
	vertices->push_back(osg::Vec3(1, 1, 1));
	normals->push_back(osg::Vec3(0, 0, 1));
	geom->setVertexArray(vertices);
	geom->setNormalArray(normals);
	geom->setNormalBinding(osg::Geometry::BIND_OVERALL);
	geom->setCullingActive(false);
	this->setCullingActive(false);
	this->getOrCreateStateSet()->setMode(GL_CULL_FACE,
		osg::StateAttribute::OFF | osg::StateAttribute::OVERRIDE);
	geom->addPrimitiveSet(new osg::DrawArrays(osg::PrimitiveSet::TRIANGLES, 0, 6));
	this->addDrawable(geom.get());
	this->getOrCreateStateSet()->setMode(GL_LIGHTING, osg::StateAttribute::OFF);

	m_programBinder.initialize("overlayProgram", this->getOrCreateStateSet());
	std::string vertexShaderSource = defaultVertexShader();
	std::string fragmentShaderSource = defaultFragmentShader();
	if (vertexShader)
		vertexShaderSource = vertexShader;
	if (fragmentShader)
		fragmentShaderSource = fragmentShader;
	m_programBinder.setVertexShader(vertexShaderSource);
	m_programBinder.setFragmentShader(fragmentShaderSource);

	this->getOrCreateStateSet()->addUniform(new osg::Uniform("texture0", 0));
}

ScreenOverlay::~ScreenOverlay()
{

}

void ScreenOverlay::setTextureLayer(osg::Texture* tex, int unit)
{
	getOrCreateStateSet()->setTextureAttributeAndModes(unit, tex, osg::StateAttribute::ON);
}