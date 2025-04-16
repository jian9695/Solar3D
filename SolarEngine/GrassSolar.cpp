
/*******************************************************************************
r.sun: This program was writen by Jaro Hofierka in Summer 1993 and re-engineered
in 1996-1999. In cooperation with Marcel Suri and Thomas Huld from JRC in Ispra
a new version of r.sun was prepared using ESRA solar radiation formulas.
See manual pages for details.
(C) 2002 Copyright Jaro Hofierka, Gresaka 22, 085 01 Bardejov, Slovakia,
							and GeoModel, s.r.o., Bratislava, Slovakia
email: hofierka@geomodel.sk,marcel.suri@jrc.it,suri@geomodel.sk,
			 Thomas Huld <Thomas.Huld@jrc.it> (lat/long fix)
*******************************************************************************/
/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

 /*v. 2.0 July 2002, NULL data handling, JH */
 /*v. 2.1 January 2003, code optimization by Thomas Huld, JH */
 /*v. 3.0 February 2006, several changes (shadowing algorithm, earth's curvature JH */

#include "GrassSolar.h"
#include <fstream>
#include <sstream>
#include <osg/Matrix>
#include <osg/Geometry>
#include <osg/LineSegment>
#include <osgUtil/IntersectionVisitor>
#include <osgUtil/LineSegmentIntersector>

#define M_PI 3.1415926
#define M2_PI    2. * M_PI
#define RAD      360. / (2. * M_PI)
#define DEG      (2. * M_PI)/360.
#define EARTHRADIUS 6371000.	/* appx. for most ellipsoids or projections */
#define UNDEF    0.		/* undefined value for terrain aspect */
#define UNDEFZ   -9999.		/* internal undefined value for NULL */
#define SKIP    "1"
#define BIG      1.e20
#define IBIG     32767
#define EPS      1.e-4
#define LINKE    "3.0"
#define ALB      "0.2"
#define STEP     "0.5"
#define BSKY	  1.0
#define DSKY	  1.0
#define DIST      0.8

#define AMAX1(arg1, arg2) ((arg1) >= (arg2) ? (arg1) : (arg2))
#define AMIN1(arg1, arg2) ((arg1) <= (arg2) ? (arg1) : (arg2))
#define DISTANCE1(x1, x2, y1, y2) (sqrt((x1 - x2)*(x1 - x2) + (y1 - y2)*(y1 - y2)))

const double deg2rad = M_PI / 180.;
const double rad2deg = 180. / M_PI;

#define DEGREEINMETERS 111120.
//#define linke 3.0
//#define cbh BSKY
//#define cdh DSKY

double cbh = BSKY;//clear-sky index for beam component
double cdh = DSKY;//clear-sky index for diffuse component
#define alb 0.2
#define pi 3.14159265358979323846
#define earthRadiusKm 6371000.0

void printfVec3(osg::Vec3 v)
{
	printf("(%f,%f,%f)\n", v.x(), v.y(), v.z());
}

// This function converts decimal degrees to radians
double GrassSolar::deg2rad2(double deg) {
	return (deg * pi / 180);
};

//  This function converts radians to decimal degrees
double GrassSolar::rad2deg2(double rad) {
	return (rad * 180 / pi);
};

