#pragma once
#include "ogrsf_frmts.h"
//read and write ESRI ShapeFile
//http://support.esri.com/en/white-paper/279
class ShapeFile
{
public:
	ShapeFile();
	ShapeFile(std::string filename, int update = 0);
	~ShapeFile();
	void close();
	int getOrCreateField(const char* _name, OGRFieldType tp);
	void create(std::string filename, OGRSpatialReference* spatialRef = NULL, OGRFeatureDefn *poFDefn = NULL, OGRwkbGeometryType geotype = wkbPolygon);
public:
	OGRLayer       *poLayer;
	GDALDataset    *poDS;
private:
	std::string g_mFileName;
	void splitFilepath(std::string filepath, std::string& _dir, std::string& _name, std::string& ext);
};

