
#ifdef _WIN32 || WIN32 
#include <Windows.h>
#endif
#include "SVFDatabasePager.h"
#include "SVFComputeTools.h"
#include "ShapeFile.h"
#include <osg/Notify>
#include <osgGA/GUIEventHandler>
#include <osgGA/StateSetManipulator>
#include <osgViewer/Viewer>
#include <osgEarth/MapNode>
#include <osgEarthUtil/ExampleResources>
#include <osgEarthUtil/EarthManipulator>
#include <osgEarthUtil/AutoClipPlaneHandler>
#include <osgEarthUtil/LogarithmicDepthBuffer>

#include <osgEarthDrivers/tms/TMSOptions>
#include <osgEarthDrivers/xyz/XYZOptions>
#include <osgEarthDrivers/feature_ogr/OGRFeatureOptions>
#include <osgEarthDrivers/model_feature_geom/FeatureGeomModelOptions>
#include <osgEarthDrivers/cache_filesystem/FileSystemCache>
#include <osgEarthDrivers/arcgis/ArcGISOptions>

using namespace osgEarth;
using namespace osgEarth::Drivers;
using namespace osgEarth::Features;
using namespace osgEarth::Symbology;
using namespace osgEarth::Util;

#define ELEVATION_URL    "http://readymap.org/readymap/tiles/1.0.0/9/"
#define BUILDINGS_URL    "./data/boston_buildings_utm19.shp"
#define RESOURCE_LIB_URL "./data/resources/textures_us/catalog.xml"
#define STREETS_URL      "./data/boston-scl-utm19n-meters.shp"
#define PARKS_URL        "./data/boston-parks.shp"
#define TREE_MODEL_URL   "./data/loopix/tree4.osgb"

// forward declarations.
void addImagery(Map* map);
void addElevation(Map* map);
void addBuildings(Map* map);
void addStreets(Map* map);
void addParks(Map* map);

osgEarth::ProfileOptions ProfileOptionsFromFile(std::string filename)
{
	osgEarth::ProfileOptions opt;
	if (filename.substr(filename.length() - 4, 4) == ".shp")
	{
		char srs[512];
		memset(srs, 0, 512);
		char* psrs = (char*)srs;
		ShapeFile shp(filename);
		shp.poLayer->GetSpatialRef()->exportToWkt(&psrs);
		opt.srsString() = psrs;
		OGREnvelope env;
		shp.poLayer->GetExtent(&env);
		opt.bounds() = osgEarth::Bounds(env.MinX, env.MinY, env.MaxX, env.MaxY);
	}
	else
	{
		double adfGeoTransform[6];
		GDALDataset* pDataset = (GDALDataset*)GDALOpen(filename.data(), GA_ReadOnly);
		pDataset->GetGeoTransform(adfGeoTransform);
		int ncols = pDataset->GetRasterXSize();
		int nrows = pDataset->GetRasterYSize();
		opt.bounds() = osgEarth::Bounds(adfGeoTransform[0], adfGeoTransform[3] + adfGeoTransform[5] * nrows, adfGeoTransform[3], adfGeoTransform[0] + adfGeoTransform[1] * ncols);
		opt.srsString() = pDataset->GetProjectionRef();
	}
	return opt;
}


void addImagery(Map* map)
{
	//use readymap imagery
	//TMSOptions imagery;
	//imagery.url() = "http://readymap.org/readymap/tiles/1.0.0/9/";
	//map->addImageLayer(new ImageLayer("TMS imagery", imagery));
	
	//use ArcGIS imagery
	ArcGISOptions arcGISLayer;
	arcGISLayer.url() = "http://services.arcgisonline.com/ArcGIS/rest/services/World_Imagery/MapServer";
	map->addImageLayer( new ImageLayer("ArcGIS", arcGISLayer) );
}


void addElevation(Map* map)
{
	// add a TMS elevation layer:
	TMSOptions elevation;
	elevation.url() = ELEVATION_URL;
	map->addElevationLayer(new ElevationLayer("ReadyMap elevation", elevation));
}

