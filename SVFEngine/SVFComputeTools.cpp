#include "SVFComputeTools.h"
#include <osg/ComputeBoundsVisitor>
#include <iomanip>
CameraBuffer::CameraBuffer() 
	:osg::Camera()
{

}

void CameraBuffer::setupBuffer(int w, int h, osg::Vec3d dir, osg::Vec3d up, std::string name)
{
	_texture = new osg::Texture2D;
	_texture->setTextureSize(w, h);
	_texture->setResizeNonPowerOfTwoHint(false);
	_texture->setFilter(osg::Texture2D::MIN_FILTER, osg::Texture2D::LINEAR);
	_texture->setFilter(osg::Texture2D::MAG_FILTER, osg::Texture2D::LINEAR);
	_texture->setWrap(osg::Texture2D::WRAP_S, osg::Texture2D::REPEAT);
	_texture->setWrap(osg::Texture2D::WRAP_T, osg::Texture2D::REPEAT);
	//rtTexture->setDataVariance(osg::Object::DYNAMIC);
	_texture->setInternalFormat(GL_RGBA);
	_texture->setSourceFormat(GL_RGBA);
	_texture->setSourceType(GL_UNSIGNED_BYTE);
	_image = new osg::Image;
	_image->allocateImage(w, h, 1, GL_RGBA, GL_UNSIGNED_BYTE);
	_texture->setImage(_image.get());

	//camera = new osg::Camera;

	setClearMask(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	setClearColor(osg::Vec4(0, 0, 0, 0));
	//camera->setClearDepth(0);
	//camera->setClearColor(osg::Vec4(0.53f, 0.85f, 1.0f, 0.9f));				// Background
	setReferenceFrame(osg::Transform::ABSOLUTE_RF_INHERIT_VIEWPOINT);
	setViewport(0, 0, w, h);
	setRenderOrder(osg::Camera::PRE_RENDER);
	setRenderTargetImplementation(osg::Camera::FRAME_BUFFER_OBJECT);

	attach(osg::Camera::COLOR_BUFFER0, _texture.get());
	attach(osg::Camera::COLOR_BUFFER0, _image.get());

	_dir = dir;
	_up = up;
	_name = name;
}

void CameraBuffer::update()
{
	osg::Matrix localOffset;
	localOffset.makeLookAt(_pos, _pos + _dir * 100, _up);
	osg::Matrix viewMatrix = localOffset;
	setReferenceFrame(osg::Camera::ABSOLUTE_RF);
	float nearDist = 0.1;
	setProjectionMatrixAsFrustum(-nearDist, nearDist, -nearDist, nearDist, nearDist, 10000.0);
	setViewMatrix(viewMatrix);
	setClearColor(osg::Vec4(0, 0, 0, 0));
}

CameraBuffer * CameraBuffer::create(int w, int h, osg::Vec3d dir, osg::Vec3d up, std::string name)
{
	CameraBuffer* camera = new CameraBuffer;
	camera->setupBuffer(w, h, dir, up, name);
	return camera;
}

CameraBuffer * CameraBuffer::createSlave(int w, int h, osg::GraphicsContext * context)
{
	CameraBuffer* camera = new CameraBuffer;
	if (!context)
	{
		osg::ref_ptr<osg::GraphicsContext::Traits> traits = new osg::GraphicsContext::Traits;
		traits->x = 0;
		traits->y = 0;
		traits->width = w;
		traits->height = h;
		traits->windowDecoration = true; // window border etc.
		traits->doubleBuffer = true;
		traits->sharedContext = 0;
		traits->vsync = false;
		context = osg::GraphicsContext::createGraphicsContext(traits.get());
	}

	camera->setViewport(0, 0, w, h);
	camera->setGraphicsContext(context);
	camera->setupBuffer(w, h, osg::Vec3d(1, 0, 0), osg::Vec3d(1, 0, 0), "");

	return camera;
}

CameraCB::CameraCB(osg::Group* svfCameraBuffers)
{ 
	_cubemapCameras = svfCameraBuffers;
}
void CameraCB::operator()(osg::Node* node, osg::NodeVisitor* nv)
{

	if (nv->getVisitorType() == osg::NodeVisitor::CULL_VISITOR)
	{
		osgUtil::CullVisitor* cv = static_cast<osgUtil::CullVisitor*>(nv);
		for (size_t i = 0; i < _cubemapCameras->getNumChildren(); i++)
		{
			CameraBuffer* cameraBuffer = (CameraBuffer*)_cubemapCameras->getChild(i);
			cameraBuffer->update();
		}

	}
	traverse(node, nv);
}
SVFComputeTools::SVFComputeTools()
{
}

SVFComputeTools::~SVFComputeTools()
{
}

osg::Group * SVFComputeTools::createSVFCameras(osg::Node * city)
{
	osg::Group* cameras = new osg::Group;
	int w = 512;
	int h = 512;
	cameras->addChild(CameraBuffer::create(w, h, osg::Vec3(1, 0, 0), osg::Vec3(0, 0, 1), "POS_X"));
	cameras->addChild(CameraBuffer::create(w, h, osg::Vec3(-1, 0, 0), osg::Vec3(0, 0, 1), "NEG_X"));
	cameras->addChild(CameraBuffer::create(w, h, osg::Vec3(0, 1, 0), osg::Vec3(0, 0, 1), "POS_Y"));
	cameras->addChild(CameraBuffer::create(w, h, osg::Vec3(0, -1, 0), osg::Vec3(0, 0, 1), "NEG_Y"));
	cameras->addChild(CameraBuffer::create(w, h, osg::Vec3(0, 0, 1), osg::Vec3(0, -1, 0), "POS_Z"));
	cameras->addChild(CameraBuffer::create(w, h, osg::Vec3(0, 0, -1), osg::Vec3(0, 1, 0), "NEG_Z"));

	for (size_t i = 0; i < cameras->getNumChildren(); i++)
	{
		CameraBuffer* cameraBuffer = (CameraBuffer*)cameras->getChild(i);
		cameraBuffer->addChild(city);
	}

	osg::StateSet* ss = cameras->getOrCreateStateSet();
	CameraCB* cameraCB = new CameraCB(cameras);
	cameras->addCullCallback(cameraCB);

	return cameras;
}

osg::Node * SVFComputeTools::cubemap2hemispherical(osg::Group * svfCameraBuffers)
{
	//osg::TextureCubeMap* cubemap = SkyDome::loadCubeMapTextures("E:/OpenSceneGraphSVF/OpenSceneGraphSVF/images_WEIHAI", ".png");
	enum { POS_X, NEG_X, POS_Y, NEG_Y, POS_Z, NEG_Z };


	osg::Geode* geode = new osg::Geode;
	osg::ref_ptr<osg::Geometry> geom = new osg::Geometry();
	osg::Vec3Array* vertices = new osg::Vec3Array();
	osg::Vec3Array* normals = new osg::Vec3Array();
	vertices->push_back(osg::Vec3(-1, -1, -1));
	vertices->push_back(osg::Vec3(-1, 1, -1));
	vertices->push_back(osg::Vec3(1, -1, -1));
	vertices->push_back(osg::Vec3(1, -1, -1));
	vertices->push_back(osg::Vec3(-1, 1, -1));
	vertices->push_back(osg::Vec3(1, 1, -1));
	normals->push_back(osg::Vec3(0, 0, 1));
	geom->setVertexArray(vertices);
	geom->setNormalArray(normals);
	geom->setNormalBinding(osg::Geometry::BIND_OVERALL);
	geode->getOrCreateStateSet()->setMode(GL_CULL_FACE,
		osg::StateAttribute::OFF | osg::StateAttribute::OVERRIDE);
	//geom->setInitialBound(osg::BoundingBox(-1000000, -1000000, -1000000, 1000000, 1000000, 1000000));
	//geom->setNormalBinding(osg::Geometry::BIND_OVERALL);
	geom->setCullingActive(false);
	geode->setCullingActive(false);
	geom->addPrimitiveSet(new osg::DrawArrays(osg::PrimitiveSet::TRIANGLES, 0, 6));
	geode->addDrawable(geom.get());
	geode->getOrCreateStateSet()->setMode(GL_LIGHTING, osg::StateAttribute::OFF);

	osg::TextureCubeMap* cubeMap = new osg::TextureCubeMap;
	cubeMap->setInternalFormat(GL_RGBA);

	cubeMap->setFilter(osg::Texture::MIN_FILTER, osg::Texture::LINEAR_MIPMAP_LINEAR);
	cubeMap->setFilter(osg::Texture::MAG_FILTER, osg::Texture::LINEAR);
	cubeMap->setWrap(osg::Texture::WRAP_S, osg::Texture::CLAMP_TO_EDGE);
	cubeMap->setWrap(osg::Texture::WRAP_T, osg::Texture::CLAMP_TO_EDGE);

	cubeMap->setImage(osg::TextureCubeMap::NEGATIVE_X, ((CameraBuffer*)svfCameraBuffers->getChild(NEG_X))->_image.get());
	cubeMap->setImage(osg::TextureCubeMap::POSITIVE_X, ((CameraBuffer*)svfCameraBuffers->getChild(POS_X))->_image.get());
	cubeMap->setImage(osg::TextureCubeMap::NEGATIVE_Y, ((CameraBuffer*)svfCameraBuffers->getChild(POS_Z))->_image.get());
	cubeMap->setImage(osg::TextureCubeMap::POSITIVE_Y, ((CameraBuffer*)svfCameraBuffers->getChild(NEG_Z))->_image.get());
	cubeMap->setImage(osg::TextureCubeMap::NEGATIVE_Z, ((CameraBuffer*)svfCameraBuffers->getChild(NEG_Y))->_image.get());
	cubeMap->setImage(osg::TextureCubeMap::POSITIVE_Z, ((CameraBuffer*)svfCameraBuffers->getChild(POS_Y))->_image.get());
	geode->getOrCreateStateSet()->setTextureAttributeAndModes(0, cubeMap, osg::StateAttribute::ON);

	osg::Program* program = new osg::Program;

	char vertexSource[] =
		"void main(void)\n"
		"{\n"
		"   gl_TexCoord[0] = vec4(gl_Vertex.x,gl_Vertex.y,0,1.0);\n"
		"   gl_Position   = vec4(gl_Vertex.x,gl_Vertex.y,0,1);\n"
		"}\n";
	char fragmentSource[] =
		"uniform vec4 color;\n"
		"uniform float alpha;\n"
		"uniform samplerCube uEnvironmentMap;\n"
		"uniform float rotateAngle;\n"
		"\n"
		"vec3 spherical2Cartisian(float lon, float lat)\n"
		"{\n"
		"float theta = lon * 0.0174533;\n"
		"float phi =   lat* 0.0174533;\n"
		"return vec3(cos(phi)*cos(theta), cos(phi)*sin(theta), sin(phi));\n"
		"}\n"

		"vec2 rotate(vec2 uv,float angle)\n"
		"{\n"
		"    angle = angle * 0.0174533;\n"
		"    float sin_factor = sin(angle);\n"
		"    float cos_factor = cos(angle);\n"
		"    uv = (uv - 0.5) * mat2(cos_factor, sin_factor, -sin_factor, cos_factor);\n"
		"    uv += 0.5;\n"
		"    return uv;\n"
		"}\n"
		"void main(void) \n"
		"{\n"
		"    float radius = length(gl_TexCoord[0].xy);\n"
		"    vec3 tex = vec3(-gl_TexCoord[0].x, gl_TexCoord[0].y, -(1-radius));\n"
		"    vec2 uv = gl_TexCoord[0].xy; \n"
		"    uv = uv * 0.5 + 0.5;\n"
		"    uv = rotate(uv,rotateAngle);\n"
		"    gl_FragColor = textureCube( uEnvironmentMap, tex.xzy );\n"
		"    if(radius > 1)\n"
		"    {\n"
		"        gl_FragColor = vec4(0,0,0,0);\n"
		"    }\n"
		"    else if(gl_FragColor.a < 0.5 )\n"
		"    {\n"
		"        gl_FragColor = vec4(1,1,1,0.5);\n"
		"    }\n"
		"}\n";

	//geode->setCullCallback(new GeomCB);
	program->setName("sky_dome_shader");
	program->addShader(new osg::Shader(osg::Shader::VERTEX, vertexSource));
	program->addShader(new osg::Shader(osg::Shader::FRAGMENT, fragmentSource));
	geode->getOrCreateStateSet()->setAttributeAndModes(program, osg::StateAttribute::ON);
	geode->getOrCreateStateSet()->addUniform(new osg::Uniform("uEnvironmentMap", 0));
	geode->getOrCreateStateSet()->addUniform(new osg::Uniform("rotateAngle", 0.0f));
	geode->getOrCreateStateSet()->addUniform(new osg::Uniform("alpha", 0.0f));

	return geode;
}

osg::Node * SVFComputeTools::cubemap2hemisphericalHUD(osg::Group * svfCameraBuffers)
{
	//osg::TextureCubeMap* cubemap = SkyDome::loadCubeMapTextures("E:/OpenSceneGraphSVF/OpenSceneGraphSVF/images_WEIHAI", ".png");
	enum { POS_X, NEG_X, POS_Y, NEG_Y, POS_Z, NEG_Z };

	osg::TextureCubeMap* cubeMap = new osg::TextureCubeMap;
	cubeMap->setInternalFormat(GL_RGBA);

	cubeMap->setFilter(osg::Texture::MIN_FILTER, osg::Texture::LINEAR_MIPMAP_LINEAR);
	cubeMap->setFilter(osg::Texture::MAG_FILTER, osg::Texture::LINEAR);
	cubeMap->setWrap(osg::Texture::WRAP_S, osg::Texture::CLAMP_TO_EDGE);
	cubeMap->setWrap(osg::Texture::WRAP_T, osg::Texture::CLAMP_TO_EDGE);

	cubeMap->setImage(osg::TextureCubeMap::NEGATIVE_X, ((CameraBuffer*)svfCameraBuffers->getChild(NEG_X))->_image.get());
	cubeMap->setImage(osg::TextureCubeMap::POSITIVE_X, ((CameraBuffer*)svfCameraBuffers->getChild(POS_X))->_image.get());
	cubeMap->setImage(osg::TextureCubeMap::NEGATIVE_Y, ((CameraBuffer*)svfCameraBuffers->getChild(POS_Z))->_image.get());
	cubeMap->setImage(osg::TextureCubeMap::POSITIVE_Y, ((CameraBuffer*)svfCameraBuffers->getChild(NEG_Z))->_image.get());
	cubeMap->setImage(osg::TextureCubeMap::NEGATIVE_Z, ((CameraBuffer*)svfCameraBuffers->getChild(NEG_Y))->_image.get());
	cubeMap->setImage(osg::TextureCubeMap::POSITIVE_Z, ((CameraBuffer*)svfCameraBuffers->getChild(POS_Y))->_image.get());
	osg::Geode* geode = new osg::Geode;
	//createGeometry( );
	//SetupStateSet(cubemap);
	osg::ref_ptr<osg::Geometry> geom = new osg::Geometry();
	osg::Vec3Array* vertices = new osg::Vec3Array();
	osg::Vec3Array* normals = new osg::Vec3Array();
	vertices->push_back(osg::Vec3(-1, -1, -1));
	vertices->push_back(osg::Vec3(-1, 1, -1));
	vertices->push_back(osg::Vec3(1, -1, -1));
	vertices->push_back(osg::Vec3(1, -1, -1));
	vertices->push_back(osg::Vec3(-1, 1, -1));
	vertices->push_back(osg::Vec3(1, 1, -1));
	normals->push_back(osg::Vec3(0, 0, -1));
	geom->setVertexArray(vertices);
	geom->setNormalArray(normals);
	geom->setNormalBinding(osg::Geometry::BIND_OVERALL);
	geode->getOrCreateStateSet()->setMode(GL_CULL_FACE, osg::StateAttribute::OFF | osg::StateAttribute::OVERRIDE);
	//geom->setInitialBound(osg::BoundingBox(-1000000, -1000000, -1000000, 1000000, 1000000, 1000000));
	//geom->setNormalBinding(osg::Geometry::BIND_OVERALL);
	geom->setCullingActive(false);
	geode->setCullingActive(false);
	geom->addPrimitiveSet(new osg::DrawArrays(osg::PrimitiveSet::TRIANGLES, 0, 6));
	geode->addDrawable(geom.get());
	geode->getOrCreateStateSet()->setMode(GL_LIGHTING, osg::StateAttribute::OFF);
	geode->getOrCreateStateSet()->setTextureAttributeAndModes(0, cubeMap, osg::StateAttribute::ON);
	//geode->getOrCreateStateSet()->setTextureAttributeAndModes(1, g_pPanoTexture, osg::StateAttribute::ON);
	osg::Program* program = new osg::Program;

	char vertexSource[] =
		"void main(void)\n"
		"{\n"
		"   gl_TexCoord[0] = vec4(gl_Vertex.x,gl_Vertex.y,0,1.0);\n"
		"   //gl_Position   = vec4(gl_Vertex.x,gl_Vertex.y,0,1);\n"
		"   gl_Position   = vec4(gl_Vertex.x*0.5+0.5,gl_Vertex.y*0.5+0.5,0,1);\n"
		"}\n";
	char fragmentSource[] =
		"uniform vec4 color;\n"
		"uniform float alpha;\n"
		"uniform samplerCube uEnvironmentMap;\n"
		"uniform float rotateAngle;\n"
		"\n"
		"vec3 spherical2Cartisian(float lon, float lat)\n"
		"{\n"
		"float theta = lon * 0.0174533;\n"
		"float phi =   lat* 0.0174533;\n"
		"return vec3(cos(phi)*cos(theta), cos(phi)*sin(theta), sin(phi));\n"
		"}\n"

		"vec2 rotate(vec2 uv,float angle)\n"
		"{\n"
		"    angle = angle * 0.0174533;\n"
		"    float sin_factor = sin(angle);\n"
		"    float cos_factor = cos(angle);\n"
		"    uv = (uv - 0.5) * mat2(cos_factor, sin_factor, -sin_factor, cos_factor);\n"
		"    uv += 0.5;\n"
		"    return uv;\n"
		"}\n"
		"void main(void) \n"
		"{\n"
		"    float radius = length(gl_TexCoord[0].xy);\n"
		"    vec3 tex = vec3(-gl_TexCoord[0].x, gl_TexCoord[0].y, -(1-radius));\n"
		"    vec4 fisheye1 = textureCube( uEnvironmentMap, tex.xzy );\n"
		"    vec2 uv = gl_TexCoord[0].xy; \n"
		"    uv = uv * 0.5 + 0.5;\n"
		"    uv = rotate(uv,rotateAngle);\n"
		"    gl_FragColor = vec4(fisheye1.rgb,1);\n"
		"    if(radius > 1)\n"
		"    {\n"
		"        gl_FragColor = vec4(0,0,0,0.5);\n"
		"    }\n"
		"    else if(fisheye1.a < 0.5 )\n"
		"    {\n"
		"        gl_FragColor = vec4(0.529411765,0.807843137,0.921568627,0.5);\n"
		"    }\n"
		"}\n";

	//geode->setCullCallback(new GeomCB);
	program->setName("sky_dome_shader");
	program->addShader(new osg::Shader(osg::Shader::VERTEX, vertexSource));
	program->addShader(new osg::Shader(osg::Shader::FRAGMENT, fragmentSource));
	geode->getOrCreateStateSet()->setAttributeAndModes(program, osg::StateAttribute::ON);
	geode->getOrCreateStateSet()->addUniform(new osg::Uniform("uEnvironmentMap", 0));
	geode->getOrCreateStateSet()->addUniform(new osg::Uniform("rotateAngle", 0.0f));
	geode->getOrCreateStateSet()->addUniform(new osg::Uniform("alpha", 0.0f));

	geode->getOrCreateStateSet()->setRenderBinDetails(5000, "RenderBin");
	geode->getOrCreateStateSet()->setMode(GL_BLEND, osg::StateAttribute::ON);
	//geode->getOrCreateStateSet()->setRenderingHint(osg::StateSet::TRANSPARENT_BIN);
	geode->getOrCreateStateSet()->setMode(GL_DEPTH_TEST, osg::StateAttribute::OFF);
	//g_pPanoState = geode->getOrCreateStateSet();

	osg::Projection * HUDProjectionMatrix = new osg::Projection;

	HUDProjectionMatrix->setMatrix(osg::Matrix::ortho2D(0, 512, 0, 512));
	osg::MatrixTransform* HUDModelViewMatrix = new osg::MatrixTransform;
	HUDModelViewMatrix->setMatrix(osg::Matrix::translate(512, 512, 0));
	// above it in the scene graph:
	HUDModelViewMatrix->setReferenceFrame(osg::Transform::ABSOLUTE_RF);

	HUDProjectionMatrix->addChild(HUDModelViewMatrix);
	HUDModelViewMatrix->addChild(geode);

	return HUDProjectionMatrix;
	//return geode;
}

osg::Node * SVFComputeTools::createTextureRect(std::string texfile)
{
	osg::ref_ptr<osg::Texture2D> tex = new osg::Texture2D;
	tex->setWrap(osg::Texture2D::WRAP_S, osg::Texture2D::CLAMP_TO_BORDER);
	tex->setWrap(osg::Texture2D::WRAP_T, osg::Texture2D::CLAMP_TO_BORDER);
	tex->setImage(osgDB::readImageFile(texfile));

	osg::Geode* geode = new osg::Geode;
	//createGeometry( );
	//SetupStateSet(cubemap);
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
	geode->setCullingActive(false);
	geode->getOrCreateStateSet()->setMode(GL_CULL_FACE,
		osg::StateAttribute::OFF | osg::StateAttribute::OVERRIDE);
	geom->addPrimitiveSet(new osg::DrawArrays(osg::PrimitiveSet::TRIANGLES, 0, 6));
	geode->addDrawable(geom.get());
	geode->getOrCreateStateSet()->setMode(GL_LIGHTING, osg::StateAttribute::OFF);
	geode->getOrCreateStateSet()->setTextureAttributeAndModes(0, tex.get(), osg::StateAttribute::ON);

	osg::Program* program = new osg::Program;

	char vertexSource[] =
		"void main(void)\n"
		"{\n"
		"   gl_TexCoord[0] = vec4(gl_Vertex.x,gl_Vertex.y,0,1.0);\n"
		"   //gl_Position   = vec4(gl_Vertex.x*0.5-0.5,gl_Vertex.y*0.5+0.5,0,1);\n"
		"   gl_Position   = vec4(gl_Vertex.x*0.5+0.5,gl_Vertex.y*0.5+0.5,0,1);\n"
		"}\n";
	char fragmentSource[] =
		"uniform sampler2D texture0;\n"
		"uniform float rotateAngle;\n"
		"vec2 rotate(vec2 uv,float angle)\n"
		"{\n"
		"    angle = angle * 0.0174533;\n"
		"    float sin_factor = sin(angle);\n"
		"    float cos_factor = cos(angle);\n"
		"    uv = (uv - 0.5) * mat2(cos_factor, sin_factor, -sin_factor, cos_factor);\n"
		"    uv += 0.5;\n"
		"    return uv;\n"
		"}\n"
		"void main(void) \n"
		"{\n"
		"    vec2 uv = gl_TexCoord[0].xy; \n"
		"    uv = uv * 0.5 + 0.5;\n"
		"    uv = rotate(uv,rotateAngle);\n"
		"    vec4 color =   texture2D( texture0, uv );\n"
		"    gl_FragColor = vec4(color.rgb,0.5);\n"
		"}\n";
	program->setName("sky_dome_shader");
	program->addShader(new osg::Shader(osg::Shader::VERTEX, vertexSource));
	program->addShader(new osg::Shader(osg::Shader::FRAGMENT, fragmentSource));
	geode->getOrCreateStateSet()->setAttributeAndModes(program, osg::StateAttribute::ON);
	geode->getOrCreateStateSet()->addUniform(new osg::Uniform("texture0", 0));
	geode->getOrCreateStateSet()->addUniform(new osg::Uniform("rotateAngle", 0.0f));

	//geode->getOrCreateStateSet()->setRenderBinDetails(100000, "RenderBin");
	geode->getOrCreateStateSet()->setMode(GL_BLEND, osg::StateAttribute::OFF);
	geode->getOrCreateStateSet()->setRenderingHint(osg::StateSet::OPAQUE_BIN);
	//g_pPanoState = geode->getOrCreateStateSet();
	return geode;
}

double SVFComputeTools::calSVF(osg::Image * img, bool applyLambert)
{
	unsigned int skypixels = 0;
	unsigned int nonskypixels = 0;
	unsigned char* data = img->data();
	unsigned int numpixels = img->s() * img->t();
	unsigned int ncols = img->s();
	unsigned int nrows = img->t();
	if (ncols != nrows)
		return 0;
	double resol = 1.0 / nrows;
	double totalarea = 0;
	double skyarea = 0;
	double y = resol * 0.5 - 0.5;
	for (unsigned int row = 0; row < nrows; row++)
	{
		y += resol;
		double x = resol * 0.5 - 0.5;
		for (unsigned int col = 0; col < ncols; col++)
		{
			unsigned char a = data[3];
			if (a == 0) {
				x += resol;
				data += 4;
				continue;//outside
			}
			double zenithD = sqrt(x*x + y*y) * 90.0;//in degrees
			if (zenithD <= 0.000000001)
				zenithD = 0.000000001;
			double zenithR = zenithD * 3.1415926 / 180.0;
			double wproj = sin(zenithR) / (zenithD / 90);//weight for equal-areal projection
			if (applyLambert)
			{
				wproj = wproj * cos(zenithR);
			}
			totalarea += wproj;
			if (a < 250)
			{
				skypixels++;
				skyarea += wproj;
			}
			else
			{
				nonskypixels++;
			}
			x += resol;
			data += 4;
		}

	}
	double svf = skyarea / totalarea;
	return svf;
}
osg::Camera* SVFComputeTools::createHUDText(osgText::Text*& _text, osg::Vec4 color)
{
	// create a camera to set _up the projection and model view matrices, and the subgraph to draw in the HUD
	osg::Camera* camera = new osg::Camera;

	// set the projection matrix
	camera->setProjectionMatrix(osg::Matrix::ortho2D(0, 1024, 0, 1024));

	// set the view matrix
	camera->setReferenceFrame(osg::Transform::ABSOLUTE_RF);
	camera->setViewMatrix(osg::Matrix::identity());

	// only clear the depth buffer
	camera->setClearMask(GL_DEPTH_BUFFER_BIT);

	// draw subgraph after main camera view.
	camera->setRenderOrder(osg::Camera::POST_RENDER);

	// we don't want the camera to grab event focus from the viewers main camera(s).
	camera->setAllowEventFocus(false);



	// add to this camera a subgraph to render
	{

		osg::Geode* geode = new osg::Geode();

		//std::string timesFont("fonts/arial.ttf");

		// turn lighting off for the _text and disable depth test to ensure it's always ontop.
		osg::StateSet* stateset = geode->getOrCreateStateSet();
		stateset->setMode(GL_LIGHTING, osg::StateAttribute::OFF);

		osg::Vec3 position(650.0f, 768.0f, 0.0f);
		_text = new  osgText::Text;
		geode->addDrawable(_text);
		_text->setColor(color);
		//_text->setFont(timesFont);
		_text->setFont("times.ttf");
		_text->setPosition(position);
		_text->setText("");

		{
			osg::BoundingBox bb;
			for (unsigned int i = 0; i<geode->getNumDrawables(); ++i)
			{
				bb.expandBy(geode->getDrawable(i)->getBoundingBox());
			}

			osg::Geometry* geom = new osg::Geometry;

			osg::Vec3Array* vertices = new osg::Vec3Array;
			float depth = bb.zMin() - 0.1;
			vertices->push_back(osg::Vec3(bb.xMin(), bb.yMax(), depth));
			vertices->push_back(osg::Vec3(bb.xMin(), bb.yMin(), depth));
			vertices->push_back(osg::Vec3(bb.xMax(), bb.yMin(), depth));
			vertices->push_back(osg::Vec3(bb.xMax(), bb.yMax(), depth));
			geom->setVertexArray(vertices);

			osg::Vec3Array* normals = new osg::Vec3Array;
			normals->push_back(osg::Vec3(0.0f, 0.0f, 1.0f));
			geom->setNormalArray(normals, osg::Array::BIND_OVERALL);

			osg::Vec4Array* colors = new osg::Vec4Array;
			colors->push_back(osg::Vec4(0.0f, 0.0, 0.0f, 0.5f));
			geom->setColorArray(colors, osg::Array::BIND_OVERALL);

			geom->addPrimitiveSet(new osg::DrawArrays(GL_QUADS, 0, 4));

			osg::StateSet* stateset = geom->getOrCreateStateSet();
			stateset->setMode(GL_BLEND, osg::StateAttribute::ON);
			//stateset->setAttribute(new osg::PolygonOffset(1.0f,1.0f),osg::StateAttribute::ON);
			stateset->setRenderingHint(osg::StateSet::TRANSPARENT_BIN);

			geode->addDrawable(geom);
		}

		camera->addChild(geode);
	}

	return camera;
}

SkyViewFactorEventHandler::SkyViewFactorEventHandler(osg::Node* threeDModel, osg::Group * root, osg::ref_ptr<osgGA::CameraManipulator> manip, osgViewer::Viewer * viewer)
{
	_viewer = viewer;
	_manip = manip;
	_root = root;


	_renderGroup = new osg::Group;
	_root->addChild(_renderGroup.get());

	//create point label for highlighting intersection point
	_pointRenderer = new PointRenderer;
	_pointRenderer->create();
	_renderGroup->addChild(_pointRenderer.get());

	std::vector<osg::GraphicsContext*> contexts;
	_viewer->getContexts(contexts);
	//create a node to render a cubemap from a 3D position picked by mouse double-click
	_cubemapCameras = SVFComputeTools::createSVFCameras(threeDModel);
	_cubemapCameras->setNodeMask(false);
	root->addChild(_cubemapCameras.get());

	//create a HUD node to transform a cubemap into a fisheye view and then render onto the screen
	osg::ref_ptr<osg::Node> cubemap2fisheyeHUD = SVFComputeTools::cubemap2hemisphericalHUD(_cubemapCameras);
	root->addChild(cubemap2fisheyeHUD.get());

	//create a node to transform a cubemap into a fisheye view and then render onto an off-screen image for svf calculation
	_cubemap2fisheyeCamera = CameraBuffer::createSlave(512, 512, contexts[0]);
	osg::ref_ptr<osg::Node> cubemap2fisheye = SVFComputeTools::cubemap2hemispherical(_cubemapCameras);
	_cubemap2fisheyeCamera->addChild(cubemap2fisheye.get());
	_viewer->addSlave(_cubemap2fisheyeCamera.get(), false);
	_cubemap2fisheyeCamera->setCullingActive(false);

	//create text label for displaying svf value
	osg::ref_ptr<osg::Camera> hudCamera = SVFComputeTools::createHUDText(_text);
	_renderGroup->addChild(hudCamera.get());
}
SkyViewFactorEventHandler::~SkyViewFactorEventHandler()
{
	_root->removeChild(_renderGroup);
}
void SkyViewFactorEventHandler::printfVec3d(osg::Vec3d v)
{
	printf("%f,%f,%f\n", v.x(), v.y(), v.z());
}

void SkyViewFactorEventHandler::printfVec3(osg::Vec3 v)
{
	printf("%f,%f,%f\n", v.x(), v.y(), v.z());
}

void SkyViewFactorEventHandler::computeMouseIntersection(osgUtil::LineSegmentIntersector * ray)
{

	osg::Vec3d orieye, oricenter, oriup;
	_viewer->getCamera()->getViewMatrixAsLookAt(orieye, oricenter, oriup);
	if (ray->getIntersections().size() == 0)
		return;
	osg::Vec3d _dir = orieye - oricenter;
	_dir.normalize();
	osg::Vec3d curcenter = ray->getFirstIntersection().getWorldIntersectPoint();

	osg::Vec3d cureye = ray->getFirstIntersection().getWorldIntersectPoint() + _dir * 50;

	_viewer->setCameraManipulator(NULL, false);
	_viewer->frame();
	VGEDatabasePager* databasePager = dynamic_cast<VGEDatabasePager*>(_viewer->getDatabasePager());
	if (databasePager)
	{
		databasePager->pause();
		databasePager->frame();
		while (databasePager->getFileRequestListSize() > 0)
		{
			_viewer->frame();
			databasePager->frame();
		}
	}

	osgUtil::IntersectionVisitor visitor(ray);
	_viewer->getCamera()->accept(visitor);
	printfVec3(ray->getFirstIntersection().getWorldIntersectPoint());
	osg::Vec3d observer = ray->getFirstIntersection().getWorldIntersectPoint();
	osg::Vec3d observerNormal = ray->getFirstIntersection().getWorldIntersectNormal();
	observer = observer + observerNormal * 0.5;
	osg::Vec3d observerup = osg::Vec3(0, 0, 1);
	_pointRenderer->setPoint(observer);

	for (size_t i = 0; i < _cubemapCameras->getNumChildren(); i++)
	{
		CameraBuffer* cameraBuffer = (CameraBuffer*)_cubemapCameras->getChild(i);
		cameraBuffer->_pos = observer;
	}
	_cubemapCameras->setNodeMask(true);
	_viewer->frame();
	if (databasePager)
	{
		databasePager->frame();
		while (databasePager->getFileRequestListSize() > 0)
		{
			_viewer->frame();
			if (databasePager)
			{
				databasePager->frame();
			}
		}
		_viewer->frame();
	}
	else
	{
		for (size_t i = 0; i < 5; i++)
		{
			_viewer->frame();
		}
	}

	_cubemapCameras->setNodeMask(false);
	_viewer->getCamera()->setViewMatrixAsLookAt(orieye, oricenter, oriup);
	if (databasePager)
	{
		databasePager->resume();
	}
	osg::Matrix viewmat;
	viewmat.makeLookAt(orieye, oricenter, oriup);
	_viewer->setCameraManipulator(_manip.get(), false);
	_manip->setByInverseMatrix(viewmat);
	_viewer->getCamera()->getViewMatrixAsLookAt(orieye, oricenter, oriup);
	if (_cubemap2fisheyeCamera && _cubemap2fisheyeCamera.valid())
	{
		double svf = SVFComputeTools::calSVF(_cubemap2fisheyeCamera->_image, false);
		printf("SVF=%f\n", svf);
		std::stringstream ss;
		ss << std::setprecision(3) << "SVF = " << svf;
		_text->setText(ss.str());
	}
}

bool SkyViewFactorEventHandler::handle(const osgGA::GUIEventAdapter & ea, osgGA::GUIActionAdapter & aa)
{
	osgViewer::Viewer* viewer = dynamic_cast<osgViewer::Viewer*>(&aa);
	if (!viewer) 
		return false;
	osg::Vec3d orieye, oricenter, oriup;
	viewer->getCamera()->getViewMatrixAsLookAt(orieye, oricenter, oriup);
	osgViewer::Renderer *render = dynamic_cast<osgViewer::Renderer *>(aa.asView()->getCamera()->getRenderer());
	osgUtil::SceneView *sceneView = render->getSceneView(0);

	sceneView->getRenderInfo().getState()->setCheckForGLErrors(osg::State::NEVER_CHECK_GL_ERRORS);
	if (ea.getEventType() == osgGA::GUIEventAdapter::KEYDOWN) {
	}
	if (ea.getEventType() == osgGA::GUIEventAdapter::KEYUP) {
	}
	if (ea.getEventType() == osgGA::GUIEventAdapter::DOUBLECLICK) {
	}

	if (ea.getEventType() == osgGA::GUIEventAdapter::PUSH)
	{
		osg::ref_ptr<osgUtil::LineSegmentIntersector> ray = new osgUtil::LineSegmentIntersector(osgUtil::Intersector::PROJECTION, ea.getXnormalized(), ea.getYnormalized());
		osgUtil::IntersectionVisitor visitor(ray);
		viewer->getCamera()->accept(visitor);
		computeMouseIntersection(ray.get());
	}

	return false;
}