SolarRadiation GrassSolar::calculateSolarRadiation(SolarParam& solar_param)
{
	double global;
	double lum, q1;
	TempVariables tmpval;
	tmpval.linke = solar_param.m_linke;
	tmpval.latitude = solar_param.m_lat;
	tmpval.day = solar_param.m_day;
	tmpval.step = solar_param.m_time_step;
	tmpval.aspect = solar_param.m_aspect;
	tmpval.slope = solar_param.m_slope;
	tmpval.shadowInfo = solar_param.m_shadowInfo;
	tmpval.tien = solar_param.m_isShadowed;
	tmpval.elev = solar_param.m_elev;
	cbh = solar_param.m_bsky;
	cdh = solar_param.m_dsky;
	if (m_COLLECT_SUN_VECTOR)
	{
		m_sunVectors.clear();
	}

	SolarRadiation result;
	m_curTimeStep = 0;
	//tmpval.aspect = 90;
	//tmpval.slope = 0;

	tmpval.elev = 0;
	if (tmpval.aspect != 0.) {
		if (tmpval.aspect < 90.)
			tmpval.aspect = 90. - tmpval.aspect;
		else
			tmpval.aspect = 450. - tmpval.aspect;
	}

	tmpval.declination = com_declin(tmpval.day);
	tmpval.sindecl = sin(tmpval.declination);
	tmpval.cosdecl = cos(tmpval.declination);

	tmpval.c = com_sol_const(tmpval.day);

	tmpval.length = 0;

	tmpval.coslat = cos(deg2rad * tmpval.latitude);
	tmpval.coslatsq = tmpval.coslat * tmpval.coslat;

	tmpval.aspect = tmpval.aspect * DEG;
	tmpval.slope = tmpval.slope * DEG;

	tmpval.latitude = -tmpval.latitude * DEG;


	tmpval.cos_u = cos(M_PI / 2 - tmpval.slope);
	tmpval.sin_u = sin(M_PI / 2 - tmpval.slope);
	tmpval.cos_v = cos(M_PI / 2 + tmpval.aspect);
	tmpval.sin_v = sin(M_PI / 2 + tmpval.aspect);

	tmpval.sinlat = sin(tmpval.latitude);
	tmpval.coslat = cos(tmpval.latitude);

	tmpval.sin_phi_l = -tmpval.coslat * tmpval.cos_u * tmpval.sin_v + tmpval.sinlat * tmpval.sin_u;
	tmpval.latid_l = asin(tmpval.sin_phi_l);

	q1 = tmpval.sinlat * tmpval.cos_u * tmpval.sin_v + tmpval.coslat * tmpval.sin_u;
	tmpval.tan_lam_l = -tmpval.cos_u * tmpval.cos_v / q1;
	tmpval.longit_l = atan(tmpval.tan_lam_l);
	tmpval.lum_C31_l = cos(tmpval.latid_l) * tmpval.cosdecl;
	tmpval.lum_C33_l = tmpval.sin_phi_l * tmpval.sindecl;

	com_par_const(tmpval);
	com_par(tmpval);
	lum = lumcline2(tmpval);
	lum = RAD * asin(lum);

	double assignedTime = solar_param.m_time.toDecimalHour();
	joules2(tmpval, solar_param.m_isInstantaneous, assignedTime);
	global = tmpval.beam_e + tmpval.diff_e + tmpval.refl_e;

	result.m_global = global;
	result.m_beam = tmpval.beam_e;
	result.m_diffuse = tmpval.diff_e;
	result.m_reflected = tmpval.refl_e;

	//printf("insol=%f,beam=%f,diff=%f,refl=%f,global=%f\n", tmpval.insol_t,tmpval.beam_e,tmpval.diff_e,tmpval.refl_e,global);
	m_COLLECT_SUN_VECTOR = false;
	return result;

}

void GrassSolar::com_par_const(TempVariables& tmpval)
{
	double pom;

	tmpval.lum_C11 = tmpval.sinlat * tmpval.cosdecl;
	tmpval.lum_C13 = -tmpval.coslat * tmpval.sindecl;
	tmpval.lum_C22 = tmpval.cosdecl;
	tmpval.lum_C31 = tmpval.coslat * tmpval.cosdecl;
	tmpval.lum_C33 = tmpval.sinlat * tmpval.sindecl;

	if (fabs(tmpval.lum_C31) >= EPS) {
		pom = -tmpval.lum_C33 / tmpval.lum_C31;
		if (fabs(pom) <= 1) {
			pom = acos(pom);
			pom = (pom * 180) / M_PI;
			tmpval.sunrise_time = (90 - pom) / 15 + 6;
			tmpval.sunset_time = (pom - 90) / 15 + 18;
		}
		else {
			if (pom < 0) {
				/*        printf("\n Sun is ABOVE the surface during the whole day\n"); */
				tmpval.sunrise_time = 0;
				tmpval.sunset_time = 24;
			}
			if (fabs(pom) - 1 <= EPS)
			{
				//printf("\texcept at midnight is sun ON THE HORIZONT\n");
			}
			else {
				/*                printf("\n The sun is BELOW the surface during the whole day\n"); */
				if (fabs(pom) - 1 <= EPS) {
					//printf("\texcept at noon is sun ON HORIZONT\n");
					tmpval.sunrise_time = 12;
					tmpval.sunset_time = 12;
				}
			}
		}
	}

}

