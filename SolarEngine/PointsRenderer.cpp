#ifdef _WIN32 || WIN32 
#include <Windows.h>
#endif

#include "PointsRenderer.h"
#include <osg/Point>
#include <osg/BlendFunc>
#include <osg/Texture2D>
#include <osg/PointSprite>
#include <osg/PolygonMode>
#include <osg/Geometry>
#include <osgText/Font>
#include <osgText/Text>
#include <osg/PolygonOffset>
#include <osgDB/WriteFile>
#include <fstream>
#include "RenderSurface.h"

void PointsRenderer::pushPointInternal(const SolarRadiationPoint& point)
{
	static osg::ref_ptr<osg::Program> pointsShader = nullptr;
	if (!pointsShader)
	{
		pointsShader = new osg::Program;
		char vertexShaderSource[] =
			"uniform float pointSize;\n"
			"void main(void)\n"
			"{\n"
			"   gl_PointSize = pointSize;\n"
			"   gl_Position = gl_ModelViewProjectionMatrix *  gl_Vertex;\n"
			"}\n";
		char fragmentShaderSource[] =
			"void main(void) \n"
			"{\n"
			"     gl_FragColor = vec4(1,0,0,1);\n"
			"}\n";
		pointsShader->addShader(new osg::Shader(osg::Shader::FRAGMENT, fragmentShaderSource));
		pointsShader->addShader(new osg::Shader(osg::Shader::VERTEX, vertexShaderSource));
		getOrCreateStateSet()->setMode(GL_VERTEX_PROGRAM_POINT_SIZE, osg::StateAttribute::OVERRIDE | osg::StateAttribute::ON);
		getOrCreateStateSet()->setRenderBinDetails(6000, "RenderBin");
	}

	static osg::ref_ptr<osg::Program> textsShader = nullptr;
	if (!textsShader)
	{
		textsShader = new osg::Program;
		char vertexShaderSource[] =
			"void main(void)\n"
			"{\n"
			"   gl_Position = gl_ModelViewProjectionMatrix *  gl_Vertex;\n"
			"   gl_Position.z += 0.00001;\n"
			"}\n";
		textsShader->addShader(new osg::Shader(osg::Shader::VERTEX, vertexShaderSource));
	}

	std::string label = Utils::value2String(point.m_global / 1000, 3);
	m_doStack.push_back(Action(ActionTypeEnum::PUSH, point));

	osg::ref_ptr<osg::Vec3Array> vertices = new osg::Vec3Array;
	osg::ref_ptr<osg::Geometry> polyGeom = new osg::Geometry;
	vertices->push_back(point.m_pos);
	polyGeom->setVertexArray(vertices.get());
	polyGeom->addPrimitiveSet(new osg::DrawArrays(osg::PrimitiveSet::POINTS, 0, vertices->size()));
	polyGeom->getOrCreateStateSet()->setAttribute(pointsShader.get(), osg::StateAttribute::ON | osg::StateAttribute::OVERRIDE);

	osg::ref_ptr<osg::Geode> pointNear = new osg::Geode;
	pointNear->addDrawable(polyGeom.get());
	pointNear->setCullingActive(false);
	osg::ref_ptr<osg::Uniform> pointSizeNear = new osg::Uniform("pointSize", 10.0f);
	pointNear->getOrCreateStateSet()->addUniform(pointSizeNear.get());

	osg::ref_ptr<osgText::Text> text = new osgText::Text;
	text->setColor(osg::Vec4(0, 0, 0, 1));
	text->setFont("fonts/arial.ttf");
	text->setCharacterSize(20);
	text->setPosition(point.m_pos);
	text->setAxisAlignment(osgText::Text::SCREEN);
	text->setCharacterSizeMode(osgText::Text::SCREEN_COORDS);
	text->setText(label);
	text->setBackdropColor(osg::Vec4(1, 1, 0, 1));
	text->setBackdropType(osgText::Text::BackdropType::OUTLINE);

	osg::ref_ptr <TextLOD> lod = new TextLOD(point);
	lod->getOrCreateStateSet()->setMode(GL_DEPTH_TEST, osg::StateAttribute::OVERRIDE | osg::StateAttribute::OFF);
	lod->setRangeMode(osg::LOD::PIXEL_SIZE_ON_SCREEN);
	//lod->addChild(pointFar.get(), 0, 1000);
	lod->addChild(pointNear.get(), 0, 10000000000);
	lod->addChild(text.get(), 0, 10000000000);
	addChild(lod.get());
}

void PointsRenderer::pushPoint(SolarRadiationPoint& point, const osg::Image* img)
{
	point.m_id = m_pointId++;
	if (img)
	{
		osg::ref_ptr<osg::Image> imageCopy = new osg::Image;
		imageCopy->allocateImage(img->s(), img->t(), img->r(), img->getPixelFormat(), img->getDataType(), img->getPacking());
		memcpy(imageCopy->data(), img->data(), img->getImageSizeInBytes());
		imageCopy->scaleImage(256, 256, 1);
		m_imagesMap[point.m_id] = imageCopy;
	}

	m_doStack.push_back(Action(ActionTypeEnum::PUSH, point));
	pushPointInternal(point);
}

