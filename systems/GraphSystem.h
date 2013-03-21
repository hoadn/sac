#pragma once

#include "System.h"
#include <list>
#include <utility>
#include <map>
#include <string>
#include "opengl/TextureLibrary.h"
#include "util/ImageLoader.h"
#include "base/Color.h"

struct GraphComponent {

    GraphComponent():lineWidth(0), maxY(0), maxX(0), minY(0), minX(0), 
    setFixedScaleMinMaxX(false), setFixedScaleMinMaxY(false), reloadTexture(true),
    lineColor(Color(1, 1, 1)) {}

    std::list<std::pair<float, float> > pointsList;
	
	std::string textureName;
	
	float lineWidth; // between ]0:1] (percent) if 0 -> 1 pixel
	
    float maxY, maxX, minY, minX;

    bool setFixedScaleMinMaxX;
    bool setFixedScaleMinMaxY;
    
    bool reloadTexture;
    
    Color lineColor;
};

#define theGraphSystem GraphSystem::GetInstance()
#define GRAPH(e) theGraphSystem.Get(e)

UPDATABLE_SYSTEM(Graph)

    void drawTexture(ImageDesc &textureDesc, GraphComponent *points);
    void drawLine(ImageDesc &textureDesc, std::pair<int, int> firstPoint, std::pair<int, int> secondPoint, int lineWidth, Color color);

    std::map<TextureRef, ImageDesc> textureRef2Image;
};