void GrassSolar::com_par(TempVariables& tmpval)
{
	double old_time, pom, xpom, ypom;

	double coslum_time;

	coslum_time = cos(tmpval.lum_time);

	old_time = tmpval.lum_time;


	tmpval.lum_Lx = -tmpval.lum_C22 * sin(tmpval.lum_time);
	tmpval.lum_Ly = tmpval.lum_C11 * coslum_time + tmpval.lum_C13;
	tmpval.lum_Lz = tmpval.lum_C31 * coslum_time + tmpval.lum_C33;

	if (fabs(tmpval.lum_C31) < EPS) {
		if (fabs(tmpval.lum_Lz) >= EPS) {
			if (tmpval.lum_Lz > 0) {
				/*                        printf("\tSun is ABOVE area during the whole day\n"); */
				tmpval.sunrise_time = 0;
				tmpval.sunset_time = 24;
			}
			else {
				tmpval.h0 = 0.;
				tmpval.A0 = UNDEF;
				return;
			}
		}
		else {
			/*                      printf("\tThe Sun is ON HORIZON during the whole day\n"); */
			tmpval.sunrise_time = 0;
			tmpval.sunset_time = 24;
		}
	}

	tmpval.h0 = asin(tmpval.lum_Lz);		/* vertical angle of the sun */
	/* lum_Lz is sin(h0) */

	xpom = tmpval.lum_Lx * tmpval.lum_Lx;
	ypom = tmpval.lum_Ly * tmpval.lum_Ly;
	pom = sqrt(xpom + ypom);


	if (fabs(pom) > EPS) {
		tmpval.A0 = tmpval.lum_Ly / pom;
		tmpval.A0 = acos(tmpval.A0);		/* horiz. angle of the Sun */

		/*                      A0 *= RAD; */

		if (tmpval.lum_Lx < 0)
			tmpval.A0 = M2_PI - tmpval.A0;
	}
	else {
		tmpval.A0 = UNDEF;
		//if (tmpval.h0 > 0)
		//    //printf("A0 = Zenit\n");
		//else
		//    //printf("A0 = Nadir\n");
	}

	if (tmpval.A0 < 0.5 * M_PI)
		tmpval.angle = 0.5 * M_PI - tmpval.A0;
	else
		tmpval.angle = 2.5 * M_PI - tmpval.A0;

	tmpval.tanh0 = tan(tmpval.h0);


}

/**********************************************************/
double GrassSolar::lumcline2(TempVariables& tmpval)
{
	double s = 0;
	int r = 0;
	//tmpval.tien = 0;
	tmpval.length = 0;
	s = tmpval.lum_C31_l * cos(-tmpval.lum_time - tmpval.longit_l) + tmpval.lum_C33_l;	/* Jenco */
	if (s < 0)
		return 0.;
	return (s);
}