void PointsRenderer::popPointInternal()
{
	int numChildren = getNumChildren();
	if (numChildren < 1)
		return;
	TextLOD* lod = dynamic_cast<TextLOD*>(getChild(numChildren - 1));
	removeChild(numChildren - 1, 1);
}

void PointsRenderer::popPoint()
{
	int numChildren = getNumChildren();
	if (numChildren < 1)
		return;
	TextLOD* lod = dynamic_cast<TextLOD*>(getChild(numChildren - 1));
	m_undoStack.push_back(Action(ActionTypeEnum::POP, lod->_point));
	removeChild(numChildren - 1, 1);
}

void PointsRenderer::undo()
{
	if (m_doStack.size() == 0)
		return;
	Action oldAction = m_doStack.back();
	m_doStack.pop_back();
	Action newAction = oldAction;
	if (oldAction.actionType == ActionTypeEnum::POP)
	{
		newAction.actionType = ActionTypeEnum::PUSH;
	}
	else
	{
		newAction.actionType = ActionTypeEnum::POP;
	}
	m_undoStack.push_back(newAction);

	popPoint();
}

void PointsRenderer::redo()
{
	if (m_undoStack.size() == 0)
		return;
	Action oldAction = m_undoStack.back();
	Action newAction = oldAction;
	if (oldAction.actionType == ActionTypeEnum::POP)
	{
		newAction.actionType = ActionTypeEnum::PUSH;
	}
	else
	{
		newAction.actionType = ActionTypeEnum::POP;
	}
	m_undoStack.pop_back();
	m_doStack.push_back(newAction);
	pushPoint(oldAction);
}

void PointsRenderer::performAction(const Action& action)
{
	if (action.actionType == POP) 
	{
		popPointInternal();
	}
	else
	{
		pushPointInternal(action);
	}
}

void PointsRenderer::exportPoints(const std::string& filename)
{
	int numChildren = getNumChildren();
	if (numChildren < 1)
		return;

	std::ofstream ofs(filename);
	ofs << "lat,lon,elev,3D position,start day,end day,time step,linke,slope,aspect,global[kWh/m2],beam[kWh/m2],diffuse[kWh/m2],reflected[kWh/m2],SkyViewFactor\n";
	for (int i = 0; i < numChildren; i++)
	{
		TextLOD* lod = dynamic_cast<TextLOD*>(getChild(i));
		if (!lod)
			continue;
		const SolarRadiationPoint& point = lod->_point;
		std::vector<OuputVariable> outputVariables;
		outputVariables.push_back(OuputVariable(point.m_lat));
		outputVariables.push_back(OuputVariable(point.m_lon));
		outputVariables.push_back(OuputVariable(point.m_elev));
		outputVariables.push_back(OuputVariable(point.m_pos));
		outputVariables.push_back(OuputVariable(point.m_startDay));
		outputVariables.push_back(OuputVariable(point.m_endDay));
		outputVariables.push_back(OuputVariable(point.m_time_step));
		outputVariables.push_back(OuputVariable(point.m_linke));
		outputVariables.push_back(OuputVariable(point.m_slope));
		outputVariables.push_back(OuputVariable(point.m_aspect));
		outputVariables.push_back(OuputVariable(point.m_global / 1000));
		outputVariables.push_back(OuputVariable(point.m_beam / 1000));
		outputVariables.push_back(OuputVariable(point.m_diffuse / 1000));
		outputVariables.push_back(OuputVariable(point.m_reflected / 1000));
		outputVariables.push_back(OuputVariable(point.m_svf));
		for (int v = 0; v < outputVariables.size(); v++)
		{
			outputVariables[v].out(ofs);
			if (v != outputVariables.size())
			{
				ofs << ",";
			}
		}
		ofs << "\n";
		//ofs << std::setprecision(5);
	}
	ofs.close();
}

