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



#include "TextSystem.h"
#include <ctype.h>
#include <sstream>
#include <iomanip>

#include <glm/glm.hpp>
#include "base/EntityManager.h"
#include "base/Log.h"

#include "AnchorSystem.h"
#include "TransformationSystem.h"
#include "RenderingSystem.h"

#include "util/MurmurHash.h"

const float TextComponent::LEFT = 0.0f;
const float TextComponent::CENTER = 0.5f;
const float TextComponent::RIGHT = 1.0f;

const char InlineImageDelimiter[] = {(char)0xC3, (char)0x97};

// Utility functions
static Entity createRenderingEntity();
static void parseInlineImageString(const std::string& s, std::string* image, float* scale);
static float computePartialStringWidth(TextComponent* trc, size_t from, size_t to, float charHeight, const TextSystem::FontDesc& fontDesc);
static float computeStringWidth(TextComponent* trc, float charHeight, const TextSystem::FontDesc& fontDesc);
static float computeStartX(float stringWidth, const TextComponent* trc);

// System implementation
INSTANCE_IMPL(TextSystem);

TextSystem::TextSystem() : ComponentSystemImpl<TextComponent>(HASH("Text", 0x5763c1af)) {
    TextComponent tc;
    componentSerializer.add(new StringProperty(HASH("text", 0x4106ae4e), OFFSET(text, tc)));
    componentSerializer.add(new Property<hash_t>(HASH("font_name", 0x27b3eedc), OFFSET(fontName, tc)));
    componentSerializer.add(new Property<Color>(HASH("color", 0xccc35cf8), OFFSET(color, tc)));
    componentSerializer.add(new Property<float>(HASH("char_height", 0x39ab1256), OFFSET(charHeight, tc), 0.001f));
    componentSerializer.add(new Property<int>(HASH("flags", 0x3de15a28), OFFSET(flags, tc), 0));
    componentSerializer.add(new Property<int>(HASH("camera_bitmask", 0xa6236824), OFFSET(cameraBitMask, tc)));
    componentSerializer.add(new Property<float>(HASH("positioning", 0x8255fb5), OFFSET(positioning, tc), 0.001f));
    componentSerializer.add(new Property<bool>(HASH("show", 0x77b5dada), OFFSET(show, tc)));
    componentSerializer.add(new Property<int>(HASH("max_line_to_use", 0xf450021d), OFFSET(maxLineToUse, tc)));
    componentSerializer.add(new Property<float>(HASH("blink_off_duration", 0x4384d64b), OFFSET(blink.offDuration, tc), 0.001f));
    componentSerializer.add(new Property<float>(HASH("blink_on_duration", 0xc515a2a3), OFFSET(blink.onDuration, tc), 0.001f));
}

struct CharSequenceToUnicode {
    unsigned char sequence[3];
    unsigned char offsets[3];
    uint32_t unicode; // or "code point"
    enum {
        FirstChar = 0,
        MiddleChar,
        LastChar,
        NewSequence
    } index;

    CharSequenceToUnicode() {
        reset();
    }

    void reset() {
        memset(sequence, 0, sizeof(sequence));
        memset(offsets, 0, sizeof(offsets));
        index = NewSequence;
        unicode = 0;
    }

    bool update(unsigned char newChar) {
        switch (index) {
            case NewSequence: {
                // 1st byte can be in these ranges:
                //    * [00, 7F] -> 1 byte  UTF8 char
                //    * [C2, DF] -> 2 bytes UTF char
                //    * [E0, EF] -> 3 bytes UTF char
                //    * [F0, F4] -> 4 bytes UTF char (unsupported)

                // Char sequence to unicode code point is:
                //    unicode = char0
                //      + (char1 - rangeStart) * 0x40
                //      + (char2 - rangeStart) * 0x1000
                // For a multibyte sequence, charN+1 is read before charN
                memset(sequence, 0, sizeof(sequence));

                if (newChar <= 0x7F) {
                    index = LastChar;
                    memset(offsets, 0, sizeof(offsets));
                } else if (newChar <= 0xDF) {
                    // example: â
                    // 2 bytes: 0xc3 0xa2
                    // unicode code point: U+00E2
                    // texture name: e2_typo
                    index = MiddleChar;
                    offsets[(int)MiddleChar] = 0xc2;
                } else if (newChar <= 0xEF) {
                    // example: 輕
                    // 2 bytes: 0xe8 0xbc 0x95
                    // unicode code point: U+8f15
                    // texture name: 8f15_typo
                    index = FirstChar;
                    offsets[(int)LastChar] =
                        offsets[(int)MiddleChar] = 0x80;
                    offsets[(int)FirstChar] = 0xe0;
                } else {
                    LOGE("Unsupported first byte in UTF8 sequence: 0x" << std::hex << newChar << std::dec);
                }
                break;
            }
            case MiddleChar: {
                index = LastChar;
                break;
            }
            case FirstChar: {
                index = MiddleChar;
                break;
            }
            case LastChar:
            default:
                LOGF_IF(index == LastChar, "Invalid state. Missing call to reset maybe ?");
        }

        // insert char in sequence
        sequence[(int)index] = newChar;

        if (index == LastChar) {
            unicode =
                (sequence[(int)FirstChar] - offsets[(int)FirstChar]) * 0x1000 +
                    (sequence[(int)MiddleChar] - offsets[(int)MiddleChar]) * 0x40 +
                    sequence[(int)LastChar] - offsets[(int)LastChar];
            LOGV_IF(2, (sequence[1] > 0), std::hex << "0x" << (int)sequence[0] << " 0x" << (int)sequence[1] << " 0x" << (int)sequence[2] << " => 0x" << unicode << std::dec);
            return true;
        } else {
            return false;
        }
    }
};

