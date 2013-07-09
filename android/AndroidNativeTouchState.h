#pragma once

#include "base/TouchInputManager.h"
class GameHolder;

struct AndroidNativeTouchState : public NativeTouchState {
	GameHolder* holder;

	AndroidNativeTouchState(GameHolder* h);

    int maxTouchingCount();

	bool isTouching (int index, glm::vec2* windowCoords);

	bool isMoving (int index);

    int eventSDL(void*) { return 0; }
};
