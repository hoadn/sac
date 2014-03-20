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

#pragma once

struct OggHandle;

struct FileBuffer;

namespace OggInfo
{
    struct Values {
        float durationSeconds;
        int durationSamples;
        int sampleRate;
        int numChannels;
    };
}

namespace OggOption
{
    enum Decoding {
        Sync,
        Async
    };
}

class OggDecoder {
    public:
        static OggHandle* load(const FileBuffer* fb, OggOption::Decoding d = OggOption::Sync);

        static const FileBuffer* release(OggHandle* handle);

        static int availableSamples(OggHandle* handle);

        static int readSamples(OggHandle* handle, int numSamples, short* output);

        static OggInfo::Values query(OggHandle* handle);

        static int decode(const FileBuffer& fb, short** output, OggInfo::Values& info);
};