//extrude building footprint polygons into 3D models
void addBuildings(Map* map)
{
	// create a feature source to load the building footprint shapefile.
	OGRFeatureOptions feature_opt;
	feature_opt.name() = "buildings";
	feature_opt.url() = BUILDINGS_URL;
	feature_opt.buildSpatialIndex() = true;

	// a style for the building data:
	Style buildingStyle;
	buildingStyle.setName("buildings");

	// Extrude the shapes into 3D buildings.
	ExtrusionSymbol* extrusion = buildingStyle.getOrCreate<ExtrusionSymbol>();
	extrusion->heightExpression() = NumericExpression("3.5 * max( [story_ht_], 1 )");
	extrusion->flatten() = true;
	extrusion->wallStyleName() = "building-wall";
	extrusion->roofStyleName() = "building-roof";

	PolygonSymbol* poly = buildingStyle.getOrCreate<PolygonSymbol>();
	poly->fill()->color() = Color::White;

	// Clamp the buildings to the terrain.
	AltitudeSymbol* alt = buildingStyle.getOrCreate<AltitudeSymbol>();
	alt->clamping() = alt->CLAMP_TO_TERRAIN;
	alt->binding() = alt->BINDING_VERTEX;

	// a style for the wall textures:
	Style wallStyle;
	wallStyle.setName("building-wall");
	SkinSymbol* wallSkin = wallStyle.getOrCreate<SkinSymbol>();
	wallSkin->library() = "us_resources";
	wallSkin->addTag("building");
	wallSkin->randomSeed() = 1;

	// a style for the rooftop textures:
	Style roofStyle;
	roofStyle.setName("building-roof");
	SkinSymbol* roofSkin = roofStyle.getOrCreate<SkinSymbol>();
	roofSkin->library() = "us_resources";
	roofSkin->addTag("rooftop");
	roofSkin->randomSeed() = 1;
	roofSkin->isTiled() = true;

	// assemble a stylesheet and add our styles to it:
	StyleSheet* styleSheet = new StyleSheet();
	styleSheet->addStyle(buildingStyle);
	styleSheet->addStyle(wallStyle);
	styleSheet->addStyle(roofStyle);

	// load a resource library that contains the building textures.
	ResourceLibrary* reslib = new ResourceLibrary("us_resources", RESOURCE_LIB_URL);
	styleSheet->addResourceLibrary(reslib);

	// set up a paging layout for incremental loading. The tile size factor and
	// the visibility range combine to determine the tile size, such that
	// tile radius = max range / tile size factor.
	FeatureDisplayLayout layout;
	layout.tileSizeFactor() = 52.0;
	layout.addLevel(FeatureLevel(0.0f, 20000.0f, "buildings"));

	// create a model layer that will render the buildings according to our style sheet.
	FeatureGeomModelOptions fgm_opt;
	fgm_opt.featureOptions() = feature_opt;
	fgm_opt.styles() = styleSheet;
	fgm_opt.layout() = layout;

	map->addModelLayer(new ModelLayer("buildings", fgm_opt));
}


void addStreets(Map* map)
{
	// create a feature source to load the street shapefile.
	OGRFeatureOptions feature_opt;
	feature_opt.name() = "streets";
	feature_opt.url() = STREETS_URL;
	feature_opt.buildSpatialIndex() = true;

	// a resampling filter will ensure that the length of each segment falls
	// within the specified range. That can be helpful to avoid cropping 
	// very long lines segments.
	feature_opt.filters().push_back(new ResampleFilter(0.0, 25.0));

	// a style:
	Style style;
	style.setName("streets");

	// Render the data as translucent yellow lines that are 7.5m wide.
	LineSymbol* line = style.getOrCreate<LineSymbol>();
	line->stroke()->color() = Color(Color::Yellow, 0.5f);
	line->stroke()->width() = 7.5f;
	line->stroke()->widthUnits() = Units::METERS;

	// Clamp the lines to the terrain.
	AltitudeSymbol* alt = style.getOrCreate<AltitudeSymbol>();
	alt->clamping() = alt->CLAMP_TO_TERRAIN;

	// Apply a depth offset to avoid z-fighting. The "min bias" is the minimum
	// apparent offset (towards the camera) of the geometry from its actual position.
	// The value here was chosen empirically by tweaking the "oe_doff_min_bias" uniform.
	RenderSymbol* render = style.getOrCreate<RenderSymbol>();
	render->depthOffset()->minBias() = 6.6f;

	// Set up a paging layout. The tile size factor and the visibility range combine
	// to determine the tile size, such that tile radius = max range / tile size factor.
	FeatureDisplayLayout layout;
	layout.tileSizeFactor() = 7.5f;
	layout.maxRange() = 5000.0f;

	// create a model layer that will render the buildings according to our style sheet.
	FeatureGeomModelOptions fgm_opt;
	fgm_opt.featureOptions() = feature_opt;
	fgm_opt.layout() = layout;
	fgm_opt.styles() = new StyleSheet();
	fgm_opt.styles()->addStyle(style);

	map->addModelLayer(new ModelLayer("streets", fgm_opt));
}


