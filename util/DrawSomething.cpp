/*
    This file is part of Soupe Au Caillou.

    @author Soupe au Caillou - Jordane Pelloux-Prayer
    @author Soupe au Caillou - Gautier Pelloux-Prayer
    @author Soupe au Caillou - Pierre-Eric Pelloux-Prayer

    Soupe Au Caillou is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, version 3.

    Soupe Au Caillou is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with Soupe Au Caillou.  If not, see <http://www.gnu.org/licenses/>.
*/



#include "DrawSomething.h"

#include "systems/TransformationSystem.h"
#include "systems/RenderingSystem.h"
#include "systems/TextSystem.h"
#include "systems/AnchorSystem.h"

#include "base/Log.h"

#include <glm/glm.hpp>
#include <glm/gtx/vector_angle.hpp>

DrawSomething DrawSomething::instance;

void DrawSomething::Clear() {
    for (auto item : instance.drawPointList) {
        theEntityManager.DeleteEntity(item.first);
    }
    instance.drawPointList.clear();

    for (auto item : instance.drawVec2List) {
        theEntityManager.DeleteEntity(item.first);
    }
    instance.drawVec2List.clear();

    for (auto item : instance.drawVec2TextList) {
        theEntityManager.DeleteEntity(item.first);
    }
    instance.drawVec2TextList.clear();

    for (auto item : instance.drawTriangleList) {
        theEntityManager.DeleteEntity(item.first);
    }
    instance.drawTriangleList.clear();
}

Entity DrawSomething::DrawPoint(const std::string& groupID, const glm::vec2& position,
 const Color & color, const std::string name, Entity vector) {
    if (vector == 0) {
        auto firstUnused = instance.drawPointList.begin();
        for (; firstUnused != instance.drawPointList.end(); ++firstUnused) {
            if (RENDERING(firstUnused->first)->show == false) {
                break;
            }
        }

        if (firstUnused == instance.drawPointList.end()) {
            vector = theEntityManager.CreateEntity(name);
            ADD_COMPONENT(vector, Transformation);
            ADD_COMPONENT(vector, Rendering);

            TRANSFORM(vector)->z = .9f;
            RENDERING(vector)->opaqueType = RenderingComponent::NON_OPAQUE;

            instance.drawPointList.push_back(std::make_pair(vector, groupID));
            vector = instance.drawPointList.back().first;
        } else {
            vector = firstUnused->first;
        }
    }

    TRANSFORM(vector)->size = glm::vec2(0.5f);
    TRANSFORM(vector)->position = position;

    RENDERING(vector)->color = color;//Color(.5, currentDrawPointIndice * 1.f / list.size(), currentDrawPointIndice * 1.f / list.size());
    RENDERING(vector)->show = true;

    return vector;
}
void DrawSomething::DrawPointRestart(const std::string & groupID) {
    for (auto e : instance.drawPointList) {
        if (e.second == groupID) {
            RENDERING(e.first)->show = false;
        }
    }
}




Entity DrawSomething::DrawVec2(const std::string& groupID, const glm::vec2& position, const glm::vec2& size,
 bool useTexture, const std::string name, Entity vector) {
    Entity e = DrawVec2(groupID, position, size, Color(), name, vector);
    if (useTexture) {
        RENDERING(e)->texture = theRenderingSystem.loadTextureFile("fleche");
    }

    return e;
}

Entity DrawSomething::Vec2Text(const std::string& groupID, const glm::vec2& position, const glm::vec2& size,
    const std::string& text, const Color & color, const std::string name, Entity vector) {
    Entity e = DrawVec2(groupID, position, size, color, name, vector);

    {
        Entity t = 0;
        auto firstUnused = instance.drawVec2TextList.begin();
        for (; firstUnused != instance.drawVec2TextList.end(); ++firstUnused) {
            if (TEXT(firstUnused->first)->show ==false) {
                break;
            }
        }
        if (firstUnused == instance.drawVec2TextList.end()) {
            t = theEntityManager.CreateEntity(name + "_text");
            ADD_COMPONENT(t, Transformation);
            ADD_COMPONENT(t, Anchor);
            ANCHOR(t)->z = 0;
            ADD_COMPONENT(t, Text);

            instance.drawVec2TextList.push_back(std::make_pair(t, groupID));
        } else {
            t = firstUnused->first;
        }
        ANCHOR(t)->parent = e;
        TEXT(t)->charHeight = glm::min(TRANSFORM(e)->size.x, 0.5f);
        ANCHOR(t)->position = glm::vec2(0.0f, TEXT(t)->charHeight * 0.5);
        TEXT(t)->text = text;
        TEXT(t)->color = Color(0,0,0);
        TEXT(t)->show = true;
    }
    return e;
}