static void adjustLineHorizontalCentering(std::vector<Entity>& charInLine, const float startX, const TextComponent* trc, const TransformationComponent* trans) {
    if (!charInLine.empty()) {
        // real line width
        float leftest = TRANSFORM(charInLine.front())->position.x - TRANSFORM(charInLine.front())->size.x * -0.5f;
        float rightest = TRANSFORM(charInLine.back())->position.x - TRANSFORM(charInLine.back())->size.x * -0.5f;
        float width = rightest - leftest;
        float start = startX + (trans->size.x - width) * trc->positioning;
        float diff = start - startX;

        for (auto e: charInLine) {
            TRANSFORM(e)->position.x += diff;
        }
        charInLine.clear();
    }
}

void TextSystem::DoUpdate(float dt) {
    if (!entityWithComponent.empty() && fontRegistry.empty()) {
        LOGW("Trying to use Text, with no font defined");
        return;
    }

    unsigned letterCount = 0;

    FOR_EACH_ENTITY_COMPONENT(Text, entity, trc)
        const unsigned firstEntity = letterCount;

        // early quit if hidden
        if (!trc->show) {
            continue;
        }

#if SAC_DEBUG
        const TextureRef invalidCharTexture = InvalidTextureRef;
#endif
        // text blinking
        if (trc->blink.onDuration > 0) {
            if (trc->blink.accum >= 0) {
                trc->blink.accum = glm::min(trc->blink.accum + dt, trc->blink.onDuration);
                if (trc->blink.accum == trc->blink.onDuration) {
                    trc->blink.accum = -trc->blink.offDuration;
                }
            } else {
                trc->blink.accum += dt;
                trc->blink.accum = glm::min(trc->blink.accum + dt, 0.0f);
                if (trc->blink.accum >= 0)
                    trc->blink.accum = 0;
                continue;
            }
        }

        // append a caret if needed
        bool caretInserted = false;
        if (trc->caret.speed > 0) {
            trc->caret.dt += dt;
            if (trc->caret.dt > trc->caret.speed) {
                trc->caret.dt = 0;
                trc->caret.show = !trc->caret.show;
            }

            caretInserted = true;
            trc->text.push_back('_');
        }

        // Lookup font description
        auto fontIt = fontRegistry.find(trc->fontName);
        if (fontIt == fontRegistry.end()) {
            LOGE("Text component uses undefined font: '" << INV_HASH(trc->fontName) << "'");
            continue;
        }

        // Cache various attributes
        const FontDesc& fontDesc = fontIt->second;
        unsigned int length = trc->text.length();
        // caret is always inserted for string length calculation,
        // but is not supposed to be always displayed
        if (caretInserted && !trc->caret.show) {
            length--;
        }

        // Add rendering entity if needed
        int missingCount = length - (renderingEntitiesPool.size() - letterCount);
        for (int i=0; i <= missingCount; i++) {
            renderingEntitiesPool.push_back(createRenderingEntity());
        }

        // Read TRANSFORM after potential calls to createRenderingEntity
        // Otherwise trans ptr might become invalid
        const TransformationComponent* trans = TRANSFORM(entity);

        // Determine font size (character height)
        float stringWidth = 0;
        float charHeight = trc->charHeight;
        if (trc->flags & TextComponent::AdjustHeightToFillWidthBit) {
            const float targetWidth = trans->size.x;
            stringWidth = computeStringWidth(trc, 1, fontDesc);
            charHeight = targetWidth / stringWidth;
            // Limit to maxCharHeight if defined
            if (trc->maxCharHeight > 0 ) {
                charHeight = glm::min(trc->maxCharHeight, charHeight);
            }
            stringWidth *= charHeight;
        }

relayout:
        if (stringWidth <= 0) {
            stringWidth = computeStringWidth(trc, charHeight, fontDesc);
        }
        int lineCount = 1, layoutCount = 0;
        // Variables
        const float startX = (trc->flags & TextComponent::MultiLineBit) ?
            (trans->size.x * -0.5f) : computeStartX(stringWidth, trc);
        float x = startX, y = 0;
        bool newWord = true;

#if SAC_DEBUG
        int lastValidCharIndex = -1;
        std::vector<int> invalidLettersTexturePosition;
#endif
        CharSequenceToUnicode seqToUni;
        std::vector<Entity> charInLine;

        // Setup rendering for each individual letter
        for(unsigned int i=0; i<length; i++) {
            // If it's a multiline text, we must compute words/lines boundaries
            if (trc->flags & TextComponent::MultiLineBit) {
                size_t wordEnd = trc->text.find_first_of(" ,:.", i);
                size_t lineEnd = trc->text.find_first_of("\n", i);
                bool newLine = false;
                if (wordEnd == i) {
                    // next letter will be the start of a new word
                    newWord = true;
                } else if (lineEnd == i) {
                    // next letter will be the start of a new line
                    newLine = true;
                } else if (newWord) {
                    if (wordEnd == std::string::npos) {
                        wordEnd = trc->text.length();
                    }
                    // compute length of next word
                    const float w = computePartialStringWidth(trc, i, wordEnd - 1, charHeight, fontDesc);
                    // If it doesn't fit on current line -> start new line
                    if (x + w >= trans->size.x * 0.5) {
                        newLine = true;
                    }
                    newWord = false;
                }
                // Begin new line if requested
                if (newLine) {
                    adjustLineHorizontalCentering(charInLine, startX, trc, trans);
                    lineCount++;
                    y -= 1.2f * charHeight;
                    x = startX;
                    if (lineEnd == i) {
                      continue;
                    }
                }
            }

            unsigned char letter = (unsigned char)trc->text[i];
            int skip = -1;

            if (!seqToUni.update(letter))
                continue;
            uint32_t unicode = seqToUni.unicode;
            seqToUni.reset();
#if SAC_DEBUG
            lastValidCharIndex++;
#endif

            LOGW_IF(renderingEntitiesPool.size() <= letterCount, "Missing rendering text entities");
            const Entity e = renderingEntitiesPool[letterCount];
            if (trc->flags & TextComponent::MultiLineBit) {
                charInLine.push_back(e);
            }
            RenderingComponent* rc = RENDERING(e);
            TransformationComponent* tc = TRANSFORM(e);

            /*
            AnchorComponent ac = ANCHOR(e);
            ac->parent = entity;
            ac->z = 0.001; // put text in front
            ac->position = trans->position;
            */
            rc->show = trc->show;
            rc->cameraBitMask = trc->cameraBitMask;
            rc->flags = RenderingFlags::NonOpaque | RenderingFlags::FastCulling;
#if SAC_INGAME_EDITORS
            rc->highLight = trc->highLight;
#endif
            // At this point, we have the proper unicodeId to display,
            // except if it's an image delimiter
            if (unicode == 0x00D7) {
                size_t next = trc->text.find(InlineImageDelimiter, i+1, 2);
                LOGV(3, "Inline image '" << trc->text.substr(i, next - i + 1) << "'");
                LOGE_IF(next == std::string::npos, "Malformed string, cannot find inline image delimiter: '" << trc->text << "'");
                std::string texture;
                float scale = 1.0f;
                parseInlineImageString(
                    trc->text.substr(i+1, next - 1 - (i+1) + 1), &texture, &scale);
                rc->texture = theRenderingSystem.loadTextureFile(texture.c_str());
                rc->color = Color();
                glm::vec2 size = theRenderingSystem.getTextureSize(texture.c_str());
                tc->size.y = charHeight * scale;
                tc->size.x = tc->size.y * size.x / size.y;
                // skip inline image letters
                skip = next + 1;
            } else {
                if (unicode > fontDesc.highestUnicode) {
#if SAC_DEBUG
                    LOGW("Missing unicode char: "
                        << unicode << "(highest one: " << fontDesc.highestUnicode
                            << ") for string '" << trc->text << "', entity: '"
                    << theEntityManager.entityName(entity) << "'");
#endif
                    unicode = 0;
                }
                const CharInfo& info = fontDesc.entries[unicode];

                tc->size = glm::vec2(charHeight * info.h2wRatio, charHeight);
                // if letter is space, hide it
                if (unicode == 0x20) {
                    rc->show = false;
                } else {
#if SAC_DEBUG
                    if (info.texture == invalidCharTexture) {
                        LOGV(1, "Missing unicode char: 0x" << std::hex << unicode << std::dec);
                        invalidLettersTexturePosition.push_back(lastValidCharIndex);
                    }
#endif
                    rc->texture = info.texture;
                    rc->color = trc->color;
                }
            }
            // Advance position
            letterCount++;
            x += tc->size.x * 0.5f;
            tc->position.x = x;
            tc->position.y = y; // + (inlineImage ? tc->size.x * 0.25 : 0);
            x += tc->size.x * 0.5f;

            // Special case for numbers rendering, add semi-space to group (e.g: X XXX XXX)
            if (trc->flags & TextComponent::IsANumberBit && ((length - i - 1) % 3) == 0) {
                x += fontDesc.entries[(unsigned)'0'].h2wRatio * charHeight * 0.75f;
            }

            // Fastforward to skip some chars (e.g: inline image description)
            if (skip >= 0) {
                i = skip;
            }
        }

        adjustLineHorizontalCentering(charInLine, startX, trc, trans);

        if (trc->maxLineToUse > 0 && lineCount > trc->maxLineToUse && ++layoutCount < 3) {
            float target = charHeight * ((float)trc->maxLineToUse) / lineCount;
            if (target < charHeight) {
                float weight = 0.5;
                charHeight = charHeight * (1 - weight) + target * weight;
                letterCount -= (letterCount - firstEntity);
                stringWidth = 0;
                goto relayout;
            }
        }

        // update position
        AnchorComponent ac;
        ac.parent = entity;
        ac.z = 0.001f; // put text in front

        #if SAC_DEBUG
        LOGW_IF (trans->z + ac.z > 1.0,
            "'" << theEntityManager.entityName(entity) << "' (text='"
                << trc->text << "') has z = " << trans->z << " -> letters won't be visible");
        #endif
        for (unsigned i=firstEntity; i<letterCount; i++) {
            auto* tc = TRANSFORM(renderingEntitiesPool[i]);
            ac.position = tc->position;
            AnchorSystem::adjustTransformWithAnchor(tc, trans, &ac);
        }


#if SAC_DEBUG
        if (invalidLettersTexturePosition.size() > 0) {
            std::stringstream ss;
            ss << "Missing character(s) in string: '";
            const auto offset = ss.str().size();
            ss << trc->text << "', entity: '"
                << theEntityManager.entityName(entity) << "'.";

            std::string str(offset + trc->text.size(), ' ');

            for (auto position : invalidLettersTexturePosition) {
                str[offset + position] = '^';
            }
            //finally, show the log!
            LOGW_EVERY_N(60, ss.str() << std::endl << std::string(LOG_OFFSET(), ' ') << str);
        }
#endif

        // if we appended a caret, remove it
        if (caretInserted) {
            trc->text.resize(trc->text.length() - 1);
        }

#if SAC_INGAME_EDITORS
        trc->highLight = false;
#endif
    END_FOR_EACH()

    if (renderingEntitiesPool.size() > letterCount*2) {
        for (unsigned i=letterCount*2; i<renderingEntitiesPool.size(); i++) {
            theEntityManager.DeleteEntity(renderingEntitiesPool[i]);
        }
        renderingEntitiesPool.resize(letterCount*2);
    }
    for (unsigned i=letterCount; i<renderingEntitiesPool.size(); i++) {
        const Entity e = renderingEntitiesPool[i];
        // ANCHOR(e)->parent = 0;
        RENDERING(e)->show = false;
    }
}

