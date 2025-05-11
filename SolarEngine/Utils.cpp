#include "Utils.h"
#include <osg/Geode>
#include <osg/Geometry>
#include <sstream>
#include <RenderSurface.h>
#include <osgDB/ReadFile>
constexpr double M_PI = 3.1415926;
constexpr double DEG_TO_RAD = M_PI / 180.0;
constexpr double RAD_TO_DEG = 180.0 / M_PI;

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
	double cosa = acos(normalxy * osg::Vec3d(0, 1, 0));
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

void Utils::computeSurfaceAngles(const osg::Vec3d normal, double& tilt, double& azimuth)
{
	osg::Vec3d n = normal;
	n.normalize(); // Ensure normal is unit length

	// Compute tilt angle (angle from vertical Z-axis)
	tilt = std::acos(n.z()) * 180.0 / M_PI; // Convert to degrees

	// Compute azimuth angle (angle from Y-axis in the XY plane)
	azimuth = std::atan2(n.x(), n.y()) * 180.0 / M_PI; // Convert to degrees

	// Ensure azimuth is within [0, 360] range
	if (azimuth < 0)
		azimuth += 360.0;
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
	double surfaceTilt, surfaceAzimuth;
	Utils::computeSurfaceAngles(normal, surfaceTilt, surfaceAzimuth);
	param.m_slope = surfaceTilt;
	param.m_aspect = surfaceAzimuth;

	int startDay = param.m_startDay;
	int endDay = param.m_endDay;
	if (endDay > startDay)
		endDay = startDay;
	int lastSteps = 0;
	double linke = solarParam->m_linke;
	double elevation = solarParam->m_elev;
	double slope = param.m_slope;
	double aspect = param.m_aspect;
	double lon = param.m_lon;
	double lat = param.m_lat;

	for (int day = startDay; day <= endDay; day++)
	{
		param.m_startDay = day;
		std::vector<SunVector> sunVecs = Utils::getSunVectors(param);
		SolarRadiation dailyRad;
		dailyRad.Zero();
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
			bool isShadowed = color.a() > 0.7 ? true : false;
			SolarRadiation rad = Utils::calSolar(day, linke, elevation, sunVec.m_azimuth, 90.0 - sunVec.m_alt, aspect, slope, isShadowed);
			dailyRad = dailyRad + (rad * param.m_time_step);
		}
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
	param.m_time_step = 10.0 / 60.0;
	param.m_linke = 3.0;
	param.m_startDay = param.m_day;
	param.m_endDay = param.m_day;
	param.m_isSingleDay = true;
	param.m_isInstantaneous;
	param.m_elev = 0;
	return param;
}

double utcToLocalTime(double utc_time, double longitude) {
	// Calculate the time zone offset in hours
	double time_zone_offset = longitude / 15.0;

	// Convert UTC time to local time
	double local_time = utc_time + time_zone_offset;

	// Ensure local time is within 0-24 hours range
	if (local_time < 0) {
		local_time += 24.0;
	}
	else if (local_time >= 24) {
		local_time -= 24.0;
	}

	return local_time;
}

double localToUTC(double local_time, double longitude)
{
	// Calculate the time zone offset in hours
	double time_zone_offset = longitude / 15.0;

	// Convert Local Solar Time to UTC
	double utc_time = local_time - time_zone_offset;

	// Ensure UTC time is within 0-24 hours range
	if (utc_time < 0) {
		utc_time += 24.0;
	}
	else if (utc_time >= 24) {
		utc_time -= 24.0;
	}

	return utc_time;
}

// Function to calculate the equation of time (E) in minutes
double equation_of_time(int day_of_year)
{
	// Mean anomaly of the Sun (in radians)
	double B = (360.0 / 365.0) * (day_of_year - 81) * DEG_TO_RAD;
	// Equation of time (in minutes)
	return 9.87 * sin(2 * B) - 7.53 * cos(B) - 1.5 * sin(B);
}

// Function to calculate the solar declination (δ) in degrees
double solar_declination(int day_of_year)
{
	// Declination angle in radians
	double declination = -23.45 * cos((360.0 / 365.0) * (day_of_year + 10) * DEG_TO_RAD);
	return declination;
}

