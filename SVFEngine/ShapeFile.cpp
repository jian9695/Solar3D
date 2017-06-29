#include "ShapeFile.h"
#include <fstream>
void ShapeFile::close()
{
	if (poDS)
		GDALClose(poDS);
	poDS = NULL;
}
void ShapeFile::splitFilepath(std::string filepath, std::string& _dir, std::string& _name, std::string& ext)
{
	int lastdot = -1;
	int lastslash = -1;
	int secondslash = -1;
	for (int i = filepath.size() - 1; i >= 0; i--)
	{
		if (lastslash == -1 && (filepath[i] == '/' || filepath[i] == '\\'))
		{
			lastslash = i;
			//break;
		}
		if (lastslash != -1 && secondslash == -1 && (filepath[i] == '/' || filepath[i] == '\\'))
		{
			secondslash = i;
			//break;
		}
		if (lastdot == -1 && filepath[i] == '.')
		{
			lastdot = i;
		}
	}
	ext = filepath.substr(lastdot, filepath.size() - lastdot);
	_name = filepath.substr(lastslash + 1, filepath.size() - (lastslash + 1) - ext.size());
	_dir = filepath.substr(0, secondslash + 1);

}
void ShapeFile::create(std::string filename, OGRSpatialReference* spatialRef, OGRFeatureDefn *poFDefn, OGRwkbGeometryType geotype)
{


	g_mFileName = filename;
	if (poDS)
		GDALClose(poDS);
	const char *pszDriverName = "ESRI Shapefile";
	GDALDriver *poDriver = OGRSFDriverRegistrar::GetRegistrar()->GetDriverByName(
		pszDriverName);
	std::ifstream ifs;
	ifs.open(filename.data());
	if (ifs){
		poDriver->Delete(filename.data());
	}
	ifs.close();

	poDS = poDriver->Create(filename.data(), 0, 0, 0, GDT_Unknown, NULL);
	std::string _dir, _name, ext;
	splitFilepath(filename, _dir, _name, ext);
	if (spatialRef)
	{
		poLayer = poDS->CreateLayer(_name.data(), spatialRef, geotype, NULL);
	}

	if (poFDefn)
	{
		for (int iField = 0; iField < poFDefn->GetFieldCount(); iField++)
		{
			OGRFieldDefn *poFieldDefn = poFDefn->GetFieldDefn(iField);
			poLayer->CreateField(poFieldDefn);
		}
	}

}
int ShapeFile::getOrCreateField(const char* _name, OGRFieldType tp) {
	int idx = poLayer->GetLayerDefn()->GetFieldIndex(_name);
	if (idx > -1)
		return idx;
	OGRFieldDefn field(_name, tp);
	poLayer->CreateField(&field);
	return poLayer->GetLayerDefn()->GetFieldIndex(_name);
}

ShapeFile::ShapeFile()
{
	poDS = NULL;
}

ShapeFile::ShapeFile(std::string filename, int update) {
	g_mFileName = filename;
	poDS = (GDALDataset*)GDALOpenEx(filename.data(), GDAL_OF_VECTOR | update, NULL, NULL, NULL);
	if(poDS)
	   poLayer = poDS->GetLayer(0);
}

ShapeFile::~ShapeFile()
{
	close();
}
