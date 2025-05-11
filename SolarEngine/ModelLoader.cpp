#include "ModelLoader.h"
#include <osgDB/ReadFile>
#include <vector>
#include <sstream>
#include <QtCore/qdir.h>
#include <QtCore/qfile.h>
#include "osg/ComputeBoundsVisitor"
#include "osgUtil/SmoothingVisitor"
#include "osgDB/writeFile"
#include "osgDB/readFile"
#include "osg/MatrixTransform"

ModelLoader::ModelLoader()
{
}


ModelLoader::~ModelLoader()
{

}

std::string getBaseName(std::string path)
{
		char last = path[path.length() - 1];
		if (last == '/' || last == '\\')
		{
				path = path.substr(0, path.length() - 1);
		}
		return QFileInfo(path.data()).baseName().toLocal8Bit().data();
}

std::vector<std::string> findSubdirs(std::string dir)
{
		std::vector<std::string> subdirs;
		QDir rootdir(dir.data());
		rootdir.setFilter(QDir::Dirs | QDir::Hidden | QDir::NoSymLinks | QDir::NoDotAndDotDot);
		rootdir.setSorting(QDir::Name);
		std::vector<std::string> files;
		QFileInfoList list = rootdir.entryInfoList();
		for (int i = 0; i < list.size(); ++i) {
				QFileInfo fileInfo = list.at(i);
				std::string dir = (fileInfo.absoluteFilePath() + "/").toLocal8Bit().data();
				subdirs.push_back(dir);
		}
		return subdirs;
}

std::vector<std::string> findFiles(std::string indir, std::string match)
{
		std::vector<std::string> files;
		QDir input_dir(indir.data());
		input_dir.setFilter(QDir::Files | QDir::Hidden | QDir::NoSymLinks | QDir::NoDotAndDotDot);
		input_dir.setSorting(QDir::Name);
		indir = (input_dir.absolutePath() + "/").toLocal8Bit().data();
		QFileInfoList list = input_dir.entryInfoList();
		for (int i = 0; i < list.size(); ++i) {
				QFileInfo fileInfo = list.at(i);
				std::string input_file = (input_dir.absolutePath() + "/" + fileInfo.fileName()).toLocal8Bit().data();
				if (/*!fileInfo.fileName().contains(match.data(),Qt::CaseInsensitive) || */match != "" && !fileInfo.fileName().endsWith(match.data(), Qt::CaseInsensitive))
						continue;
				files.push_back(fileInfo.absoluteFilePath().toLocal8Bit().data());
		}
		return files;
}

std::vector<std::string> findLeafTileFiles(const std::vector<std::string>& tileFiles, const std::string& masterTileName)
{
		std::vector<int> levels;
		int maxLevel = -1;
		for (size_t i = 0; i < tileFiles.size(); i++)
		{
				std::string tilename = QFileInfo(tileFiles[i].data()).baseName().toLocal8Bit().data();
				if (tilename.length() - masterTileName.length() < 3)
				{
						levels.push_back(-1);
						continue;
				}

				tilename = tilename.substr(masterTileName.length() + 2, tilename.size() - masterTileName.length() - 2);
				std::string levelStr = "";
				for (size_t j = 0; j < tilename.length(); j++)
				{
						if (!isdigit(tilename[j]))
								break;
						levelStr += tilename[j];
				}

				int level = -1;
				std::stringstream ss;
				ss << levelStr;
				ss >> level;
				levels.push_back(level);
				if (maxLevel < level)
						maxLevel = level;
		}

		std::vector<std::string> leafTiles;
		for (size_t i = 0; i < tileFiles.size(); i++)
		{
				if (levels[i] == maxLevel)
				{
						leafTiles.push_back(tileFiles[i]);
				}
		}
		return leafTiles;
}

std::vector<std::string> findMasterTiles(std::string indir)
{
		std::vector<std::string> files;
		std::vector<std::string> dirs = findSubdirs(indir);
		for (size_t i = 0; i < dirs.size(); i++)
		{
				if (dirs[i].find("Tile_") == std::string::npos)
						continue;
				std::string masterTileName = getBaseName(dirs[i]);
				std::string masterTilePath = dirs[i] + masterTileName + ".osgb";
				files.push_back(masterTilePath);
		}
		return files;
}