void addParks(Map* map)
{
	// create a feature source to load the shapefile.
	OGRFeatureOptions feature_opt;
	feature_opt.name() = "parks";
	feature_opt.url() = PARKS_URL;
	feature_opt.buildSpatialIndex() = true;

	// a style:
	Style style;
	style.setName("parks");

	// Render the data using point-model substitution, which replaces each point
	// in the feature geometry with an instance of a 3D model. Since the input
	// data are polygons, the PLACEMENT_RANDOM directive below will scatter
	// points within the polygon boundary at the specified density.
	ModelSymbol* model = style.getOrCreate<ModelSymbol>();
	model->url()->setLiteral(TREE_MODEL_URL);
	model->scale()->setLiteral(0.2);
	model->placement() = model->PLACEMENT_RANDOM;
	model->density() = 3000.0f; // instances per sqkm

								// Clamp to the terrain:
	AltitudeSymbol* alt = style.getOrCreate<AltitudeSymbol>();
	alt->clamping() = alt->CLAMP_TO_TERRAIN;

	// Since the tree model contains alpha components, we will discard any data
	// that's sufficiently transparent; this will prevent depth-sorting anomolies
	// common when rendering lots of semi-transparent objects.
	RenderSymbol* render = style.getOrCreate<RenderSymbol>();
	render->minAlpha() = 0.15f;

	// Set up a paging layout. The tile size factor and the visibility range combine
	// to determine the tile size, such that tile radius = max range / tile size factor.
	FeatureDisplayLayout layout;
	layout.tileSizeFactor() = 3.0f;
	layout.maxRange() = 10000.0f;

	// create a model layer that will render the buildings according to our style sheet.
	FeatureGeomModelOptions fgm_opt;
	fgm_opt.featureOptions() = feature_opt;
	fgm_opt.layout() = layout;
	fgm_opt.styles() = new StyleSheet();
	fgm_opt.styles()->addStyle(style);
	fgm_opt.compilerOptions().instancing() = true;

	map->addModelLayer(new ModelLayer("parks", fgm_opt));
}


//for compiling and using osgEarth, refer to:
//http://docs.osgearth.org/en/latest/startup.html
//http://docs.osgearth.org/en/latest/

int
main(int argc, char** argv)
{
	osg::ArgumentParser arguments(&argc, argv);
	osgViewer::Viewer* viewer = new osgViewer::Viewer(arguments);
	viewer->setUpViewAcrossAllScreens();
	GDALAllRegister();

	ProfileOptions profileOpt = ProfileOptionsFromFile(PARKS_URL);
	MapOptions mapOpt;
	mapOpt.coordSysType() = MapOptions::CSTYPE_PROJECTED;
	mapOpt.profile() = profileOpt;

	//cache map tiles
	FileSystemCacheOptions cacheOpt;
	cacheOpt.rootPath() = "./cache";
	mapOpt.cache() = cacheOpt;
	mapOpt.cachePolicy() = osgEarth::CachePolicy::USAGE_READ_WRITE;
	Map* map = new Map(mapOpt);

	addImagery(map);
	addElevation(map);
	addStreets(map);
	addBuildings(map);
	addParks(map);

	osg::ref_ptr<EarthManipulator> manip = new EarthManipulator;
	viewer->setCameraManipulator(manip.get());

	osg::ref_ptr<osg::Group> root = new osg::Group();
	viewer->setSceneData(root.get());
	// make the map scene graph:
	MapNode* mapNode = new MapNode(map);
	root->addChild(mapNode);

	// zoom to a good startup position
	manip->setViewpoint(Viewpoint(
		"Home",
		-71.0763, 42.34425, 0,   // longitude, latitude, altitude
		24.261, -21.6, 3450.0), // heading, pitch, range
		5.0);                    // duration

	viewer->addEventHandler(new SkyViewFactorEventHandler(mapNode, root, manip, viewer));

	return viewer->run();
}