void GrassSolar::joules2(TempVariables& tmpval, const bool& isInstaneous, const double& assignedTime)
{

	double s0, dfr, dfr_rad, dfr1_rad, dfr2_rad, fr1, fr2, dfr1, dfr2;
	double ra, dra, ss_rad = 0., sr_rad;
	int i1, i2, ss = 1, ss0 = 1;

	tmpval.beam_e = 0.;
	tmpval.diff_e = 0.;
	tmpval.refl_e = 0.;
	tmpval.insol_t = 0.;
	//tmpval.tien = 0;

	//if (tt == NULL)
	tmpval.lum_time = 0.;

	com_par_const(tmpval);
	com_par(tmpval);
	if (isInstaneous)
	{

		if (assignedTime < tmpval.sunrise_time || assignedTime > tmpval.sunset_time)
		{
			if (m_COLLECT_SUN_VECTOR)
			{
				SunVector sunvec;
				sunvec.m_azimuth = 0;
				sunvec.m_alt = 0;
				sunvec.m_time = tmpval.lum_time;
				m_sunVectors.push_back(sunvec);
				m_curTimeStep++;
			}
			return;
		}

		tmpval.sunrise_time = assignedTime;

	}
	i1 = (int)tmpval.sunrise_time;
	fr1 = tmpval.sunrise_time - i1;
	if (fr1 > 0.)
		fr1 = 1 - fr1;
	else
		fr1 = -fr1;

	dfr1 = fr1;
	while (dfr1 > tmpval.step) {
		dfr1 = dfr1 - tmpval.step;
	}

	i2 = (int)tmpval.sunset_time;
	fr2 = tmpval.sunset_time - i2;

	dfr2 = fr2;
	while (dfr2 > tmpval.step) {
		dfr2 = dfr2 - tmpval.step;
	}

	sr_rad = (tmpval.sunrise_time - 12.) * 15.;
	if (ss_rad < 0)
		sr_rad += 360;
	sr_rad = sr_rad * DEG;
	ss_rad = (tmpval.sunset_time - 12.) * 15.;
	if (ss_rad < 0)
		ss_rad += 360;
	ss_rad = ss_rad * DEG;

	dfr1_rad = dfr1 * 15. * DEG;
	dfr2_rad = dfr2 * 15. * DEG;
	dfr_rad = tmpval.step * 15. * DEG;

	tmpval.lum_time = sr_rad + dfr1_rad / 2.;
	dfr = dfr1;

	while (ss == 1) {

		com_par(tmpval);
		//printf("time=%f,horizontal=%f,vertical=%f\n",tmpval.lum_time,rad2deg2(tmpval.A0),rad2deg2(tmpval.h0));
		if (m_COLLECT_SUN_VECTOR)
		{
			SunVector sunvec;
			sunvec.m_azimuth = rad2deg2(tmpval.A0);
			sunvec.m_alt = rad2deg2(tmpval.h0);
			sunvec.m_time = tmpval.lum_time;
			m_sunVectors.push_back(sunvec);
			m_curTimeStep++;
		}
		else if (tmpval.shadowInfo)
		{
			tmpval.tien = tmpval.shadowInfo[m_curTimeStep];
		}

		m_curTimeStep++;
		s0 = lumcline2(tmpval);


		if (tmpval.h0 > 0.) {
			//		if (tmpval.tien != 1 && s0 > 0.) {
			if (!tmpval.tien && s0 > 0.) {
				tmpval.insol_t += dfr;
				ra = brad(s0, tmpval);
				if (isInstaneous)
				{
					tmpval.beam_e = ra;
				}

				tmpval.beam_e += dfr * ra;
				ra = 0.;

			}
			else
				tmpval.bh = 0.;

			//dra = drad(s0,tmpval);
			dra = drad_isotropic(s0, tmpval);


			if (isInstaneous)
			{
				tmpval.diff_e = dra;
				tmpval.refl_e = tmpval.rr;
				break;
			}

			tmpval.diff_e += dfr * dra;
			dra = 0.;
			//drad(s0,tmpval);

			tmpval.refl_e += dfr * tmpval.rr;
			tmpval.rr = 0.;

			//}
		}			/* illuminated */
		//printf("insol=%f,beam=%f,diff=%f,refl=%f,global=%f\n", tmpval.insol_t,tmpval.beam_e,tmpval.diff_e,tmpval.refl_e,tmpval.insol_t+tmpval.beam_e+tmpval.diff_e);

		if (ss0 == 0)
			return;

		if (dfr < tmpval.step) {
			dfr = tmpval.step;
			tmpval.lum_time = tmpval.lum_time + dfr1_rad / 2. + dfr_rad / 2.;
		}
		else {
			tmpval.lum_time = tmpval.lum_time + dfr_rad;
		}
		if (tmpval.lum_time > ss_rad - dfr2_rad / 2.) {
			dfr = dfr2;
			tmpval.lum_time = ss_rad - dfr2_rad / 2.;
			ss0 = 0;	/* we've got the sunset */
		}
	}			/* end of while */


}