void Utils::solar_azimuth_angle(int day_of_year, double latitude, double longitude, double local_time, double& zenith, double& azimuth)
{
	// Calculate the solar declination
	double declination = solar_declination(day_of_year);

	// Calculate the equation of time
	double E = equation_of_time(day_of_year);
	double utc = localToUTC(local_time, longitude);
	double lons = -15 * (utc - 12 + E / 60.0);

	// Convert angles to radians
	double lat_rad = latitude * DEG_TO_RAD;
	double dec_rad = declination * DEG_TO_RAD;
	double ha_rad = (lons - longitude) * DEG_TO_RAD;// hour_angle* DEG_TO_RAD;

	// Calculate the components of the unit vector pointing toward the Sun
	double S_x = cos(dec_rad) * sin(ha_rad);
	double S_y = cos(lat_rad) * sin(dec_rad) - sin(lat_rad) * cos(dec_rad) * cos(ha_rad);
	double S_z = sin(lat_rad) * sin(dec_rad) + cos(lat_rad) * cos(dec_rad) * cos(ha_rad);
	zenith = acos(S_z) * RAD_TO_DEG;
	// Calculate the solar azimuth angle using the atan2 function
	double azimuth_rad = atan2(S_x, S_y); // North-Clockwise Convention

	// Convert azimuth angle from radians to degrees
	azimuth = azimuth_rad * RAD_TO_DEG;

	// Normalize azimuth angle to the range [0, 360)
	if (azimuth < 0)
		azimuth += 360.0;
}

//The formula used to calculate the angle of incidence(𝜃θ) of a solar ray on a tilted surface is well - documented in solar geometry and photovoltaic system modeling.A primary reference for this formula is :
//Duffie, J.A., & Beckman, W.A. (2013).Solar Engineering of Thermal Processes(4th ed.).Wiley
// Solar zenith angle(angle between the sun and the vertical)
// Solar azimuth angle(angle of the sun's projection on the horizontal plane)
// Surface tilt angle(angle between the surface and the horizontal plane)
// Surface azimuth angle(direction of the tilted surface relative to north)
double calculate_incidence_angle(double surface_tilt, double surface_azimuth, double solar_zenith, double solar_azimuth)
{
	// Convert degrees to radians
	double beta_rad = surface_tilt * DEG_TO_RAD;
	double gamma_rad = surface_azimuth * DEG_TO_RAD;
	double Z_rad = solar_zenith * DEG_TO_RAD;
	double gamma_s_rad = solar_azimuth * DEG_TO_RAD;

	// Compute the cosine of the incidence angle
	double cos_theta = cos(Z_rad) * cos(beta_rad) +
		sin(Z_rad) * sin(beta_rad) * cos(gamma_s_rad - gamma_rad);

	// Ensure cos_theta is in a valid range
	cos_theta = osg::maximum(-1.0, osg::minimum(1.0, cos_theta));

	// Compute and return the incidence angle in degrees
	return acos(cos_theta) / DEG_TO_RAD;
}

double getIrradianceExtraTerrestrial(int dayNum) {

	double j = (2. * M_PI) * dayNum / 365.25; //day angle in radians

	//correction factor due to sun-earth distance varies across the year
	double G0 = 1 + 0.03344 * cos(j - 0.048869);
	//extra terrestrial calculation

	return (1367. * G0); //calculating in WM-2 unit
}

double getDirectIrradianceNormal(int dayofyear, double zenith, double elevation, double linke)
{
	//double dni = 1.353 * pow(0.7, pow(1.0 / cos(zenith * DEG_TO_RAD), 0.678));

	double sunAlt = (90 - zenith) * DEG_TO_RAD;
	if (sunAlt < 0)
		return 0;

	double M = float(1. / (sin(sunAlt) + 0.50572 * pow(6.07995 + sunAlt, -1.6364)));

	double opticalAirMass, airMass2Linke, rayl, br;
	double drefract, temp1, temp2, h0refract;
	double elevationCorr;

	/* FIXME: please document all coefficients */
	elevationCorr = exp(-elevation / 8434.5);
	temp1 = 0.1594 + sunAlt * (1.123 + 0.065656 * sunAlt);
	temp2 = 1. + sunAlt * (28.9344 + 277.3971 * sunAlt);
	drefract = 0.061359 * temp1 / temp2; /* in radians */
	h0refract = sunAlt + drefract;

	double airMass =
		elevationCorr / (sin(h0refract) +
			0.50572 * pow(h0refract * RAD_TO_DEG + 6.07995, -1.6364));
	airMass2Linke = 0.8662 * linke;
	// this is linke to airmass turbidity factor
	double tlk = 0.8662 * linke;
	double rayleighThickness = 0;

	//as written in paper by Hofierka and Suri
	//calculates reyleigh optical thickness at air mass given by airMass variable

	if (airMass <= 20) {
		rayleighThickness = 1.0 / (6.6296 + airMass * (1.7513 + airMass * (-0.1202 + airMass * (0.0065 - airMass * 0.00013))));
	}
	else {
		rayleighThickness = 1.0 / (10.4 + 0.718 * airMass); // if airmass is greater than 20
	}


	//*bh = sunRadVar->cbh * sunRadVar->G_norm_extra *
	//  sunVarGeom->sinSolarAltitude *
	//  exp(-rayl * opticalAirMass * airMass2Linke);

	double dniExtra = getIrradianceExtraTerrestrial(dayofyear);
	double dni = dniExtra * exp(-tlk * airMass * rayleighThickness);
	return dni;
}

