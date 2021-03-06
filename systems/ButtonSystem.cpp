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



#include "ButtonSystem.h"

#include "TransformationSystem.h"
#include "RenderingSystem.h"
#include "CameraSystem.h"

#include "base/TimeUtil.h"

#include "api/VibrateAPI.h"
#include "base/TouchInputManager.h"

#include "util/IntersectionUtil.h"

INSTANCE_IMPL(ButtonSystem);

ButtonSystem::ButtonSystem() : ComponentSystemImpl<ButtonComponent>(HASH("Button", 0x47dde38d)) {
    /* nothing saved */
    vibrateAPI = 0;

    ButtonComponent bc;
    componentSerializer.add(new Property<bool>(HASH("enabled", 0x1d6995b7), OFFSET(enabled, bc)));
    componentSerializer.add(new Property<float>(HASH("over_size", 0x43d1b2c4), OFFSET(overSize, bc), 0.001f));
    componentSerializer.add(new Property<float>(HASH("vibration", 0x53a1f80b), OFFSET(vibration, bc), 0.001f));
    componentSerializer.add(new Property<float>(HASH("trigger", 0xf84abc63), OFFSET(trigger, bc), 0.001f));
    componentSerializer.add(new Property<float>(HASH("first_touch", 0xc1df7f56), OFFSET(firstTouch, bc), 0.001f));
    componentSerializer.add(new Property<int>(HASH("type", 0xf3ebd1bf), OFFSET(type, bc)));
    componentSerializer.add(new Property<TextureRef>(HASH("texture_active", 0xcf6bf927), PropertyType::Texture, OFFSET(textureActive, bc), 0));
    componentSerializer.add(new Property<TextureRef>(HASH("texture_inactive", 0xb56b2c7d), PropertyType::Texture, OFFSET(textureInactive, bc), 0));
}

void ButtonSystem::DoUpdate(float) {
    bool touch = theTouchInputManager.isTouched(0);

    #if SAC_DEBUG
    if (theCameraSystem.entityCount() > 1) {
        LOGW_EVERY_N(600, "Button system uses world position based on the first camera");
    }
    #endif

    const glm::vec2& pos = theTouchInputManager.getTouchLastPosition(0);


    theButtonSystem.forEachECDo([&] (Entity e, ButtonComponent *bt) -> void {
        UpdateButton(e, bt, touch, pos);
    });
}

void ButtonSystem::UpdateButton(Entity entity, ButtonComponent* comp, bool touching, const glm::vec2& touchPos) {
    if (!comp->enabled) {
        auto* rc = theRenderingSystem.Get(entity, false);
        if (rc && rc->show && rc->texture == InvalidTextureRef)
            rc->texture = comp->textureInactive;
        comp->mouseOver = comp->clicked = comp->touchStartOutside = false;
        return;
    }

    comp->clicked = false;

    if (!touching)
        comp->touchStartOutside = false;

    const auto* tc = TRANSFORM(entity);
    const glm::vec2& pos = tc->position;
    const glm::vec2& size = tc->size;

    bool over = touching && IntersectionUtil::pointRectangle(touchPos, pos, size * comp->overSize, tc->rotation);

    auto* rc = theRenderingSystem.Get(entity, false);
    if (rc) {
        if (comp->textureActive != InvalidTextureRef) {
            #ifdef SAC_DEBUG
            if (oldTexture.find(entity) != oldTexture.end() && rc->texture != oldTexture[entity])
                LOGW("Texture is changed in another place! Current=" << rc->texture << "!= old=" << oldTexture[entity]);
            #endif

            // Adapt texture to button state
            if (touching && over && !comp->touchStartOutside)
                rc->texture = comp->textureActive;
            else
                rc->texture = comp->textureInactive;

            #ifdef SAC_DEBUG
            oldTexture[entity] = rc->texture;
            #endif
        }
    }
    // If button is enabled and we have clicked on button at beginning
    if (comp->enabled && !comp->touchStartOutside) {
        // If we are clicking on the button
        if (comp->mouseOver) {
            if (touching) {
                comp->mouseOver = over;
                if (comp->type == ButtonComponent::LONGPUSH) {
                    float t =TimeUtil::GetTime();
                    if (comp->firstTouch == 0) {
                        comp->firstTouch = t;
                    }
                    LOGI_EVERY_N(100, t-comp->firstTouch);
                    if (t - comp->firstTouch > comp->trigger) {
                        comp->firstTouch = 0;
                        comp->lastClick = t;
                        comp->clicked = true;
                    }
                }
            } else {
                if (!comp->touchStartOutside) {
                    float t =TimeUtil::GetTime();
                    // at least 50 ms between 2 clicks

                    if (t - comp->lastClick > .05) {
                        if (comp->type == ButtonComponent::NORMAL) {
                            comp->lastClick = t;
                            comp->clicked = true;
                        }

                        LOGI("Entity '" << theEntityManager.entityName(entity) << "' clicked");

                        if (vibrateAPI && comp->vibration > 0) {
                            vibrateAPI->vibrate(comp->vibration);
                        }
                    }

                    comp->firstTouch = 0;
                }

                comp->mouseOver = false;
            }
        } else {
            comp->touchStartOutside = touching & !over;
            comp->mouseOver = touching & over;

            comp->firstTouch = 0;
        }
    } else {
        comp->mouseOver = false;
    }
}