void TextSystem::registerFont(const char* name, const std::map<uint32_t, float>& charH2Wratio) {
    hash_t fontId = Murmur::RuntimeHash(name);
    uint32_t highestUnicode = charH2Wratio.rbegin()->first;

    TextureRef invalidCharTexture = InvalidTextureRef;
    float invalidRatio = 0.5;
    FontDesc font;
    font.highestUnicode = highestUnicode;
    font.entries = new CharInfo[highestUnicode + 1];
    // init
    for (unsigned i=0; i<=highestUnicode; i++) {
        font.entries[i].texture = invalidCharTexture;
        font.entries[i].h2wRatio = invalidRatio;
    }

    for (std::map<uint32_t, float>::const_iterator it=charH2Wratio.begin(); it!=charH2Wratio.end(); ++it) {
        CharInfo& info = font.entries[it->first];
        info.h2wRatio = it->second;
        std::stringstream ss;
        ss.fill('0');
        ss << std::hex << std::setw(2) << it->first << '_' << name;
        info.texture = theRenderingSystem.loadTextureFile(ss.str().c_str());
        if (info.texture == InvalidTextureRef) {
            LOGW("Font '" << name << "' uses unknown texture: '" << ss.str() << "'");
        }
    }
    unsigned space = 0x20;
    unsigned r = glm::min((unsigned)0x72, highestUnicode);
    font.entries[space].h2wRatio = font.entries[r].h2wRatio;
    fontRegistry[fontId] = font;
}