double GrassSolar::com_sol_const(double no_of_day)
{
	double I0, d1;

	/*  v W/(m*m) */
	d1 = M2_PI * no_of_day / 365.25;
	I0 = 1367. * (1 + 0.03344 * cos(d1 - 0.048869));

	return I0;
}

double GrassSolar::com_declin(double no_of_day)
{
	double d1, decl;

	d1 = M2_PI * no_of_day / 365.25;
	decl = asin(0.3978 * sin(d1 - 1.4 + 0.0355 * sin(d1 - 0.0489)));
	decl = -decl;
	//printf(" declination : %lf\n", decl); 

	return (decl);
}

double GrassSolar::brad(double sh, TempVariables& tmpval)
{
	double p, lm, tl, rayl, br;
	double drefract, temp1, temp2, h0refract;

	p = exp(-tmpval.elev / 8434.5);
	temp1 = 0.1594 + tmpval.h0 * (1.123 + 0.065656 * tmpval.h0);
	temp2 = 1. + tmpval.h0 * (28.9344 + 277.3971 * tmpval.h0);
	drefract = 0.061359 * temp1 / temp2;	/* in radians */
	h0refract = tmpval.h0 + drefract;
	lm = p / (sin(h0refract) +
		0.50572 * pow(h0refract * RAD + 6.07995, -1.6364));
	tl = 0.8662 * tmpval.linke;
	if (lm <= 20.)
		rayl =
		1. / (6.6296 +
			lm * (1.7513 +
				lm * (-0.1202 + lm * (0.0065 - lm * 0.00013))));
	else
		rayl = 1. / (10.4 + 0.718 * lm);
	tmpval.bh = cbh * tmpval.c * tmpval.lum_Lz * exp(-rayl * lm * tl);
	if (tmpval.aspect != UNDEF && tmpval.slope != 0.)
		br = tmpval.bh * sh / tmpval.lum_Lz;
	else
		br = tmpval.bh;

	return (br);
}

//double drad(double sh,TempVariables& tmpval)
//{
//    double tn, fd, fx = 0., A1, A2, A3, A1b;
//    double r_sky, kb, dr, gh, a_ln, ln, fg;
//    double cosslope, sinslope;
//
//    cosslope = cos(tmpval.slope);
//    sinslope = sin(tmpval.slope);
//
//    tn = -0.015843 + tmpval.linke * (0.030543 + 0.0003797 * tmpval.linke);
//    A1b = 0.26463 +  tmpval.linke * (-0.061581 + 0.0031408 * tmpval.linke);
//    if (A1b * tn < 0.0022)
//	A1 = 0.0022 / tn;
//    else
//	A1 = A1b;
//    A2 = 2.04020 + tmpval.linke * (0.018945 - 0.011161 * tmpval.linke);
//    A3 = -1.3025 + tmpval.linke * (0.039231 + 0.0085079 * tmpval.linke);
//
//    fd = A1 + A2 * tmpval.lum_Lz + A3 * tmpval.lum_Lz * tmpval.lum_Lz;
//    tmpval.dh = cdh * tmpval.c * fd * tn;
//    gh = tmpval.bh + tmpval.dh;
//    if (tmpval.aspect != UNDEF && tmpval.slope != 0.) {
//	kb = tmpval.bh / (tmpval.c * tmpval.lum_Lz);
//	r_sky = (1. + cosslope) / 2.;
//	a_ln = tmpval.A0 - tmpval.aspect;
//	ln = a_ln;
//	if (a_ln > M_PI)
//	    ln = a_ln - M2_PI;
//	else if (a_ln < -M_PI)
//	    ln = a_ln + M2_PI;
//	a_ln = ln;
//	fg = sinslope - tmpval.slope * cosslope -
//	    M_PI * sin(tmpval.slope / 2.) * sin(tmpval.slope / 2.);
//	if (tmpval.tien == 1 || sh <= 0.)
//	    fx = r_sky + fg * 0.252271;
//	else if (tmpval.h0 >= 0.1) {
//	    fx = ((0.00263 - kb * (0.712 + 0.6883 * kb)) * fg + r_sky) * (1. -
//									  kb)
//		+ kb * sh / tmpval.lum_Lz;
//	}
//	else if (tmpval.h0 < 0.1)
//	    fx = ((0.00263 - 0.712 * kb - 0.6883 * kb * kb) * fg +
//		  r_sky) * (1. - kb) + kb * sin(tmpval.slope) * cos(a_ln) / (0.1 -
//								      0.008 *
//								      tmpval.h0);
//	dr = tmpval.dh * fx;
//	/* refl. rad */
//	tmpval.rr = alb * gh * (1 - cos(tmpval.slope)) / 2.;
//    }
//    else {			/* plane */
//	dr = tmpval.dh;
//	tmpval.rr = 0.;
//    }
//    return (dr);
//}

