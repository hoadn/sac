#include "AnchorSystem.h"

#include "TransformationSystem.h"
#include <set>
#include <glm/gtx/rotate_vector.hpp>

INSTANCE_IMPL(AnchorSystem);

AnchorSystem::AnchorSystem() : ComponentSystemImpl<AnchorComponent>("Anchor") {
    AnchorComponent tc;
    componentSerializer.add(new EntityProperty("parent", OFFSET(parent, tc)));
    componentSerializer.add(new Property<glm::vec2>("position", OFFSET(position, tc), glm::vec2(0.001, 0)));
    componentSerializer.add(new Property<glm::vec2>("anchor", OFFSET(anchor, tc), glm::vec2(0.001, 0)));
    componentSerializer.add(new Property<float>("rotation", OFFSET(rotation, tc), 0.001));
}

struct CompareParentChain {
    bool operator() (const std::pair<Entity, AnchorComponent*>& t1, const std::pair<Entity, AnchorComponent*>& t2) const {
        // if both have the same parent/none, update order doesn't matter
        if (t1.second->parent == t2.second->parent) {
            return (t1.first < t2.first);
        }
        // if they both have parents
        else if (t1.second->parent && t2.second->parent) {
            const auto p1 = theAnchorSystem.Get(t1.second->parent, false);
            const auto p2 = theAnchorSystem.Get(t2.second->parent, false);
            // p1 or p2 may be null
            if (p1 && p2)
                return operator()
                    (std::make_pair(t1.second->parent, p1), std::make_pair(t2.second->parent, p2));
            else if (p1)
                return false;
            else if (p2)
                return true;
            else
                return (t1.first < t2.first);
        }
        // if only t1 has a parent, update it last
        else if (t1.second->parent) {
            return false;
        }
        // else, only t2 has a parent -> t1 first
        else {
            return true;
        }
    }
};

glm::vec2 AnchorSystem::adjustPositionWithAnchor(const glm::vec2& position, const glm::vec2& anchor) {
    return position - anchor;
}

void AnchorSystem::DoUpdate(float) {
    std::set<std::pair<Entity, AnchorComponent*> , CompareParentChain> cp;

    // sort all, root node first
    std::for_each(components.begin(), components.end(), [&cp] (std::pair<Entity, AnchorComponent*> a) -> void {
        cp.insert(a);
    });

    for (auto p: cp) {
        const auto anchor = p.second;
        if (anchor->parent) {
            const auto pTc = TRANSFORM(anchor->parent);
            auto tc = TRANSFORM(p.first);
            // compute global rotation first
            tc->rotation = pTc->rotation + anchor->rotation;
            // then position
            tc->position = pTc->position
                + glm::rotate(anchor->position, pTc->rotation)
                - glm::rotate(anchor->anchor, tc->rotation);
            // and z
            tc->z = pTc->z + anchor->z;
        }
    }
}

#if SAC_DEBUG
void AnchorSystem::Delete(Entity e) {
    FOR_EACH_ENTITY_COMPONENT(Anchor, child, bc)
        if (bc->parent == e) {
            LOGE("deleting an entity which is parent ! (Entity " << e << "/" << theEntityManager.entityName(e) << " is parent of " << child << '/' << theEntityManager.entityName(child) << ')')
        }
    }
    ComponentSystemImpl<AnchorComponent>::Delete(e);
}
#endif

#if SAC_INGAME_EDITORS
void AnchorSystem::addEntityPropertiesToBar(Entity entity, TwBar* bar) {
    AnchorComponent* tc = Get(entity, false);
    if (!tc) return;
    TwAddVarRW(bar, "position.X", TW_TYPE_FLOAT, &tc->position.x, "group=Anchor precision=3 step=0,01");
    TwAddVarRW(bar, "position.Y", TW_TYPE_FLOAT, &tc->position.y, "group=Anchor precision=3 step=0,01");
    TwAddVarRW(bar, "anchor.X", TW_TYPE_FLOAT, &tc->anchor.x, "group=Anchor precision=3 step=0,01");
    TwAddVarRW(bar, "anchor.Y", TW_TYPE_FLOAT, &tc->anchor.y, "group=Anchor precision=3 step=0,01");
    TwAddVarRW(bar, "rotation", TW_TYPE_FLOAT, &tc->rotation, "group=Anchor step=0,01 precision=3");
    TwAddVarRW(bar, "Z", TW_TYPE_FLOAT, &tc->z, "group=Anchor precision=3 step=0,01");
}
#endif