Entity DrawSomething::DrawVec2(const std::string& groupID, const glm::vec2& position, const glm::vec2& size,
 const Color & color, const std::string name, Entity vector) {
    if (vector == 0) {
        auto firstUnused = instance.drawVec2List.begin();
        for (; firstUnused != instance.drawVec2List.end(); ++firstUnused) {
            if (RENDERING(firstUnused->first)->show ==false) {
                break;
            }
        }

        if (firstUnused == instance.drawVec2List.end()) {
            vector = theEntityManager.CreateEntity(name);
            ADD_COMPONENT(vector, Transformation);
            ADD_COMPONENT(vector, Rendering);

            TRANSFORM(vector)->z = .9f;
            RENDERING(vector)->opaqueType = RenderingComponent::NON_OPAQUE;

            instance.drawVec2List.push_back(std::make_pair(vector, groupID));
           vector = instance.drawVec2List.back().first;
        } else {
            vector = firstUnused->first;
        }
    }

    TRANSFORM(vector)->size = glm::vec2(glm::length(size), .05f);
    TRANSFORM(vector)->rotation = glm::orientedAngle(glm::vec2(1.f, 0.f), glm::normalize(size));
    //LOGV(1, "normalize : " << glm::normalize(size).x << "," << glm::normalize(size).y << " : " << glm::orientedAngle(glm::vec2(1.f, 0.f), glm::normalize(size)));

    float y = TRANSFORM(vector)->size.x * glm::sin(TRANSFORM(vector)->rotation);
    float x = TRANSFORM(vector)->size.x * glm::cos(TRANSFORM(vector)->rotation);
    TRANSFORM(vector)->position = position + glm::vec2(x, y) / 2.f;
    //LOGV(1, "Vector " << vector << ": " << TRANSFORM(vector)->position.x << "," << TRANSFORM(vector)->position.y << " : " << TRANSFORM(vector)->size.x << "," << TRANSFORM(vector)->size.y << " : " << TRANSFORM(vector)->rotation);

    RENDERING(vector)->color = color;
    RENDERING(vector)->show = true;

    return vector;
}
void DrawSomething::DrawVec2Restart(const std::string & groupID) {
    for (auto e : instance.drawVec2List) {
        if (e.second == groupID) {
            RENDERING(e.first)->show = false;
        }
    }
    for (auto e : instance.drawVec2TextList) {
        if (e.second == groupID) {
            TEXT(e.first)->show = false;
        }
    }
}


Entity DrawSomething::DrawTriangle(const std::string& groupID, const glm::vec2& firstPoint, const glm::vec2& secondPoint, const glm::vec2& thirdPoint,
 const Color & color, const std::string name, Entity vector, int dynamicVertices) {
    if (vector == 0) {
        auto firstUnused = instance.drawTriangleList.begin();
        int i = 0;
        for (; firstUnused != instance.drawTriangleList.end(); ++firstUnused) {

            if (RENDERING(firstUnused->first)->show ==false) {
                break;
            }
            ++i;
        }

        if (firstUnused == instance.drawTriangleList.end()) {
            vector = theEntityManager.CreateEntity(name);
            ADD_COMPONENT(vector, Transformation);
            ADD_COMPONENT(vector, Rendering);

            TRANSFORM(vector)->z = .9f;
            RENDERING(vector)->opaqueType = RenderingComponent::NON_OPAQUE;

            dynamicVertices = instance.drawTriangleList.size();
            instance.drawTriangleList.push_back(std::make_pair(vector, groupID));

            vector = instance.drawTriangleList.back().first;
        } else {
            dynamicVertices = firstUnused - instance.drawTriangleList.begin();
            LOGF_IF(i != dynamicVertices, i << " vs " << dynamicVertices);
            vector = firstUnused->first;
        }
    }
    TRANSFORM(vector)->position = glm::vec2(0.);
    TRANSFORM(vector)->size = glm::vec2(1.f);
    RENDERING(vector)->shape = Shape::Triangle;
    RENDERING(vector)->color = color;
    RENDERING(vector)->dynamicVertices = dynamicVertices;

    std::vector<glm::vec2> vert;
    vert.push_back(firstPoint);
    vert.push_back(secondPoint);
    vert.push_back(thirdPoint);
    theRenderingSystem.defineDynamicVertices(RENDERING(vector)->dynamicVertices, vert);
    RENDERING(vector)->show = true;


    return vector;
}
void DrawSomething::DrawTriangleRestart(const std::string & groupID) {
    for (auto e : instance.drawTriangleList) {
        if (e.second == groupID) {
            RENDERING(e.first)->show = false;
        }
    }
}

Entity DrawSomething::DrawRectangle(const std::string& groupID, const glm::vec2& 
    centerPosition, const glm::vec2& size, float rotation, const Color & color, 
    const std::string name, Entity vector) {
    
    if (vector == 0) {
        auto firstUnused = instance.drawRectangleList.begin();
        for (; firstUnused != instance.drawRectangleList.end(); ++firstUnused) {
            if (RENDERING(firstUnused->first)->show ==false) {
                break;
            }
        }

        if (firstUnused == instance.drawRectangleList.end()) {
            vector = theEntityManager.CreateEntity(name);
            ADD_COMPONENT(vector, Transformation);
            ADD_COMPONENT(vector, Rendering);

            TRANSFORM(vector)->z = .9f;
            RENDERING(vector)->opaqueType = RenderingComponent::NON_OPAQUE;
            
            instance.drawRectangleList.push_back(std::make_pair(vector, groupID));
            vector = instance.drawRectangleList.back().first;
        } else {
            vector = firstUnused->first;
        }
    }

    TRANSFORM(vector)->size = size;
    TRANSFORM(vector)->rotation = rotation;
    TRANSFORM(vector)->position = centerPosition;

    RENDERING(vector)->color = color;
    RENDERING(vector)->show = true;

    return vector;
}
void DrawSomething::DrawRectangleRestart(const std::string & groupID) {
    for (auto e : instance.drawRectangleList) {
        if (e.second == groupID) {
            RENDERING(e.first)->show = false;
        }
    }
}
