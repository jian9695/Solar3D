#include "Cubemap.h"
#include <Utils.h>

CubemapSurface::CubemapSurface(int width, int height, GLenum internalFormat, GLenum sourceFormat, GLenum sourceType, bool allocateImage,
	osg::Vec3d dir, osg::Vec3d up, std::string name) :
	RenderSurface(width, height, internalFormat, sourceFormat, sourceType, allocateImage),
	m_dir(dir), m_up(up)
{
	m_local2World = osg::Matrixd::identity();
	osg::Camera::setName(name);
	setComputeNearFarMode(osg::CullSettings::DO_NOT_COMPUTE_NEAR_FAR);
	setReferenceFrame(osg::Camera::ABSOLUTE_RF);
	setClearColor(osg::Vec4(0, 0, 0, 0));
}

void CubemapSurface::update()
{
	// local2World ajusts the view matrix for global view
	osg::Vec3d dir = m_dir * m_local2World;
	dir.normalize();
	osg::Vec3d up = m_up * m_local2World;
	_viewMatrix.makeLookAt(m_pos, m_pos + dir * 100, up);
	_projectionMatrix.makePerspective(90, 1.0, 0.01, 1000000);
	m_adjustedViewProjMatrix = _viewMatrix * _projectionMatrix;

	dir = m_dir;
	dir.normalize();
	up = m_up;
	osg::Matrixd viewMatrix;
	viewMatrix.makeLookAt(m_pos, m_pos + dir * 100, up);
	osg::Matrixd projectionMatrix;
	projectionMatrix.makePerspective(90, 1.0, 0.01, 1000000);
	m_viewProjMatrix = viewMatrix * projectionMatrix;

	dirtyBound();
}

Cubemap* Cubemap::create(int imageSize, osg::Node* scene)
{
	Cubemap* cubemap = new Cubemap;
	int w = imageSize;
	int h = imageSize;
	cubemap->addChild(new CubemapSurface(w, h, GL_RGBA, GL_RGBA, GL_UNSIGNED_BYTE, true, osg::Vec3d(1, 0, 0), osg::Vec3d(0, 0, 1), "POS_X"));
	cubemap->addChild(new CubemapSurface(w, h, GL_RGBA, GL_RGBA, GL_UNSIGNED_BYTE, true, osg::Vec3(-1, 0, 0), osg::Vec3(0, 0, 1), "NEG_X"));
	cubemap->addChild(new CubemapSurface(w, h, GL_RGBA, GL_RGBA, GL_UNSIGNED_BYTE, true, osg::Vec3(0, 1, 0), osg::Vec3(0, 0, 1), "POS_Y"));
	cubemap->addChild(new CubemapSurface(w, h, GL_RGBA, GL_RGBA, GL_UNSIGNED_BYTE, true, osg::Vec3(0, -1, 0), osg::Vec3(0, 0, 1), "NEG_Y"));
	cubemap->addChild(new CubemapSurface(w, h, GL_RGBA, GL_RGBA, GL_UNSIGNED_BYTE, true, osg::Vec3(0, 0, 1), osg::Vec3(0, -1, 0), "POS_Z"));
	cubemap->addChild(new CubemapSurface(w, h, GL_RGBA, GL_RGBA, GL_UNSIGNED_BYTE, true, osg::Vec3(0, 0, -1), osg::Vec3(0, 1, 0), "NEG_Z"));
	if (scene)
	{
		for (int i = 0; i < 6; i++)
		{
			cubemap->getFace((osg::TextureCubeMap::Face)i)->addChild(scene);
		}
	}
	return cubemap;
}

CubemapSurface* Cubemap::getFace(int face)
{
	if (face > getNumChildren())
		return nullptr;
	return dynamic_cast<CubemapSurface*>(getChild(face));
}

CubemapSurface* Cubemap::getFace(osg::TextureCubeMap::Face face)
{
	return getFace((int)face);
}

