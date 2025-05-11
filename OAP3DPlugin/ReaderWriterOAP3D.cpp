
#include <osg/Notify>
#include <osg/Group>
#include <osg/Geode>
#include <osg/Geometry>
#include <osg/Texture2D>
#include <osg/Material>
#include <osg/TexEnv>
#include <osg/ref_ptr>
#include <osg/MatrixTransform>
#include <osg/BlendFunc>
#include <osg/TexEnvCombine>
#include <osg/CullFace>

#include <osgDB/Registry>
#include <osgDB/FileUtils>
#include <osgDB/FileNameUtils>
#include <osgDB/ReadFile>

//MIKEC debug only for PrintVisitor
#include <osg/NodeVisitor>

#include <stdlib.h>
#include <string.h>

#include <set>
#include <map>
#include <iostream>
#include <sstream>
#include <limits>
#include <assert.h>
#include <QFileInfo>
#include "ModelLoader.h"
using namespace std;
using namespace osg;

class ReaderWriterOAP3D : public osgDB::ReaderWriter
{
public:

	ReaderWriterOAP3D() {};

    virtual const char* className() const { return "OAP3D Reader"; }

		virtual ReadResult readNode(const std::string& file, const osgDB::ReaderWriter::Options* options) const
		{
			if (!isOAP3D(file))
				return ReadResult::NOT_IMPLEMENTED;
			return ModelLoader::LoadIntegratedMesh(file, false);
		}

		virtual ReadResult readNode(std::istream& fin, const Options* options) const
		{
			/* SERIALIZER();*/
			if (options->getDatabasePathList().size() < 1)
				return ReadResult::FILE_NOT_FOUND;
			std::string dir = options->getDatabasePathList()[0];
			if (dir.empty())
				return ReadResult::FILE_NOT_FOUND;
			if (!isOAP3D(dir))
				return ReadResult::FILE_NOT_FOUND;
			return ModelLoader::LoadIntegratedMesh(dir, false);
		}


		virtual ReadResult readObject(std::istream& fin, const osgDB::ReaderWriter::Options* options = NULL) const
		{
			return readNode(fin, options);
		}

		virtual ReadResult readObject(const std::string& file, const osgDB::ReaderWriter::Options* options = NULL) const
		{
			if(!isOAP3D(file))
				return ReadResult::NOT_IMPLEMENTED;
			return readNode(file, options);
		}
private:
	bool isOAP3D(const std::string& filename) const
	{
		if (filename.size() < 6)
			return false;
		std::string name = filename.substr(filename.size() - 6, 6);
		transform(name.begin(), name.end(), name.begin(), ::tolower);
		if (name != ".oap3d")
			return false;
		return true;
	}
};

// now register with Registry to instantiate the above
// reader/writer.
REGISTER_OSGPLUGIN(oap3d, ReaderWriterOAP3D)