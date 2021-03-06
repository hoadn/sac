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



#include "Serializer.h"
#include "systems/System.h"
#if SAC_NETWORK
#include "systems/NetworkSystem.h"
#endif
#include <cstring>
#include <sstream>
#include <iomanip>

#define PTR_OFFSET_2_PTR(ptr, offset) ((uint8_t*)ptr + offset)

IProperty::IProperty(hash_t pId, PropertyType::Enum pType, PropertyAttribute::Enum attr, unsigned long pOffset, unsigned pSize) : offset(pOffset), _size(pSize), id(pId), type(pType), attribute(attr) {

}

unsigned IProperty::size(void*) const {
    return _size;
}

bool IProperty::different(void* object, void* refObject) const {
    return (memcmp(PTR_OFFSET_2_PTR(object, offset), PTR_OFFSET_2_PTR(refObject, offset), _size) != 0);
}

int IProperty::serialize(uint8_t* out, void* object) const {
    memcpy(out, PTR_OFFSET_2_PTR(object, offset), _size);
    return _size;
}

int IProperty::deserialize(const uint8_t* in, void* object) const {
    memcpy(PTR_OFFSET_2_PTR(object, offset), in, _size);
    return _size;
}

EntityProperty::EntityProperty(hash_t id, unsigned long offset) : IProperty(id, PropertyType::Entity, PropertyAttribute::None, offset, sizeof(Entity)) {

}

unsigned EntityProperty::size(void*) const {
#if SAC_NETWORK
    return sizeof(unsigned int);
#else
    return sizeof(Entity);
#endif
}

int EntityProperty::serialize(uint8_t* out, void* object) const {
    Entity* e = (Entity*) PTR_OFFSET_2_PTR(object, offset);
#if SAC_NETWORK
    if (NetworkSystem::GetInstancePointer()) {
        unsigned int guid = theNetworkSystem.entityToGuid(*e);
        memcpy(out, &guid, sizeof(guid));
        return sizeof(unsigned int);
    }
#endif
    memcpy(out, e, sizeof(Entity));
    return sizeof(Entity);
}

int EntityProperty::deserialize(const uint8_t* in, void* object) const {
#if SAC_NETWORK
    if (NetworkSystem::GetInstancePointer()) {
        unsigned int guid;
        memcpy(&guid, in, sizeof(guid));
        Entity e = theNetworkSystem.guidToEntity(guid);
        memcpy(PTR_OFFSET_2_PTR(object, offset), &e, sizeof(Entity));
        return sizeof(unsigned int);
    }
#endif
    memcpy(PTR_OFFSET_2_PTR(object, offset), in, sizeof(Entity));
    return sizeof(Entity);
}

StringProperty::StringProperty(hash_t id, unsigned long pOffset) : IProperty(id, PropertyType::String, PropertyAttribute::None, pOffset, 0) {}

unsigned StringProperty::size(void* object) const {
   std::string* a = (std::string*) PTR_OFFSET_2_PTR(object, offset);
   return (a->size() + 1);
}

bool StringProperty::different(void* object, void* refObject) const {
    std::string* a = (std::string*) PTR_OFFSET_2_PTR(object, offset);
    std::string* b = (std::string*) PTR_OFFSET_2_PTR(refObject, offset);
    return (a->compare(*b) != 0);
}

int StringProperty::serialize(uint8_t* out, void* object) const {
    std::string* a = (std::string*) PTR_OFFSET_2_PTR(object, offset);
    uint8_t length = (uint8_t)a->size();
    memcpy(out, &length, 1);
    memcpy(&out[1], a->c_str(), length);
    return length + 1;
}

int StringProperty::deserialize(const uint8_t* in, void* object) const {
    uint8_t length = in[0];
    std::string* a = (std::string*) PTR_OFFSET_2_PTR(object, offset);
    *a = std::string((const char*)&in[1], length);
    return length + 1;
}

Serializer::~Serializer() {
    for (unsigned i=0; i<properties.size(); i++) {
        delete properties[i];
    }
    properties.clear();
}

int Serializer::size(void* object) {
    int s = properties.size();
    for (unsigned i=0; i<properties.size(); i++) {
        s += properties[i]->size(object);
    }
    return s;
}

int Serializer::serializeObject(uint8_t* out, void* object, void* refObject) {
    int s = 0;
    for (unsigned i=0;i <properties.size(); i++) {
        if (refObject) {
            if (!properties[i]->different(object, refObject))
                continue;
        }
        out[s++] = (uint8_t)i;
        s += properties[i]->serialize(&out[s], object);
    }
    return s;
}

int Serializer::serializeObject(uint8_t** out, void* object, void* refObject) {
    int s = 0;
    for (unsigned i=0;i <properties.size(); i++) {
        if (!refObject || properties[i]->different(object, refObject)) {
            unsigned propSize = properties[i]->size(object);
            s += 1 + propSize;
            LOGE_IF (s < 0, "Invalid size");
        }
    }
    if (s == 0)
        return 0;
    *out = new uint8_t[s];
    return serializeObject(*out, object, refObject);
}

int Serializer::deserializeObject(const uint8_t* in, int size, void* object) {
    int index = 0;
    while (index < size) {
        uint8_t prop = in[index++];
        index += properties[prop]->deserialize(&in[index], object);
    }
    return index;
}