//isotropic diffuse model
double GrassSolar::drad_isotropic(double sh, TempVariables& tmpval)
{
	double tn, fd, fx = 0., A1, A2, A3, A1b;
	double r_sky, kb, dr, gh, a_ln, ln, fg;
	double cosslope, sinslope;

	cosslope = cos(tmpval.slope);
	sinslope = sin(tmpval.slope);

	tn = -0.015843 + tmpval.linke * (0.030543 + 0.0003797 * tmpval.linke);
	A1b = 0.26463 + tmpval.linke * (-0.061581 + 0.0031408 * tmpval.linke);
	if (A1b * tn < 0.0022)
		A1 = 0.0022 / tn;
	else
		A1 = A1b;
	A2 = 2.04020 + tmpval.linke * (0.018945 - 0.011161 * tmpval.linke);
	A3 = -1.3025 + tmpval.linke * (0.039231 + 0.0085079 * tmpval.linke);

	fd = A1 + A2 * tmpval.lum_Lz + A3 * tmpval.lum_Lz * tmpval.lum_Lz;

	//
	tmpval.dh = cdh * tmpval.c * fd * tn;// diffuse horizontal irradiance
	gh = tmpval.bh + tmpval.dh;
	if (tmpval.aspect != UNDEF && tmpval.slope != 0.) {
		kb = tmpval.bh / (tmpval.c * tmpval.lum_Lz);
		r_sky = (1. + cosslope) / 2.;
		a_ln = tmpval.A0 - tmpval.aspect;
		ln = a_ln;
		if (a_ln > M_PI)
			ln = a_ln - M2_PI;
		else if (a_ln < -M_PI)
			ln = a_ln + M2_PI;
		a_ln = ln;
		fg = sinslope - tmpval.slope * cosslope -
			M_PI * sin(tmpval.slope / 2.) * sin(tmpval.slope / 2.);
		if (tmpval.tien == 1 || sh <= 0.)
			fx = r_sky + fg * 0.252271;
		else if (tmpval.h0 >= 0.1) {
			fx = ((0.00263 - kb * (0.712 + 0.6883 * kb)) * fg + r_sky) * (1. -
				kb)
				+ kb * sh / tmpval.lum_Lz;
		}
		else if (tmpval.h0 < 0.1)
			fx = ((0.00263 - 0.712 * kb - 0.6883 * kb * kb) * fg +
				r_sky) * (1. - kb) + kb * sin(tmpval.slope) * cos(a_ln) / (0.1 -
					0.008 *
					tmpval.h0);
		double rate = cos(tmpval.slope);
		dr = tmpval.dh * (1 + rate) / 2;
		/* refl. rad */
		tmpval.rr = alb * gh * (1 - cos(tmpval.slope)) / 2.;
	}
	else {			/* plane */
		dr = tmpval.dh;
		tmpval.rr = 0.;
	}
	return (dr);
}
#pragma region perez diffuse model