RenderSurface* Cubemap::toHemisphericalSurface()
{
	osg::ref_ptr<osg::TextureCubeMap> cubeMap = new osg::TextureCubeMap;
	cubeMap->setInternalFormat(GL_RGBA);
	cubeMap->setFilter(osg::Texture::MIN_FILTER, osg::Texture::NEAREST);
	cubeMap->setFilter(osg::Texture::MAG_FILTER, osg::Texture::NEAREST);
	cubeMap->setWrap(osg::Texture::WRAP_S, osg::Texture::CLAMP_TO_EDGE);
	cubeMap->setWrap(osg::Texture::WRAP_T, osg::Texture::CLAMP_TO_EDGE);

	cubeMap->setImage(osg::TextureCubeMap::NEGATIVE_X, getFace(osg::TextureCubeMap::NEGATIVE_X)->Image());
	cubeMap->setImage(osg::TextureCubeMap::POSITIVE_X, getFace(osg::TextureCubeMap::POSITIVE_X)->Image());
	cubeMap->setImage(osg::TextureCubeMap::NEGATIVE_Y, getFace(osg::TextureCubeMap::POSITIVE_Z)->Image());
	cubeMap->setImage(osg::TextureCubeMap::POSITIVE_Y, getFace(osg::TextureCubeMap::NEGATIVE_Z)->Image());
	cubeMap->setImage(osg::TextureCubeMap::NEGATIVE_Z, getFace(osg::TextureCubeMap::NEGATIVE_Y)->Image());
	cubeMap->setImage(osg::TextureCubeMap::POSITIVE_Z, getFace(osg::TextureCubeMap::POSITIVE_Y)->Image());
	char vertexSource[] =
		"void main(void)\n"
		"{\n"
		"   gl_TexCoord[0] = vec4(-gl_Vertex.x,gl_Vertex.y,0,1.0);\n"
		"   gl_Position   = vec4(-gl_Vertex.x,gl_Vertex.y,0,1);\n"
		"}\n";

	char fragmentSource[] =
		"uniform samplerCube uEnvironmentMap;\n"
		"const float PI = 3.1415926535897932384626433832795;\n"
		"const float PI_2 = 1.57079632679489661923;\n"
		"const float degree2radian = 0.0174533;\n"
		"const float radian2degree = 57.2958;\n"

		"float calAzimuth(float x, float y)\n"
		"{\n"
		"  float x2 = 0.0;\n"
		"  float y2 = 1.0;\n"
		"  float dot = x * x2 + y * y2;      //# dot product\n"
		"  float det = x * y2 - y * x2;      //# determinant\n"
		"  float angle = atan(det, dot) * radian2degree;  //# atan2(y, x) or atan2(sin, cos)\n"
		"  if (angle < 0)\n"
		"  angle += 360;\n"
		"  return angle;\n"
		"}\n"
		"vec3 cartesian2hemispherical(vec2 xy)\n"
		"{\n"
		"  float rho = length(xy);\n"
		"  float x = xy.x;\n"
		"  float y = xy.y;\n"
		"  float azimuth = calAzimuth(x, y);\n"
		"  float alt = (1.0 - rho) * 90.0;\n"
		"  float z = cos((90.0 - alt)*degree2radian);\n"
		"  float projectedLenghOnXY = cos(alt*degree2radian);\n"
		"  y = projectedLenghOnXY * cos(azimuth*degree2radian);\n"
		"  x = projectedLenghOnXY * cos((90 - azimuth)*degree2radian);\n"
		"  return normalize(vec3(x, y, z));\n"
		"}\n"
		"void main(void) \n"
		"{\n"
		"  float radius = length(gl_TexCoord[0].xy);\n"
		"  //vec3 dir = normalize(vec3(gl_TexCoord[0].x, -(1-radius), gl_TexCoord[0].y));//a close approximation of the solar vector\n"
		"  //vec4 rgba = textureCube(uEnvironmentMap, dir);\n"
		"  vec3 dir = cartesian2hemispherical(gl_TexCoord[0].xy);\n"
		"  vec4 rgba = textureCube( uEnvironmentMap, vec3(dir.x, -dir.z, dir.y));\n"
		"  if(radius > 1)\n"
		"  {\n"
		"     rgba = vec4(0.0,0.0,0.0,0.0);\n"
		"     gl_FragColor = rgba;\n"
		"     return;\n"
		"  }\n"
		"  else if(rgba.a < 0.5)\n"
		"  {\n"
		"     rgba = vec4(0, 0.5, 1, 0.65);\n"
		"  }\n"
		"  else\n"
		"  {\n"
		"     rgba = vec4(rgba.rgb,1);\n"
		"  }\n"
		"  //rgba = vec4(dir*0.5+vec3(0.5),1);//check the solar vectors\n"
		"  gl_FragColor = rgba;\n"
		"}\n";

	OverlayRenderSurface* fisheye = new OverlayRenderSurface(512, 512, GL_RGBA, GL_RGBA, GL_UNSIGNED_BYTE, true);
	fisheye->overlay()->setProgramName("fisheye");
	fisheye->overlay()->setTextureLayer(cubeMap.get(), 0);
	fisheye->overlay()->SetVertexShader(vertexSource);
	fisheye->overlay()->SetFragmentShader(fragmentSource);
	fisheye->getOrCreateStateSet()->addUniform(new osg::Uniform("uEnvironmentMap", 0));
	return fisheye;
}