float TextSystem::computeTextComponentWidth(TextComponent* trc) const {
    // Lookup font description
    auto fontIt = fontRegistry.find(trc->fontName);
    if (fontIt == fontRegistry.end()) {
        LOGE("Text component uses undefined font: '" << trc->fontName << "'");
        return 0;
    }
    return computeStringWidth(trc, trc->charHeight, fontIt->second);
}

void TextSystem::Delete(Entity e) {
    /*
    for (Entity subE: renderingEntitiesPool) {
        // if we're deleting all entities, subE could have
        // already been destroyed, without Text system noticing
        auto* ac = theAnchorSystem.Get(subE, false);
        if (ac && ac->parent == e) {
            ac->parent = 0;
            RENDERING(subE)->show = false;
        }
    }
    */
    ComponentSystemImpl<TextComponent>::Delete(e);
}

static Entity createRenderingEntity() {
    Entity e = theEntityManager.CreateEntity(HASH("__/text_letter", 0x1fca5927));
    ADD_COMPONENT(e, Transformation);
    ADD_COMPONENT(e, Rendering);
    // ADD_COMPONENT(e, Anchor);
    return e;
}

static void parseInlineImageString(const std::string& s, std::string* image, float* scale) {
    int idx0 = s.find(',');
    if (image)
        *image = s.substr(0, idx0);
    if (scale) {
        *scale = atof(s.substr(idx0 + 1, s.length() - idx0).c_str());
    }
}