//double get_am(double z)
//{
//	//where the radius of the Earth R_\mathrm E = 6371 km, the effective height of the atmosphere y_\mathrm{atm} ¡Ö 9 km, and their ratio r = R_\mathrm E / y_\mathrm{atm} ¡Ö 708.
//	double r = 708;
//	double rcosz = r*cos(z);
//	double am = sqrt(rcosz*rcosz + 2*r + 1) - rcosz;
//	return am;
//}
//void perez_coeff(int bin,double& f11,double& f12,double& f13,double& f21,double& f22,double& f23)
//{
//
//	static double table[48] = 
//	{
//		-0.008,0.588,-0.062,-0.06,0.072,-0.022
//		,0.13,0.683,-0.151,-0.019,0.066,-0.029
//		,0.33,0.487,-0.221,0.055,-0.064,-0.026
//		,0.568,0.187,-0.295,0.109,-0.152,-0.014
//		,0.873,-0.392,-0.362,0.226,-0.462,0.001
//		,1.132,-1.237,-0.412,0.288,-0.823,0.056
//		,1.06,-1.6,-0.359,0.264,-1.127,0.131
//		,0.678,-0.327,-0.25,0.156,-1.377,0.251
//	};
//	int pos = (bin-1)*6;
//	f11 = table[pos+0];f12 = table[pos+1];f13 = table[pos+2];f21 = table[pos+3];f22 = table[pos+4];f23 = table[pos+5];
//	//1	-0.008	0.588	-0.062	-0.06	0.072	-0.022
//	//2	0.13	0.683	-0.151	-0.019	0.066	-0.029
//	//3	0.33	0.487	-0.221	0.055	-0.064	-0.026
//	//4	0.568	0.187	-0.295	0.109	-0.152	-0.014
//	//5	0.873	-0.392	-0.362	0.226	-0.462	0.001
//	//6	1.132	-1.237	-0.412	0.288	-0.823	0.056
//	//7	1.06	-1.6	-0.359	0.264	-1.127	0.131
//	//8	0.678	-0.327	-0.25	0.156	-1.377	0.251
//}
//int sky_clearness_bin(double val)
//{
//
//	//1 Overcast	1	1.065
//	//2	1.065	1.230
//	//3	1.230	1.500
//	//4	1.500	1.950
//	//5	1.950	2.800
//	//6	2.800	4.500
//	//7	4.500	6.200
//	//8 Clear	6.200	¨C
//	if(val < 1.065)
//		return 1;
//	if(val < 1.230)
//		return 2;
//	if(val < 1.500)
//		return 3;
//	if(val < 1.950)
//		return 4;
//	if(val < 2.800)
//		return 5;
//	if(val < 4.500)
//		return 6;
//	if(val < 6.200)
//		return 7;
//	return 8;
//
//}
//double drad_petez(double sh,TempVariables& tmpval)
//{
//	double f1,f2,f11,f12,f13,f21,f22,f23,delta;
//	double thetaZ2 = (90 - rad2deg*asin(tmpval.lum_Lz))*deg2rad;//\theta_{Z} is the solar zenith angle.
//	double thetaZ = deg2rad * 90 - asin(tmpval.lum_Lz);//\theta_{Z} is the solar zenith angle.
//	double thetaT = tmpval.slope;//\theta_{T} is the array tilt angle from horizontal.
//	double aoi = asin(sh);//is the angle of incidence between the sun and the plane of the array.
//	double zenith = 90*deg2rad-tmpval.h0;
//	double azimuth = tmpval.A0;
//	double aoi2 = acos(cos(zenith)*cos(tmpval.slope)+sin(zenith)*sin(tmpval.slope)*cos(azimuth-tmpval.aspect));
//	double aoiAngle = rad2deg * aoi;
//	double aoiAngle2 = rad2deg * aoi2;
//	aoi = aoi2;
//	double dhi = get_dhi(sh,tmpval);// diffuse horizontal irradiance,
//	double dni = tmpval.dni;// diffuse horizontal irradiance,   
//	if(dhi == 0)
//		return 0;
//	delta = dhi * get_am(thetaZ) / tmpval.c;
//
//	double a = MAX(0,cos(aoi));
//	double b = MAX(cos(85*deg2rad),cos(thetaZ));
//	//double b = MAX(0.087,cos(thetaZ));
//	double k = 1.041;//\kappa is a constant equal to 1.041 for angles are in radians (or  5.535\times 10^{-6}  for angles in degrees
//	double k_theta_3 = k*pow(thetaZ,3.0);
//	double clearb = ((dhi+dni)/dhi+k_theta_3)/(1+k_theta_3);//epsilon Sky clearness bins
//	int bin = sky_clearness_bin(clearb);
//	perez_coeff(bin,f11,f12,f13,f21,f22,f23);
//	f1 = MAX(0,f11 + f12*delta+thetaZ*f13);
//	f2 = f21 + f22*delta + thetaZ*f23;
//	double term1,term2,term3;
//	term1 = (1-f1)*(0.5+cos(thetaT)*0.5);
//	term2 = f1*(a/b);
//	term3 = f2*sin(thetaT);
//	double ed = dhi * (term1 + term2 + term3);
//	return ed;
//}
#pragma endregion perez diffuse model
//diffuse horizontal irradiance

