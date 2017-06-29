#include "SVFDatabasePager.h"
struct DatabasePager::DatabasePagerCompileCompletedCallback : public osgUtil::IncrementalCompileOperation::CompileCompletedCallback
{
	DatabasePagerCompileCompletedCallback(osgDB::DatabasePager* pager, osgDB::DatabasePager::DatabaseRequest* databaseRequest) :
		_pager(pager),
		_databaseRequest(databaseRequest) {}

	virtual bool compileCompleted(osgUtil::IncrementalCompileOperation::CompileSet* /*compileSet*/)
	{
		_pager->compileCompleted(_databaseRequest.get());
		return true;
	}

	osgDB::DatabasePager*                               _pager;
	osg::ref_ptr<osgDB::DatabasePager::DatabaseRequest> _databaseRequest;
};

class DatabasePager::FindCompileableGLObjectsVisitor : public osgUtil::StateToCompile
{
public:
	FindCompileableGLObjectsVisitor(const DatabasePager* pager) :
		osgUtil::StateToCompile(osgUtil::GLObjectsVisitor::COMPILE_DISPLAY_LISTS | osgUtil::GLObjectsVisitor::COMPILE_STATE_ATTRIBUTES),
		_pager(pager),
		_changeAutoUnRef(false), _valueAutoUnRef(false),
		_changeAnisotropy(false), _valueAnisotropy(1.0)
	{
		_assignPBOToImages = _pager->_assignPBOToImages;

		_changeAutoUnRef = _pager->_changeAutoUnRef;
		_valueAutoUnRef = _pager->_valueAutoUnRef;
		_changeAnisotropy = _pager->_changeAnisotropy;
		_valueAnisotropy = _pager->_valueAnisotropy;

		switch (_pager->_drawablePolicy)
		{
		case DatabasePager::DO_NOT_MODIFY_DRAWABLE_SETTINGS:
			// do nothing, leave settings as they came in from loaded database.
			// OSG_NOTICE<<"DO_NOT_MODIFY_DRAWABLE_SETTINGS"<<std::endl;
			break;
		case DatabasePager::USE_DISPLAY_LISTS:
			_mode = _mode | osgUtil::GLObjectsVisitor::SWITCH_ON_DISPLAY_LISTS;
			_mode = _mode | osgUtil::GLObjectsVisitor::SWITCH_OFF_VERTEX_BUFFER_OBJECTS;
			_mode = _mode & ~osgUtil::GLObjectsVisitor::SWITCH_ON_VERTEX_BUFFER_OBJECTS;
			break;
		case DatabasePager::USE_VERTEX_BUFFER_OBJECTS:
			_mode = _mode | osgUtil::GLObjectsVisitor::SWITCH_ON_VERTEX_BUFFER_OBJECTS;
			break;
		case DatabasePager::USE_VERTEX_ARRAYS:
			_mode = _mode & ~osgUtil::GLObjectsVisitor::SWITCH_ON_DISPLAY_LISTS;
			_mode = _mode & ~osgUtil::GLObjectsVisitor::SWITCH_ON_VERTEX_BUFFER_OBJECTS;
			_mode = _mode | osgUtil::GLObjectsVisitor::SWITCH_OFF_DISPLAY_LISTS;
			_mode = _mode | osgUtil::GLObjectsVisitor::SWITCH_OFF_VERTEX_BUFFER_OBJECTS;
			break;
		}

		if (osgDB::Registry::instance()->getBuildKdTreesHint() == osgDB::Options::BUILD_KDTREES &&
			osgDB::Registry::instance()->getKdTreeBuilder())
		{
			_kdTreeBuilder = osgDB::Registry::instance()->getKdTreeBuilder()->clone();
		}
	}

	META_NodeVisitor("osgDB", "FindCompileableGLObjectsVisitor")

		bool requiresCompilation() const { return !empty(); }

	virtual void apply(osg::Geode& geode)
	{
		StateToCompile::apply(geode);

		if (_kdTreeBuilder.valid())
		{
			geode.accept(*_kdTreeBuilder);
		}
	}