static float computePartialStringWidth(TextComponent* trc, size_t from, size_t toInc, float charHeight, const TextSystem::FontDesc& fontDesc) {
    float width = 0;
    // If it's a number, pre-add grouping spacing
    if (trc->flags & TextComponent::IsANumberBit) {
        float spaceW = fontDesc.entries[(unsigned)'0'].h2wRatio * charHeight * 0.75f;
        int count = toInc - from;
        // count [0, 3] -> 0 space
        // count [4, 6] -> 1 space
        width += glm::max(0, (count-1) / 3) * spaceW;
    }

    CharSequenceToUnicode seqToUni;
    for (unsigned int i=from; i<toInc; i++) {
        unsigned char letter = (unsigned char)trc->text[i];

        if (!seqToUni.update(letter))
            continue;
        const uint32_t unicode = seqToUni.unicode;
        seqToUni.reset();

        if (unicode == 0x00D7) {
            size_t next = trc->text.find(InlineImageDelimiter, i+1, 2);
            LOGE_IF(next == std::string::npos, "Malformed string, cannot find inline image delimiter: '" << trc->text << "'");
            float scale = 0;
            std::string texture;
            parseInlineImageString(
                trc->text.substr(i+1, next - 1 - (i+1) + 1), &texture, &scale);
            glm::vec2 size = theRenderingSystem.getTextureSize(texture.c_str());
            width += (charHeight * scale) * size.x / size.y;
            i = next + 1;
        } else if (unicode == '\n') {

        } else {
            width += fontDesc.entries[unicode].h2wRatio * charHeight;
        }
    }
    return width;
}

static float computeStringWidth(TextComponent* trc, float charHeight, const TextSystem::FontDesc& fontDesc) {
    float width = computePartialStringWidth(trc, 0, trc->text.length(), charHeight, fontDesc);
    LOGV(3, "String width: '" << trc->text << "' = " << width);
    return width;
}

static float computeStartX(float stringWidth, const TextComponent* trc) {
    const float result = -stringWidth * trc->positioning;
    return result;
}

TextSystem::~TextSystem() {
    for (auto & font : _instance->fontRegistry) {
        delete[] font.second.entries;
    }
}