void PointsRenderer::postDrawUpdate()
{
	int numChildren = getNumChildren();
	if (numChildren < 1)
		return;

	for (int i = 0; i < numChildren; i++)
	{
		TextLOD* lod = dynamic_cast<TextLOD*>(getChild(i));
		if (!lod)
			continue;
		osg::Geode* pointNode = (osg::Geode*)lod->getChild(0);
		osgText::Text* textNode = (osgText::Text*)lod->getChild(1);
		const SolarRadiationPoint& point = lod->_point;
		osg::Vec3d pos = lod->_point.m_pos;
		osg::Vec3d eye, center, up;
		m_sceneCamera->getViewMatrixAsLookAt(eye, center, up);
		double distTocamera = (eye - pos).length();
		osg::Vec4d vertex = osg::Vec4d(pos, 1.0) * m_sceneCamera->getViewMatrix() * m_sceneCamera->getProjectionMatrix();
		osg::Vec3d screenCoords = osg::Vec3d(vertex.x() / vertex.w(), vertex.y() / vertex.w(), vertex.z() / vertex.w());
		//double fov, aspect, znear, zfar;
		//m_sceneCamera->getProjectionMatrixAsPerspective(fov, aspect, znear, zfar);
		screenCoords.z() = (screenCoords.z() + 1.0) * 0.5;
		osg::Vec2 uv = osg::Vec2(screenCoords.x() * 0.5 + 0.5, screenCoords.y() * 0.5 + 0.5);
		osg::Matrixd projInverse = osg::Matrixd::inverse(m_sceneCamera->getProjectionMatrix());
		osg::Matrixd viewInverse = osg::Matrixd::inverse(m_sceneCamera->getViewMatrix());
		//osg::Vec3d p1 = Utils::WorldPosFromDepth(screenCoords.z(), projInverse, viewInverse, uv.x(), uv.y());
		//float sceneDepth = m_sceneDepthImage->getColor(osg::Vec2(uv.x(), uv.y())).r();
		int imgx = (int)(uv.x() * m_sceneDepthImage->s());
		int imgy = (int)(uv.y() * m_sceneDepthImage->t());
		bool isVisible = isPointVisible(projInverse, viewInverse, eye, uv, distTocamera);
		bool textVisible = isVisible;
		if (distTocamera > 300)
		{
			if (!m_toggleTextDisplay)
				textVisible = false;
		}
		textNode->setNodeMask(textVisible);
		pointNode->setNodeMask(isVisible);
	}
}

bool PointsRenderer::queryPoint(const float& mouseX, const float& mouseY, SolarRadiationPoint& solarPoint)
{
	int numChildren = getNumChildren();
	if (numChildren < 1)
		return false;
	osg::Vec2 normalizedMousePos(mouseX, mouseY);
	osg::Vec2 mousePos(normalizedMousePos.x() * m_sceneDepthImage->s(), normalizedMousePos.y() * m_sceneDepthImage->t());
	osg::Vec3d eye, center, up;
	m_sceneCamera->getViewMatrixAsLookAt(eye, center, up);
	osg::Matrixd projInverse = osg::Matrixd::inverse(m_sceneCamera->getProjectionMatrix());
	osg::Matrixd viewInverse = osg::Matrixd::inverse(m_sceneCamera->getViewMatrix());
	bool found = false;
	float minDist = 25; //select points within a distance of 25 pixels
	for (int i = 0; i < numChildren; i++)
	{
		TextLOD* lod = dynamic_cast<TextLOD*>(getChild(i));
		if (!lod)
			continue;
		const SolarRadiationPoint& point = lod->_point;
		osg::Vec3d pos = lod->_point.m_pos;
		double distTocamera = (eye - pos).length();
		osg::Vec4d vertex = osg::Vec4d(pos, 1.0) * m_sceneCamera->getViewMatrix() * m_sceneCamera->getProjectionMatrix();
		osg::Vec2 normalizedScreenPos(vertex.x() / vertex.w(), vertex.y() / vertex.w());
		osg::Vec2 screenPos(normalizedScreenPos.x() * m_sceneDepthImage->s(), normalizedScreenPos.y() * m_sceneDepthImage->t());
		osg::Vec2 uv(normalizedScreenPos.x() * 0.5 + 0.5, normalizedScreenPos.y() * 0.5 + 0.5);
		if (!isPointVisible(projInverse, viewInverse, eye, uv, distTocamera))
			continue;
		float dist = (mousePos - screenPos).length();
		if (dist < minDist)
		{
			found = true;
			minDist = dist;
			solarPoint = lod->_point;
		}
	}
	return found;
}

bool PointsRenderer::isPointVisible(const osg::Matrixd& projInverse, const osg::Matrixd& viewInverse, const osg::Vec3d& eye, const osg::Vec2& uv, double distTocamera, int windowSize)
{
	int imgx = (int)(uv.x() * m_sceneDepthImage->s());
	int imgy = (int)(uv.y() * m_sceneDepthImage->t());
	bool isVisible = false;
	for (int xoffset = -windowSize; xoffset < windowSize; xoffset++)
	{
		float x = (imgx + xoffset) / (float)m_sceneDepthImage->s();
		if (x < 0 || x > 1.0)
			continue;
		for (int yoffset = -windowSize; yoffset < windowSize; yoffset++)
		{
			float y = (imgy + yoffset) / (float)m_sceneDepthImage->t();
			if (y < 0 || y > 1.0)
				continue;
			float sceneDepth = m_sceneDepthImage->getColor(osg::Vec2(x, y)).r();
			osg::Vec3d p2 = Utils::worldPosFromDepth(sceneDepth, projInverse, viewInverse, uv);
			double distTocamera2 = (p2 - eye).length() + 0.5; //add an offset of 0.5 to avoid z fighting
			if (distTocamera < distTocamera2)
			{
				isVisible = true;
				break;
			}
		}
	}

	return isVisible;
}

osg::Image* PointsRenderer::getFisheyeForPoint(const int& pointId)
{
	if (m_imagesMap.find(pointId) == m_imagesMap.end())
		return NULL;
	return m_imagesMap[pointId].get();
}