osg::Image* Cubemap::toHemisphericalImage(int width, int height)
{
	osg::Image* fisheye = new osg::Image;
	fisheye->allocateImage(width, height, 1, GL_RGBA, GL_UNSIGNED_BYTE);
	double yresol = 1.0 / height;
	double xresol = 1.0 / width;
	ColorUB4* data = (ColorUB4*)fisheye->data();
	for (int row = 0; row < height; row++)
	{
		double y = (row / (height - 1.0) - 0.5) * 2.0;
		for (int col = 0; col < width; col++)
		{
			double x = (col / (width - 1.0) - 0.5) * 2.0;
			double radius = sqrt(x * x + y * y);
			if (radius > 1.0)
			{
				*data++ = ColorUB4(osg::Vec4(0, 0, 1, 0.3));
				continue;
			}
			double azimuth = Utils::calAzimuthAngle(x, y);
			double alt = (1.0 - radius) * 90.0;
			osg::Vec3 dir = Utils::solarAngle2Vector(alt, azimuth);
			CubemapSurface* face = getFace(alt, azimuth);
			osg::Image* faceImage = face->Image();
			osg::Vec3d sundir = Utils::solarAngle2Vector(alt, azimuth);
			osg::Vec3d targetPos = face->m_pos + sundir * 10000;
			osg::Vec3d screenPos = targetPos * face->m_adjustedViewProjMatrix;
			double u = (screenPos.x() + 1.0) * 0.5;
			double v = (screenPos.y() + 1.0) * 0.5;
			osg::Vec4 color = faceImage->getColor(osg::Vec2(u, v));
			*data++ = ColorUB4(color);
		}
	}
	return fisheye;
}

osg::TextureCubeMap::Face Cubemap::getFaceNum(float alt, float azimuth)
{
	if (alt > 45)
		return osg::TextureCubeMap::POSITIVE_Z;
	else if (alt < -45)
		return osg::TextureCubeMap::NEGATIVE_Z;
	if (azimuth < 45)
		return  osg::TextureCubeMap::POSITIVE_Y;
	if (azimuth < 135)
		return  osg::TextureCubeMap::POSITIVE_X;
	if (azimuth < 225)
		return  osg::TextureCubeMap::NEGATIVE_Y;
	if (azimuth < 315)
		return  osg::TextureCubeMap::NEGATIVE_X;
	return  osg::TextureCubeMap::POSITIVE_Y;
}

CubemapSurface* Cubemap::getFace(float alt, float azimuth)
{
	osg::TextureCubeMap::Face face = getFaceNum(alt, azimuth);
	return getFace(face);
}

Cubemap::Cubemap()
	: osg::Group()
{
	//getOrCreateStateSet()->setMode(GL_BLEND, osg::StateAttribute::OFF | osg::StateAttribute::OVERRIDE);
	//getOrCreateStateSet()->setMode(GL_CULL_FACE, osg::StateAttribute::OFF | osg::StateAttribute::OVERRIDE);
	static char vertexSource[] =
		"void main(void)\n"
		"{\n"
		"   gl_TexCoord[0] = gl_MultiTexCoord0;\n"
		"   gl_TexCoord[1] = gl_MultiTexCoord1;\n"
		"   gl_TexCoord[2] = gl_MultiTexCoord2;\n"
		"   gl_TexCoord[3] = gl_MultiTexCoord3;\n"
		"   gl_Position = gl_ModelViewProjectionMatrix * gl_Vertex;\n"
		"}\n";
	static char fragmentSource[] =
		"void main(void) \n"
		"{\n"
		"    gl_FragColor = vec4(1,1,0,1);\n"
		"}\n";
	m_programBinder.setVertexShader(vertexSource);
	//m_programBinder.setFragmentShader(fragmentSource);
	m_programBinder.initialize("cubeProgram", getOrCreateStateSet(), true);
}

bool Cubemap::isShadowed(double alt, double azimuth)
{
	CubemapSurface* face = getFace(alt, azimuth);
	osg::Image* faceImage = face->Image();
	osg::Vec3d sundir = Utils::solarAngle2Vector(alt, azimuth);
	osg::Vec3d targetPos = face->m_pos + sundir * 10000;
	osg::Vec3d screenPos = targetPos * face->m_viewProjMatrix;
	double u = (screenPos.x() + 1.0) * 0.5;
	double v = (screenPos.y() + 1.0) * 0.5;
	osg::Vec4 color = faceImage->getColor(osg::Vec2(u, v));
	return color.a() > 0.95;
}

void Cubemap::setWallShaderOn(bool on)
{
	m_programBinder.attach(on,on);
}