//https://github.com/3dgeo-heidelberg/vostok/blob/main/src/IrradianceCalc.cpp
double getDiffuseIrradiance(int dayofyear, double linke, double irrBeamNormal, double solarAzimuth, double solarZenith, double surfaceAzimuth, double surfaceTilt)
{
	double irrET = getIrradianceExtraTerrestrial(dayofyear);
	double cosinc = calculate_incidence_angle(surfaceTilt, surfaceAzimuth, solarZenith, solarAzimuth);
	cosinc = cos(cosinc * DEG_TO_RAD);
	double solarAltitude = (90 - solarZenith) * DEG_TO_RAD;
	double tn, fd, a1, a2, a3, a4, fx = 0;  //, gh;
	double kb, rs, aln, ln, fg, diffrad = 0.;
	//value for linke variable is taken from Suncalculator file
	double cosSlope, sinSlope;
	//taken from SunCalculator file
	double bew = (0 * 0.8) / 8;
	double kc = 1 - (0.75 * pow(bew, 3.4));
	//double alb = 0.2;   // for reflected radiation

	// This is Tn_T_LK:
	tn = -0.015843 + linke * (0.030543 + 0.0003797 * linke); //theoretical diffuse irradiance on a horizontal surface with for the air mass 2 Linke turbidity

	//calculating a4 which in turn determines how to cal a,a1 , a2 for fd calculaiton
	//taken from Hofeirka and suri paper
	a4 = 0.26463 + linke * (-0.061581 + 0.0031408 * linke);

	//as done in Hofeirka and suri paper
	if ((a4 * tn) < 0.0022) {
		a1 = 0.0022 / tn;
	}
	else {
		a1 = a4;
	}

	a2 = 2.04020 + linke * (0.018945 - 0.011161 * linke);
	a3 = -1.3025 + linke * (0.039231 + 0.0085079 * linke);

	//getting tilt value in radian and h0 value from elevref field of struct into radian
	double pointTilt = surfaceTilt * M_PI / 180;

	cosSlope = cos(pointTilt); // cos of tilt
	sinSlope = sin(pointTilt);  // sin of tilt; tilt is in radian

	//calculaitng solar altitude function
	fd = a1 + a2 * sin(solarAltitude) + a3 * sin(solarAltitude) * sin(solarAltitude);

	//calculaitng the irradiance on horizontal surface
	double hsr = irrET * fd * tn; //  on horizontal plane
	double bRad = irrBeamNormal * sin(solarAltitude); //beam radiation

	//gh = hsr + bRad; // adding beam radiaiton and extra terrestrial radiance
	//calculaitng the solar radiation on inlcined surface

	//double rr = 0;

	//formula used has been taken from paper
	// it is for inclined surface
	if (pointTilt >= 0.1) {

		kb = bRad / (irrET * sin(solarAltitude)); //kb is amount of normal beam radiaiton available
		rs = (1. + cosSlope) / 2.;

		aln = (solarAzimuth * DEG_TO_RAD) - (surfaceAzimuth * DEG_TO_RAD);
		ln = aln;

		if (aln > M_PI) {
			ln = aln - (2. * M_PI);
		}
		else if (aln < -M_PI) {
			ln = aln + (2. * M_PI);
		}

		aln = ln;
		fg = sinSlope - pointTilt * cosSlope - M_PI * sin(pointTilt / 2.) * sin(pointTilt / 2.);

		// shadow
		if (cosinc < 0) {
			fx = rs + fg * 0.252271;
		}
		else if (solarAltitude >= 0.1) {
			fx = ((0.00263 - kb * (0.712 + 0.6883 * kb)) * fg + rs) * (1. - kb) + kb * (sin(M_PI / 2 - acos(cosinc)) / sin(solarAltitude));
		}
		else if (solarAltitude < 0.1) {
			fx = ((0.00263 - 0.712 * kb - 0.6883 * kb * kb) * fg + rs) * (1. - kb)
				+ kb * sinSlope * cos(aln) / (0.1 - 0.008 * solarAltitude);
		}

		diffrad = kc * hsr * fx;

		//rr = alb * gh * (1 - cos(pointTilt)) / 2.; // getting reflected radiation value
		// it is not added to global radiation value
	}

	else // horizontal plane
	{
		diffrad = kc * hsr;
		//rr = 0.;  // reflected radiaiton is zero
	}

	//there is no radiation before sunrise and after sunset
	//if(varData->elevref <= 0.0 || diffrad<0)
	if (diffrad < 0) {
		diffrad = 0;
		//rr = 0;
	}
	return diffrad;
}

