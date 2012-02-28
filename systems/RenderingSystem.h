#pragma once

#include "System.h"
#include "../base/Vector2.h"
#include <GLES2/gl2.h>
#include <map>

typedef int TextureRef;

class NativeAssetLoader {
	public:
		virtual char* decompressPngImage(const std::string& assetName, int* width, int* height) = 0;

		virtual char* loadShaderFile(const std::string& assetName) = 0;
};

struct Color {
	union {
		struct {
			float rgba[4];
		};
		struct {
			float r, g, b, a;
		};
	};

	Color(float _r=1.0, float _g=1.0, float _b=1.0, float _a=1.0):
		r(_r), g(_g), b(_b), a(_a) {}
};
struct RenderingComponent {
	RenderingComponent() : size(1.0f, 1.0f), bottomLeftUV(0, 0), topRightUV(1, 1), hide(false) {}
	Vector2 size;
	Vector2 bottomLeftUV, topRightUV;
	TextureRef texture;
	Color color;
	bool hide;
};

#define theRenderingSystem RenderingSystem::GetInstance()
#define RENDERING(e) theRenderingSystem.Get(e)

UPDATABLE_SYSTEM(Rendering)

public:
void init();

void setWindowSize(int w, int h);

TextureRef loadTextureFile(const std::string& assetName);

void setNativeAssetLoader(NativeAssetLoader* ptr) { assetLoader = ptr; }

public:
static void loadOrthographicMatrix(float left, float right, float bottom, float top, float near, float far, float* mat);
GLuint compileShader(const std::string& assetName, GLuint type);

bool isEntityVisible(Entity e);

private:
int w,h;
/* textures cache */
TextureRef nextValidRef;
std::map<std::string, TextureRef> assetTextures;
std::map<TextureRef, GLuint> textures;

NativeAssetLoader* assetLoader;

/* default (and only) shader */
GLuint defaultProgram;
GLuint uniformMatrix;
};