osg::Node* loadModels(std::string file)
{
	return osgDB::readNodeFile(file);
}

osg::Node* loadModels(std::vector<std::string> files)
{
	osg::Group* scene = new osg::Group;
	for (int i = 0; i<files.size(); i++)
	{
		osg::ref_ptr<osg::Node> node = osgDB::readNodeFile(files[i]);
		if (!node || !node.valid())
			continue;
		node->setName(files[i]);
		scene->addChild(node.get());
	}
	return scene;
}

osg::Node* ModelLoader::LoadModel(std::string path, bool& isIntegratedMesh)
{
		isIntegratedMesh = false;

		if(QFileInfo(path.data()).exists())
				return osgDB::readNodeFile(path);

		std::string dir = path;
		if (!QDir(path.data()).exists())
		{
			std::size_t found = path.find_last_of(".");
			if (found == std::string::npos)
				return nullptr;
			dir = path.substr(0, found) + "/";
		}
		
		if (!QDir(dir.data()).exists())
			return nullptr;

		osg::Node* node = Load3DTiles(dir);
		if (node)
			isIntegratedMesh = true;
		return node;
}

osg::Node* ModelLoader::LoadIntegratedMesh(std::string path, bool translateToOrigin)
{
	std::string dir = path;
	if (!QDir(path.data()).exists())
	{
		std::size_t found = path.find_last_of(".");
		if (found == std::string::npos)
			return nullptr;
		dir = path.substr(0, found) + "/";
	}

	if (!QDir(dir.data()).exists())
		return nullptr;

	osg::ref_ptr<osg::Node> node = Load3DTiles(dir);
	if (!translateToOrigin)
		return node.release();
	osg::ComputeBoundsVisitor cbs;
	node->accept(cbs);
	osg::MatrixTransform* group = new osg::MatrixTransform;
	osg::Vec3 center = cbs.getBoundingBox().center();
	group->setMatrix(osg::Matrix::translate(-center.x(), -center.y(), 0));
	group->addChild(node.get());
	return group;
}

osg::Node* ModelLoader::Load3DTiles(std::string indir)
{
		std::vector<std::string> files = findMasterTiles(indir);
	 return loadModels(files);
}

osg::Node* ModelLoader::Load3DTiles(std::string indir, osg::BoundingBox mask, bool intersects)
{
		double area = (mask.xMax() - mask.xMin()) * (mask.yMax() - mask.yMin());
		std::vector<std::string> files;
		std::vector<std::string> dirs = findSubdirs(indir);
		for (size_t i = 0; i < dirs.size(); i++)
		{
				if (dirs[i].find("Tile_") == std::string::npos)
						continue;
				std::string masterTileName = getBaseName(dirs[i]);
				std::string masterTilePath = dirs[i] + masterTileName + ".osgb";

				osg::ref_ptr<osg::Node> node = osgDB::readNodeFile(masterTilePath);
				osg::ComputeBoundsVisitor cbs;
				node->accept(cbs);
				osg::BoundingBox masterTileBB = cbs.getBoundingBox();
				bool intersectsBB = masterTileBB.intersects(mask);
				osg::BoundingBox intersection = masterTileBB.intersect(mask);
				if ((intersection.xMax() - intersection.xMin()) * (intersection.yMax() - intersection.yMin()) < area * 0.1)
				{
						intersectsBB = false;
				}
				if (intersectsBB != intersects)
						continue;
				files.push_back(masterTilePath);
		}
		return loadModels(files);
}

osg::Node* ModelLoader::Load3DTiles(std::string indir, std::vector<std::string> maskTiles, bool include)
{

		std::set<std::string> tileset;
		for (size_t i = 0; i < maskTiles.size(); i++)
		{
				tileset.insert(maskTiles[i]);
		}

		std::vector<std::string> files;
		std::vector<std::string> dirs = findSubdirs(indir);
		for (size_t i = 0; i < dirs.size(); i++)
		{
				if (dirs[i].find("Tile_") == std::string::npos)
						continue;
				std::string masterTileName = getBaseName(dirs[i]);
				std::string masterTilePath = dirs[i] + masterTileName + ".osgb";
				bool found = false;
				if (tileset.find(masterTileName) != tileset.end())
				{
						found = true;
				}
				if (found != include)
						continue;
				files.push_back(masterTilePath);
		}
		return loadModels(files);
}