std::vector<SunVector> GrassSolar::getSunVectors(SolarParam& sparam)
{
	m_COLLECT_SUN_VECTOR = true;
	m_sunVectors.clear();
	calculateSolarRadiation(sparam);
	m_COLLECT_SUN_VECTOR = false;
	//printf("longitude: %f, latitude: %f\n", sparam.m_lon, sparam.m_lat);
	//for (size_t i = 0; i < m_sunVectors.size(); i++)
	//{
	//	double altitude = m_sunVectors[i].m_alt;
	//	double azimuth = m_sunVectors[i].m_azimuth;
	//	osg::Vec3d sundir = Utils::solarAngle2Vector(altitude, azimuth);
	//	//std::stringstream ss;
	//	//ss << "azimuth: " << azimuth << ", altitude: " << altitude << ", solar direction: (" << sundir.x() << "," << sundir.y() << "," << sundir.z() << ")\n";
	//	//std::string str = ss.str();
	//	printf("azimuth: %f, altitude: %f,solor direction: (%f,%f,%f)\n", azimuth, altitude, sundir.x(), sundir.y(), sundir.z());
	//}

	return m_sunVectors;
}

SolarRadiation GrassSolar::calculateSolarRadiation(SolarParam& solar_param, osg::Node* sceneNode, osg::Vec3d pos)
{
	std::vector<SunVector> sunVectors = getSunVectors(solar_param);
	std::vector<osg::Vec3d> lightDirs = Utils::sunVector2LightDir(sunVectors);

	solar_param.m_shadowInfo = new bool[lightDirs.size()];
	std::string shadowMasks = "";
	for (size_t i = 0; i < lightDirs.size(); i++)
	{
		osg::Vec3d start = pos;
		osg::Vec3d end = pos + lightDirs[i] * 100000;
		osg::ref_ptr<osgUtil::LineSegmentIntersector> intersector = new osgUtil::LineSegmentIntersector(start, end);
		osgUtil::IntersectionVisitor intersectVisitor(intersector.get());
		sceneNode->accept(intersectVisitor);
		if (intersector->containsIntersections())
		{
			solar_param.m_shadowInfo[i] = true;
			shadowMasks += "1";
		}
		else
		{
			solar_param.m_shadowInfo[i] = false;
			shadowMasks += "0";
		}
		if (i < lightDirs.size() - 1)
		{
			shadowMasks += ",";
		}
	}
	SolarRadiation rad = calculateSolarRadiation(solar_param);
	rad.m_shadowMasks = shadowMasks;
	delete[] solar_param.m_shadowInfo;
	return rad;
}

void PrintVec3d(const osg::Vec3d& vec)
{
	printf("%f,%f,%f\n", vec.x(), vec.y(), vec.z());
}