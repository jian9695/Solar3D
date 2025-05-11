#pragma once

#ifdef _WIN32 || WIN32 
#include <Windows.h>
#endif

#include <string>
#include <math.h>
#include <fstream>
#include <sstream>
#include <vector>
#include <osg/Vec3d>
#include <osg/Texture2D>

enum ValType
{
	d = 0,
	f = 1,
	vec = 2,
	s = 3,
	i = 4
};

struct OuputVariable
{
public:
	double m_d;
	float m_f;
	osg::Vec3d m_vec;
	std::string m_s;
	long m_i;
	ValType m_type;
	OuputVariable(double val)
	{
		m_d = val;
		m_type = ValType::d;
	}

	OuputVariable(float val)
	{
		m_f = val;
		m_type = ValType::f;
	}

	OuputVariable(const osg::Vec3d& val)
	{
		m_vec = val;
		m_type = ValType::vec;
	}

	OuputVariable(const std::string& val)
	{
		m_s = val;
		m_type = ValType::s;
	}

	OuputVariable(int val)
	{
		m_i = val;
		m_type = ValType::i;
	}

	OuputVariable(long val)
	{
		m_i = val;
		m_type = ValType::i;
	}

	void out(std::ofstream& ofs)
	{
		if (m_type == ValType::d)
		{
			ofs << m_d;
		}
		else if (m_type == ValType::f)
		{
			ofs << m_f;
		}
		else if (m_type == ValType::s)
		{
			ofs << "\"" << m_s << "\"";
		}
		else if (m_type == ValType::i)
		{
			ofs << m_i;
		}
		else if (m_type == ValType::vec)
		{
			ofs << "\"" << "[" << m_vec.x() << "," << m_vec.y() << "," << m_vec.z() << "]" << "\"";
		}
	}

	void out(std::stringstream& ss)
	{
		if (m_type == ValType::d)
		{
			ss.precision(3);
			ss << std::fixed << m_d;
		}
		else if (m_type == ValType::f)
		{
			ss.precision(3);
			ss << std::fixed << m_f;
		}
		else if (m_type == ValType::s)
		{
			ss << "\"" << m_s << "\"";
		}
		else if (m_type == ValType::i)
		{
			ss.precision(0);
			ss << m_i;
		}
		else if (m_type == ValType::vec)
		{
			ss.precision(3);
			ss << std::fixed << "\"" << "[" << m_vec.x() << "," << m_vec.y() << "," << m_vec.z() << "]" << "\"";
		}
	}
};

//structure of solar radiation result
struct SolarRadiation
{
	float m_global;//global component
	float m_beam;//beam component
	float m_diffuse;//diffuse component
	float m_reflected;//reflected component
	float m_svf;//sky view factor
	std::string m_shadowMasks;
public:
	SolarRadiation()
	{
		m_global = -9999;
		m_beam = -9999;
		m_diffuse = -9999;
		m_reflected = -9999;
		m_shadowMasks = "";
	}
	void Zero()
	{
		m_global = 0;
		m_beam = 0;
		m_diffuse = 0;
		m_reflected = 0;
		m_shadowMasks = "";
	}

	SolarRadiation operator+(const SolarRadiation& rad)
	{
		SolarRadiation newrad;
		newrad.m_beam = rad.m_beam + this->m_beam;
		newrad.m_global = rad.m_global + this->m_global;
		newrad.m_diffuse = rad.m_diffuse + this->m_diffuse;
		newrad.m_reflected = rad.m_reflected + this->m_reflected;
		return newrad;
	}

	SolarRadiation operator*(const SolarRadiation& rad)
	{
		SolarRadiation newrad;
		newrad.m_beam = rad.m_beam * this->m_beam;
		newrad.m_global = rad.m_global * this->m_global;
		newrad.m_diffuse = rad.m_diffuse * this->m_diffuse;
		newrad.m_reflected = rad.m_reflected * this->m_reflected;
		return newrad;
	}
	SolarRadiation operator*(const float& val)
	{
		SolarRadiation newrad;
		newrad.m_beam = m_beam * val;
		newrad.m_global = m_global * val;
		newrad.m_diffuse = m_diffuse * val;
		newrad.m_reflected = m_reflected * val;
		return newrad;
	}

	SolarRadiation operator/(const float& val)
	{
		SolarRadiation newrad;
		newrad.m_beam = m_beam / val;
		newrad.m_global = m_global / val;
		newrad.m_diffuse = m_diffuse / val;
		newrad.m_reflected = m_reflected / val;
		return newrad;
	}
};

//structure of solar vector
struct SunVector
{
	float m_time;
	float m_azimuth;//solar azimuth angle
	float m_alt; //solar elevation angle
	SunVector() { m_time = 0;  m_azimuth = 0; m_alt = 0; }
	SunVector(float azimuthAngle, float altAngle) { m_azimuth = azimuthAngle; m_alt = altAngle; }
};

struct SolarTime
{
	int m_hour;
	int m_minute;
	int m_second;
	SolarTime()
	{
		m_hour = 6;
		m_minute = 0;
		m_second = 0;
	}
	SolarTime(int h, int m, int s)
		:m_hour(h), m_minute(m), m_second(s)
	{
	}
	double toDecimalHour()
	{
		return m_hour + m_minute / 60.0 + m_second / 3600.0;
	}

};

//parameters for r.sun calculation
struct SolarParam
{
	float m_linke;//turbidity factor
	float m_bsky;//scale factor for the beam component
	float m_dsky;//scale factor for the diffuse component
	float m_lon;//longitude
	float m_lat;//latitude
	float m_elev;//elevation
	float m_slope;//slope in degrees
	float m_aspect;//aspect in degrees
	float m_time_step;//time resolution in hours
	int m_day;//range from 1 to 366
	bool* m_shadowInfo;//an array of shadow masks corresponding to the number of solar vectors in a day
	bool m_isShadowed;//a single shadow mask will be used if 'shadowInfo' is null
	bool m_isInstantaneous;//apply instantaneous calculation mode
	bool m_isSingleDay;
	bool m_useLatitudeOverride;
	bool m_useElevationOverride;
	int m_startDay;
	int m_endDay;
	SolarTime m_time;//decimal time 

	SolarParam()
	{
		m_shadowInfo = NULL;
		m_isShadowed = false;
		m_isInstantaneous = false;//time-integrated calculation mode as default
		m_elev = 0;
		m_slope = 0;
		m_aspect = 0;
		m_linke = 3.0;
		m_bsky = 1;
		m_dsky = 1;
		m_time_step = 10.0 / 60.0;
		m_day = 1;
		m_startDay = 1;
		m_endDay = 1;
		m_lon = -9999;
		m_lat = 0;
		m_isSingleDay = true;
		m_useLatitudeOverride = true;
		m_useElevationOverride = true;
	}
};

struct SolarRadiationPoint : public SolarParam, public SolarRadiation
{
	osg::Vec3d m_pos;
	int m_id;

	SolarRadiationPoint() { m_id = 0; }
	SolarRadiationPoint(const osg::Vec3d position, const SolarParam& param, const SolarRadiation& rad);
	std::string toString();
	std::string toString(std::string& names, std::string& values);
};



