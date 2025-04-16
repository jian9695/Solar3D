#include "Utils.h"
#include "GrassSolar.h"
#include <osg/Geode>
#include <osg/Geometry>
#include <sstream>
#include <RenderSurface.h>
#include <osgDB/ReadFile>

//Angle between vector (x,y) and the positive Y axis (0,1) with origin at (0,0)
double Utils::calAzimuthAngle(double x, double y)
{
	double x2 = 0.0;
	double y2 = 1.0;
	double dot = x * x2 + y * y2;      //# dot product
	double det = x * y2 - y * x2;      //# determinant
	double angle = osg::RadiansToDegrees(atan2(det, dot));  //# atan2(y, x) or atan2(sin, cos)
	if (angle < 0)
		angle += 360;
	return angle;
}

//The aspect categories represent the number degrees of east and they increase counterclockwise: 90deg is North, 180 is West, 270 is South 360 is East.
//The aspect value 0 is used to indicate undefined aspect in flat areas with slope=0. 
//
//The slope output raster map contains slope values, 
//stated in degrees of inclination from the horizontal if format=degrees option (the default) is chosen, 
//and in percent rise if format=percent option is chosen. Category and color table files are generated. 
//***********N90

//*******W180    E360 

//***********S270

//***********N0

//*******W270    E90 

//***********S180
//tmpval.elev = 0;
//if (tmpval.aspect != 0.) {
//	if (tmpval.aspect < 90.)
//		tmpval.aspect = 90. - tmpval.aspect;
//	else
//		tmpval.aspect = 450. - tmpval.aspect;
//}

double Utils::calculateAspect(osg::Vec3d normal)
{
	double aspect = 0;
	if (normal.x() == 0 && normal.y() == 0)
		return aspect;
	osg::Vec3d normalxy = normal;
	normalxy.normalize();
	normalxy.z() = 0;
	normalxy.normalize();
	double cosa = acos(normalxy * osg::Vec3d(1, 0, 0));
	aspect = osg::RadiansToDegrees(cosa);
	if (normal.y() < 0)
		aspect = 360 - aspect;
	return aspect;
}

double Utils::calculateSlope(osg::Vec3d normal)
{
	double cosa = acos(normal * osg::Vec3d(0, 0, 1));
	double slope = osg::RadiansToDegrees(cosa);
	return slope;
}

osg::Vec3d Utils::solarAngle2Vector(double alt, double azimuth)
{
	osg::Vec3d lightDir;
	lightDir.z() = cos(osg::DegreesToRadians(90.0 - alt));
	double projectedLenghOnXY = cos(osg::DegreesToRadians(alt));
	lightDir.y() = projectedLenghOnXY * cos(osg::DegreesToRadians(azimuth));
	//lightDir.x() = sqrt((projectedLenghOnXY * projectedLenghOnXY) - lightDir.y() * lightDir.y());
	lightDir.x() = projectedLenghOnXY * cos(osg::DegreesToRadians(90 - azimuth));
	lightDir.normalize();
	return lightDir;
}

std::vector<osg::Vec3d> Utils::sunVector2LightDir(std::vector<SunVector>& sunvectors)
{
	std::vector<osg::Vec3d> vsuns;
	for (int i = 0; i < sunvectors.size(); i++)
	{
		vsuns.push_back(Utils::solarAngle2Vector(sunvectors[i].m_alt, sunvectors[i].m_azimuth));
	}
	return vsuns;
}

double Utils::calSVF(osg::Image* img, bool applyLambert)
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
			double zenithD = sqrt(x * x + y * y) * 90.0;//in degrees
			if (zenithD <= 0.000000001)
				zenithD = 0.000000001;
			double zenithR = zenithD * 3.1415926 / 180.0;
			double wproj = sin(zenithR) / (zenithD / 90);//weight for equal-areal projection
			if (applyLambert)
			{
				wproj = wproj * cos(zenithR);
			}
			totalarea += wproj;
			if (a < 240)
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

SolarRadiation Utils::calSolar(osg::Image* img, SolarParam* solarParam, osg::Vec3d pos, osg::Vec3d normal, osg::Node* sceneNode)
{
	SolarRadiation annualRad;
	annualRad.Zero();
	std::vector<SolarRadiation> dailyRads;
	SolarParam param = *solarParam;
	param.m_slope = Utils::calculateSlope(normal);
	param.m_aspect = Utils::calculateAspect(normal);

	int startDay = param.m_startDay;
	int endDay = param.m_endDay;
	if (endDay > startDay)
		endDay = startDay;
	int lastSteps = 0;
	GrassSolar rsun;
	for (int day = startDay; day <= endDay; day++)
	{
		param.m_startDay = day;
		std::vector<SunVector> sunVecs = rsun.getSunVectors(param);
		if (lastSteps != sunVecs.size())
		{
			if (param.m_shadowInfo)
				delete[] param.m_shadowInfo;
			param.m_shadowInfo = new bool[sunVecs.size()];
		}
		for (int n = 0; n < sunVecs.size(); n++)
		{
			SunVector sunVec = sunVecs[n];
			double radius = (90.0 - sunVec.m_alt) / 90.0 * 0.5;
			double theta = sunVec.m_azimuth - 90;
			if (theta < 0)
				theta += 360;
			theta = osg::DegreesToRadians(theta);
			double x = radius * cos(theta);
			double y = radius * sin(theta);
			x += 0.5;
			y += 0.5;
			osg::Vec4 color = img->getColor(osg::Vec2(x, y));
			param.m_shadowInfo[n] = color.a() > 0.7 ? true : false;
		}

		param.m_day = day;
		SolarRadiation dailyRad = rsun.calculateSolarRadiation(param);
		dailyRads.push_back(dailyRad);
		annualRad = annualRad + dailyRad;
	}
	if (param.m_shadowInfo)
		delete[] param.m_shadowInfo;
	return annualRad;
}