void Utils::getDaylightHours(int dayofyear, double lat, double lon, double& startHour, double& endHour)
{
	double time = 0.0;
	double delta = 0.01;
	startHour = -1;
	endHour = -1;
	while (time < 23.0)
	{
		double solarZenith = 0;
		double solarAzimuth = 0;
		solar_azimuth_angle(dayofyear, lat, lon, time, solarZenith, solarAzimuth);
		if (startHour < 0 && solarZenith < 90)
		{
			startHour = time;
		}
		else if (startHour > 0 && endHour < 0 && solarZenith > 90)
		{
			endHour = time;
			break;
		}
		time += delta;
	}
	if (startHour < 0)
		startHour = 0;
	if (endHour < 0)
		endHour = 23.9999999;

	//printf("startHour: %f, endHour: %f\n", startHour, endHour);
}

std::vector<SunVector> Utils::getSunVectors(SolarParam& sparam)
{
	std::vector<SunVector> sunVectors;
	double time = 0.0;
	double delta = sparam.m_time_step;
	double startHour = -1;
	double endHour = -1;
	while (time < 23.0)
	{
		double solarZenith = 0;
		double solarAzimuth = 0;
		solar_azimuth_angle(sparam.m_day, sparam.m_lat, sparam.m_lon, time, solarZenith, solarAzimuth);
		if (startHour < 0 && solarZenith < 90)
		{
			startHour = time;
		}
		else if (startHour > 0 && endHour < 0 && solarZenith > 90)
		{
			endHour = time;
			break;
		}

		time += delta;
		if (solarZenith < 90)
		{
			SunVector sunVec;
			sunVec.m_azimuth = solarAzimuth;
			sunVec.m_alt = 90 - solarZenith;
			sunVec.m_time = time;
			sunVectors.push_back(sunVec);
		}
	}
	if (startHour < 0)
		startHour = 0;
	if (endHour < 0)
		endHour = 23.9999999;
	return sunVectors;
}

//https://github.com/3dgeo-heidelberg/vostok/blob/main/src/IrradianceCalc.cpp
SolarRadiation Utils::calSolar(int dayofyear, double linke, double elevation, double solarAzimuth, double solarZenith, double surfaceAzimuth, double surfaceTilt, bool isShadowed)
{
	double dni = 1.353 * pow(0.7, pow(1.0 / cos(solarZenith * DEG_TO_RAD), 0.678));
	dni = getDirectIrradianceNormal(dayofyear, solarZenith, elevation, linke);
	//osg::Vec3 sunVector = computeUnitVector(solarAzimuth, solarZenith);
	//osg::Vec3 surfaceNormal = computeUnitVector(surfaceAzimuth, surfaceTilt);
	double cosinc = calculate_incidence_angle(surfaceTilt, surfaceAzimuth, solarZenith, solarAzimuth);
	//double angle = computeAngleOfIncidence(surfaceNormal, sunVector);
	//angle = computeAngleOfIncidence(surfaceNormal, sunVector);
	double di = (!isShadowed && cosinc < 90) ? dni * cos(cosinc * DEG_TO_RAD) : 0.0;
	double dhi = getDiffuseIrradiance(dayofyear, linke, dni, solarAzimuth, solarZenith, surfaceAzimuth, surfaceTilt);
	SolarRadiation rad;
	rad.Zero();
	//rad.DirectNormal = dni;
	rad.m_reflected = 0.0;
	rad.m_beam = di;
	rad.m_diffuse = dhi;
	rad.m_global = di + dhi;
	return rad;
}