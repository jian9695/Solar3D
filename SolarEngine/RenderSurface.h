#pragma once

#ifdef _WIN32 || WIN32 
#include <Windows.h>
#endif

#include <osg/Texture2D>
#include <osg/Geometry>
#include <osg/Camera>
#include <osg/Geode>
#include <osg/ClampColor>

struct ColorUB4
{
	unsigned char m_r, m_g, m_b, m_a;
	ColorUB4(unsigned char r, unsigned char g, unsigned char b, unsigned char a)
	{
		m_r = r; m_g = g; m_b = b; m_a = a;
	}
	ColorUB4(osg::Vec4 color)
	{
		m_r = (unsigned char)(color.r() * 255);
		m_g = (unsigned char)(color.g() * 255);
		m_b = (unsigned char)(color.b() * 255);
		m_a = (unsigned char)(color.a() * 255);
	}
};

struct ColorUB3
{
	unsigned char m_r, m_g, m_b;
	ColorUB3(unsigned char r, unsigned char g, unsigned char b)
	{
		m_r = r; m_g = g; m_b = b;
	}
	ColorUB3(osg::Vec4 color)
	{
		m_r = (unsigned char)(color.r() * 255);
		m_g = (unsigned char)(color.g() * 255);
		m_b = (unsigned char)(color.b() * 255);
	}
	ColorUB3(osg::Vec3 color)
	{
		m_r = (unsigned char)(color.x() * 255);
		m_g = (unsigned char)(color.y() * 255);
		m_b = (unsigned char)(color.z() * 255);
	}
};

//Shader help class
class ProgramBinder
{
public:
	ProgramBinder(const std::string& name, osg::StateSet* stateset) 
	{
		initialize(name, stateset);
	}

	ProgramBinder() { m_stateset = nullptr; }

	void initialize(const std::string& name, osg::StateSet* stateset, bool override = false)
	{
		m_program = new osg::Program;
		m_stateset = stateset;
		m_program->setName(name);
		if(override)
			m_stateset->setAttributeAndModes(m_program.get(), osg::StateAttribute::ON | osg::StateAttribute::OVERRIDE);
		else
			m_stateset->setAttributeAndModes(m_program.get(), osg::StateAttribute::ON);
	}

	void setVertexShader(const std::string& source) 
	{ 
		if (!m_stateset)
			return;
		if (!m_vertexShader)
		{
			m_vertexShader = new osg::Shader(osg::Shader::VERTEX, source);
			m_program->addShader(m_vertexShader.get());
		}
		else
		{
			m_vertexShader->setShaderSource(source);
		}
	}

	void setFragmentShader(const std::string& source) 
	{
		if (!m_stateset)
			return;
		if (!m_fragmentShader)
		{
			m_fragmentShader = new osg::Shader(osg::Shader::FRAGMENT, source);
			m_program->addShader(m_fragmentShader.get());
		}
		else
		{
			m_fragmentShader->setShaderSource(source);
		}
	}

	void setProgramName(const std::string& name)
	{ 
		m_program->setName(name.data());
		m_vertexShader->setName((name + ".vertex").data());
		m_fragmentShader->setName((name + ".fragment").data());
	}

	void attach(bool on, bool override = false)
	{
		if (on)
		{
			if (override)
				m_stateset->setAttributeAndModes(m_program.get(), osg::StateAttribute::ON | osg::StateAttribute::OVERRIDE);
			else
				m_stateset->setAttributeAndModes(m_program.get(), osg::StateAttribute::ON);
		}
		else
		{
			if (override)
				m_stateset->setAttributeAndModes(m_program.get(), osg::StateAttribute::OFF);
			else
				m_stateset->setAttributeAndModes(m_program.get(), osg::StateAttribute::OFF);
		}
	}

private:
	osg::ref_ptr<osg::Program> m_program;
	osg::ref_ptr<osg::Shader> m_vertexShader;
	osg::ref_ptr<osg::Shader> m_fragmentShader;
	osg::StateSet* m_stateset;
};

//Class used to render a full-screen rectangle
class ScreenOverlay : public osg::Geode
{
public:
	ScreenOverlay(const char* vertexShader = nullptr, const char* fragmentShader = nullptr);
	~ScreenOverlay();

	void setTextureLayer(osg::Texture* tex, int unit = 0);

	std::string defaultVertexShader()
	{
		static char vertexSource[] =
			"void main(void)\n"
			"{\n"
			"   gl_TexCoord[0] = vec4(gl_Vertex.x*0.5+0.5,gl_Vertex.y*0.5+0.5,0,1);\n"
			"   gl_Position = vec4(gl_Vertex.x,gl_Vertex.y,0,1.0);\n"
			"}\n";
		return vertexSource;
	}

	std::string defaultFragmentShader()
	{
		static char fragmentSource[] =
			"uniform sampler2D texture0;\n"
			"void main(void) \n"
			"{\n"
			"    vec4 color = texture2D(texture0, gl_TexCoord[0].xy);\n"
			"    gl_FragColor = color;\n"
			"}\n";
		return fragmentSource;
	}