std::string Utils::value2String(float value, int precision)
{
	std::stringstream buf;
	buf.precision(precision);
	buf << std::fixed << value;
	return buf.str();
}

osg::Vec3 Utils::worldPosFromDepth(float depth, const osg::Matrixd& projMatrixInv, const osg::Matrixd& viewMatrixInv, const osg::Vec2& uv)
{
	float z = depth * 2.0 - 1.0;

	osg::Vec4 clipSpacePosition(uv.x() * 2.0 - 1.0, uv.y() * 2.0 - 1.0, z, 1.0);
	osg::Vec4 viewSpacePosition = clipSpacePosition * projMatrixInv;

	// Perspective division
	viewSpacePosition /= viewSpacePosition.w();
	osg::Vec4 worldSpacePosition = viewSpacePosition * viewMatrixInv;

	return osg::Vec3(worldSpacePosition.x(), worldSpacePosition.y(), worldSpacePosition.z());
}

std::string Utils::padRight(std::string str, const size_t num, const char paddingChar)
{
	if (num > str.size())
		str.insert(str.size(), num - str.size(), paddingChar);
	return str;
}

bool Utils::nodeHasNormals(osg::Node* node)
{
	osg::Geode* geode = dynamic_cast<osg::Geode*>(node);
	osg::Group* group = dynamic_cast<osg::Group*>(node);
	if (geode)
	{
		for (int n = 0; n < geode->getNumDrawables(); n++)
		{
			osg::Geometry* geom = dynamic_cast<osg::Geometry*>(geode->getChild(n));
			if (!geom)
				continue;
			if (geom->getNormalArray())
				return true;
		}
	}

	if (group)
	{
		for (int n = 0; n < group->getNumChildren(); n++)
		{
			if (nodeHasNormals(group->getChild(n)))
				return true;
		}
	}

	return false;
}

CustomControls::CustomImageControl* Utils::createCompass(CustomControls::ControlCanvas* cs, int viewWidth, int viewHeight)
{
	char fragmentSource[] =
		"uniform sampler2D texture0;\n"
		"uniform float rotateAngle;\n"
		"vec2 rotateXY(vec2 xy, float rotation)\n"
		"{\n"
		"   float mid = 0.5;\n"
		"   float x = cos(rotation) * (xy.x - mid) + sin(rotation) * (xy.y - mid) + mid;\n"
		"   float y = cos(rotation) * (xy.y - mid) - sin(rotation) * (xy.x - mid) + mid;\n"
		"   return vec2(x,y);\n"
		"}\n"
		"void main(void) \n"
		"{\n"
		"    vec4 color = texture2D(texture0, rotateXY(gl_TexCoord[0].xy, -rotateAngle * 0.0174533));\n"
		"    if(color.a < 0.5)\n"
		"      color = vec4(0.1,0.1,0.1,0.4);\n"
		"    else\n"
		"      color = color + vec4(0,0.3,0,0);\n"
		"    gl_FragColor = color;\n"
		"}\n";
	osg::ref_ptr<osg::Image> img = osgDB::readImageFile("./data/compass.png");
	osg::ref_ptr<osg::Texture2D> tex = new osg::Texture2D;
	tex->setFilter(osg::Texture::MIN_FILTER, osg::Texture::LINEAR);
	tex->setFilter(osg::Texture::MAG_FILTER, osg::Texture::LINEAR);
	tex->setImage(img.get());

	CustomControls::CustomImageControl* imageCtrl = new CustomControls::CustomImageControl;
	imageCtrl->setSize(128, 128);
	imageCtrl->setPosition(viewWidth - 128 - 10, 10);
	imageCtrl->setImage(img.get());
	imageCtrl->getOrCreateStateSet()->addUniform(new osg::Uniform("rotateAngle", 0.0f));

	ProgramBinder binder;
	binder.initialize("NorthArrowProgram", imageCtrl->getOrCreateStateSet());
	binder.setFragmentShader(fragmentSource);
	imageCtrl->getOrCreateStateSet()->setTextureAttributeAndModes(0, tex.get(), osg::StateAttribute::ON);
	cs->addChild(imageCtrl);
	return imageCtrl;
}

SolarParam Utils::createDefaultSolarParam()
{
	SolarParam param;
	param.m_aspect = 270;
	param.m_slope = 0;
	param.m_lon = -9999;
	param.m_lat = 37.5131;
	param.m_day = 183;
	param.m_time_step = 1;
	param.m_linke = 3.0;
	param.m_startDay = param.m_day;
	param.m_endDay = param.m_day;
	param.m_isSingleDay = true;
	param.m_isInstantaneous;
	param.m_elev = 0;
	return param;
}