	void apply(osg::Texture& _texture)
	{
		StateToCompile::apply(_texture);

		if (_changeAutoUnRef)
		{
			_texture.setUnRefImageDataAfterApply(_valueAutoUnRef);
		}

		if ((_changeAnisotropy && _texture.getMaxAnisotropy() != _valueAnisotropy))
		{
			_texture.setMaxAnisotropy(_valueAnisotropy);
		}
	}

	const DatabasePager*                    _pager;
	bool                                    _changeAutoUnRef;
	bool                                    _valueAutoUnRef;
	bool                                    _changeAnisotropy;
	float                                   _valueAnisotropy;
	osg::ref_ptr<osg::KdTreeBuilder>        _kdTreeBuilder;

protected:

	FindCompileableGLObjectsVisitor& operator = (const FindCompileableGLObjectsVisitor&) { return *this; }
};

void VGEDatabasePager::frame()
{


	//bool firstTime = true;

	osg::ref_ptr<DatabasePager::ReadQueue> read_queue;
	osg::ref_ptr<DatabasePager::ReadQueue> out_queue;


	read_queue = _fileRequestQueue;



	//read_queue->block();


	//
	// delete any children if required.
	//
	if (_deleteRemovedSubgraphsInDatabaseThread/* && !(read_queue->_childrenToDeleteList.empty())*/)
	{
		ObjectList deleteList;
		{
			// Don't hold lock during destruction of deleteList
			OpenThreads::ScopedLock<OpenThreads::Mutex> lock(read_queue->_requestMutex);
			if (!read_queue->_childrenToDeleteList.empty())
			{
				deleteList.swap(read_queue->_childrenToDeleteList);
				read_queue->updateBlock();
			}
		}
	}

	//
	// load any subgraphs that are required.
	//
	osg::ref_ptr<DatabaseRequest> databaseRequest;
	read_queue->takeFirst(databaseRequest);

	bool readFromFileCache = false;

	osg::ref_ptr<FileCache> fileCache = osgDB::Registry::instance()->getFileCache();
	osg::ref_ptr<FileLocationCallback> fileLocationCallback = osgDB::Registry::instance()->getFileLocationCallback();
	osg::ref_ptr<Options> dr_loadOptions;
	std::string fileName;
	int frameNumberLastRequest = 0;
	if (databaseRequest.valid())
	{
		{
			OpenThreads::ScopedLock<OpenThreads::Mutex> drLock(_dr_mutex);
			dr_loadOptions = databaseRequest->_loadOptions;
			fileName = databaseRequest->_fileName;
			frameNumberLastRequest = databaseRequest->_frameNumberLastRequest;
		}
		if (dr_loadOptions.valid())
		{
			if (dr_loadOptions->getFileCache()) fileCache = dr_loadOptions->getFileCache();
			if (dr_loadOptions->getFileLocationCallback()) fileLocationCallback = dr_loadOptions->getFileLocationCallback();

			dr_loadOptions = dr_loadOptions->cloneOptions();
		}
		else
		{
			dr_loadOptions = new osgDB::Options;
		}

		dr_loadOptions->setTerrain(databaseRequest->_terrain);

		// disable the FileCache if the fileLocationCallback tells us that it isn't required for this request.
		if (fileLocationCallback.valid() && !fileLocationCallback->useFileCache()) fileCache = 0;


		// check if databaseRequest is still relevant
		if ((_frameNumber - frameNumberLastRequest) <= 1)
		{


			// do nothing as this thread can handle the load
			if (fileCache.valid() && fileCache->isFileAppropriateForFileCache(fileName))
			{
				if (fileCache->existsInCache(fileName))
				{
					readFromFileCache = true;
				}
			}


		}

	}
	if (databaseRequest.valid())
	{

		// load the data, note safe to write to the databaseRequest since once
		// it is created this thread is the only one to write to the _loadedModel pointer.
		//OSG_NOTICE<<"In DatabasePager thread readNodeFile("<<databaseRequest->_fileName<<")"<<std::endl;
		//osg::Timer_t before = osg::Timer::instance()->tick();


		// assume that readNode is thread safe...
		ReaderWriter::ReadResult rr = readFromFileCache ?
			fileCache->readNode(fileName, dr_loadOptions.get(), false) :
			Registry::instance()->readNode(fileName, dr_loadOptions.get(), false);

		osg::ref_ptr<osg::Node> loadedModel;
		if (rr.validNode()) loadedModel = rr.getNode();
		if (rr.error()) OSG_WARN << "Error in reading file " << fileName << " : " << rr.message() << std::endl;
		if (rr.notEnoughMemory()) OSG_INFO << "Not enought memory to load file " << fileName << std::endl;

		if (loadedModel.valid() &&
			fileCache.valid() &&
			fileCache->isFileAppropriateForFileCache(fileName) &&
			!readFromFileCache)
		{
			fileCache->writeNode(*(loadedModel), fileName, dr_loadOptions.get());
		}

		{
			OpenThreads::ScopedLock<OpenThreads::Mutex> drLock(_dr_mutex);
			if ((_frameNumber - databaseRequest->_frameNumberLastRequest)>1)
			{

				loadedModel = 0;
			}
		}

		//OSG_NOTICE<<"     node read in "<<osg::Timer::instance()->delta_m(before,osg::Timer::instance()->tick())<<" ms"<<std::endl;

		if (loadedModel.valid())
		{
			loadedModel->getBound();

			// find all the compileable rendering objects
			DatabasePager::FindCompileableGLObjectsVisitor stateToCompile(this);
			loadedModel->accept(stateToCompile);

			bool loadedObjectsNeedToBeCompiled = _doPreCompile &&
				_incrementalCompileOperation.valid() &&
				_incrementalCompileOperation->requiresCompile(stateToCompile);

			// move the databaseRequest from the front of the fileRequest to the end of
			// dataToCompile or dataToMerge lists.
			osg::ref_ptr<osgUtil::IncrementalCompileOperation::CompileSet> compileSet = 0;
			if (loadedObjectsNeedToBeCompiled)
			{
				// OSG_NOTICE<<"Using IncrementalCompileOperation"<<std::endl;

				compileSet = new osgUtil::IncrementalCompileOperation::CompileSet(loadedModel.get());
				compileSet->buildCompileMap(_incrementalCompileOperation->getContextSet(), stateToCompile);
				compileSet->_compileCompletedCallback = new DatabasePagerCompileCompletedCallback(this, databaseRequest.get());
				_incrementalCompileOperation->add(compileSet.get(), false);
			}
			{
				OpenThreads::ScopedLock<OpenThreads::Mutex> drLock(_dr_mutex);
				databaseRequest->_loadedModel = loadedModel;
				databaseRequest->_compileSet = compileSet;
			}
			// Dereference the databaseRequest while the queue is
			// locked. This prevents the request from being
			// deleted at an unpredictable time within
			// addLoadedDataToSceneGraph.
			if (loadedObjectsNeedToBeCompiled)
			{
				OpenThreads::ScopedLock<OpenThreads::Mutex> listLock(
					_dataToCompileList->_requestMutex);
				_dataToCompileList->addNoLock(databaseRequest.get());
				databaseRequest = 0;
			}
			else
			{
				OpenThreads::ScopedLock<OpenThreads::Mutex> listLock(
					_dataToMergeList->_requestMutex);
				_dataToMergeList->addNoLock(databaseRequest.get());
				databaseRequest = 0;
			}

		}

		// _pager->_dataToCompileList->pruneOldRequestsAndCheckIfEmpty();
	}


}
void VGEDatabasePager::pause()
{

	for (DatabaseThreadList::iterator dt_itr = _databaseThreads.begin();
	dt_itr != _databaseThreads.end();
		++dt_itr)
	{
		(*dt_itr)->setActive(true);
	}

}
void VGEDatabasePager::resume()
{

	for (DatabaseThreadList::iterator dt_itr = _databaseThreads.begin();
	dt_itr != _databaseThreads.end();
		++dt_itr)
	{
		(*dt_itr)->setActive(false);
	}
}