	void SetVertexShader(const std::string& source) { m_programBinder.setVertexShader(source); }
	void SetFragmentShader(const std::string& source) { m_programBinder.setFragmentShader(source); }
	void setProgramName(const std::string& name) { m_programBinder.setProgramName(name); }

private:
	ProgramBinder m_programBinder;
};

//Camera used to render to a texture
class RenderSurface : public osg::Camera
{
protected:
	osg::ref_ptr<osg::Texture2D> m_texture;
	osg::ref_ptr<osg::Image> m_image;
public:
	//internalFormat: GL_RGBA,          GL_RGB,           GL_RGB32F_ARB, GL_RGBA32F_ARB, GL_ALPHA32F_ARB
	//sourceFormat:   GL_RGBA,          GL_RGB,           GL_RGB,        GL_RGBA,        GL_ALPHA
	//sourceType:     GL_UNSIGNED_BYTE, GL_UNSIGNED_BYTE, GL_FLOAT,      GL_FLOAT,       GL_FLOAT
	RenderSurface(int width, int height, GLenum internalFormat, GLenum sourceFormat, GLenum sourceType, bool allocateImage)
		: osg::Camera()
	{
		setComputeNearFarMode(osg::CullSettings::DO_NOT_COMPUTE_NEAR_FAR);
		setClearMask(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		setClearColor(osg::Vec4(0, 0, 0, 0));
		setReferenceFrame(osg::Transform::ABSOLUTE_RF_INHERIT_VIEWPOINT);
		setViewport(0, 0, width, height);
		setRenderOrder(osg::Camera::PRE_RENDER);
		setRenderTargetImplementation(osg::Camera::FRAME_BUFFER_OBJECT);

		//orthoCamera->getOrCreateStateSet()->setRenderBinDetails(2,"RenderBin"); 
		m_texture = new osg::Texture2D;
		m_texture->setTextureSize(width, height);
		m_texture->setResizeNonPowerOfTwoHint(false);
		m_texture->setFilter(osg::Texture2D::MIN_FILTER, osg::Texture2D::NEAREST);
		m_texture->setFilter(osg::Texture2D::MAG_FILTER, osg::Texture2D::NEAREST);
		m_texture->setWrap(osg::Texture2D::WRAP_S, osg::Texture2D::CLAMP_TO_EDGE);
		m_texture->setWrap(osg::Texture2D::WRAP_T, osg::Texture2D::CLAMP_TO_EDGE);
		//rtTexture->setDataVariance(osg::Object::DYNAMIC);
		m_texture->setInternalFormat(internalFormat);
		m_texture->setSourceFormat(sourceFormat);
		m_texture->setSourceType(sourceType);
		attach(osg::Camera::COLOR_BUFFER0, m_texture.get());
		if (allocateImage)
		{
			m_image = new osg::Image;
			m_image->allocateImage(width, height, 1, sourceFormat, sourceType);
			attach(osg::Camera::COLOR_BUFFER0, m_image.get());
		}

		osg::ref_ptr<osg::ClampColor> clamp = new osg::ClampColor();
		clamp->setClampVertexColor(GL_FALSE);
		clamp->setClampFragmentColor(GL_FALSE);
		clamp->setClampReadColor(GL_FALSE);
		getOrCreateStateSet()->setAttribute(clamp.get(), osg::StateAttribute::ON | osg::StateAttribute::OVERRIDE);
	}

	osg::ref_ptr<osg::Texture2D> Texture() { return m_texture; }
	osg::ref_ptr<osg::Image> Image() { return m_image; };

	int width() { return m_image->s(); }
	int height() { return m_image->t(); }

	//Resize the camera buffer
	void resize(int newWidth, int newHeight)
	{
		if (!m_texture)
			return;
		if (m_texture->getTextureWidth() == newWidth && m_texture->getTextureHeight() == newHeight)
			return;
		resize(newWidth, newHeight);
		m_texture->setTextureSize(newWidth, newHeight);
		if (m_image)
		{
			m_image->scaleImage(newWidth, newHeight, 1);
		}
	}
};

//Camera with a built-in ScreenOverlay used for rendering to texture
class OverlayRenderSurface : public RenderSurface
{
private:
	osg::ref_ptr<ScreenOverlay> m_overlay;

public:
	//internalFormat: GL_RGBA,          GL_RGB,           GL_RGB32F_ARB, GL_RGBA32F_ARB, GL_ALPHA32F_ARB
	//sourceFormat:   GL_RGBA,          GL_RGB,           GL_RGB,        GL_RGBA,        GL_ALPHA
	//sourceType:     GL_UNSIGNED_BYTE, GL_UNSIGNED_BYTE, GL_FLOAT,      GL_FLOAT,       GL_FLOAT
	OverlayRenderSurface(int width, int height, GLenum internalFormat, GLenum sourceFormat, GLenum sourceType, bool allocateImage)
		: RenderSurface(width, height, internalFormat, sourceFormat, sourceType, allocateImage)
	{
		m_overlay = new ScreenOverlay();
		this->addChild(m_overlay.get());
	}

	ScreenOverlay* overlay() { return m_overlay.get(); };
};

