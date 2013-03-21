/*
    This file is part of sac.

    @author Soupe au Caillou - Pierre-Eric Pelloux-Prayer
    @author Soupe au Caillou - Gautier Pelloux-Prayer

    RecursiveRunner is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, version 3.

    RecursiveRunner is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with RecursiveRunner.  If not, see <http://www.gnu.org/licenses/>.
*/
#pragma once

#include "System.h"
#include <functional>

struct CollisionComponent {
    Vector2 previousPosition;
    float previousRotation;
    std::map<Entity, std::function<void(Entity, Entity, Vector2, Vector2)> > collisionCallback;
};

#define theCollisionSystem CollisionSystem::GetInstance()
#define COLLISION(e) theCollisionSystem.Get(e)

UPDATABLE_SYSTEM(Collision)

};