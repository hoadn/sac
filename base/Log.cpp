bool AssertOnFatal = true;

#if SAC_ENABLE_LOG

#include "Log.h"

#include "TimeUtil.h"
#include <iomanip>
#include <sstream>

LogVerbosity::Enum logLevel =
#ifdef SAC_ANDROID
    LogVerbosity::INFO;
#else
    LogVerbosity::INFO;
#endif
std::map<std::string, bool> verboseFilenameFilters;

class NullStream : public std::ostream {
public:
    template<class T>
    std::ostream& operator<<(const T& ) {
        return *this;
    }
};
NullStream slashDevslashNull;

static const char* enumNames[] ={
    //5 chars length for all
	"FATAL",
	"ERROR",
    "TODO ",
    "WARN ",
	"INFO ",
	"VERB "
};

static const char* keepOnlyFilename(const char* fullPath) {
	const char* result = fullPath;
	const char* ptr = fullPath;
	while (*ptr++ != '\0') {
		if (*ptr == '\\' || *ptr == '/') result = ptr + 1;
	}
	return result;
}

static const char* enum2Name(LogVerbosity::Enum t) {
	return enumNames[(int)t];
}

int logHeaderLength(const char* file, int line) {
    static std::stringstream ss;
    ss.str("");
    logToStream(ss, LogVerbosity::INFO, file, line);
    return ss.str().size();
}

std::ostream& logToStream(std::ostream& stream, LogVerbosity::Enum type, const char* file, int line) {
	stream << std::fixed << std::setprecision(4) << TimeUtil::GetTime() << ' ' << enum2Name(type) << ' ' << keepOnlyFilename(file) << ':' << line << " : ";
	return stream;
}

std::ostream& vlogToStream(std::ostream& stream, int level, const char* file, int line) {
    const char* trimmed = keepOnlyFilename(file);
    auto it = verboseFilenameFilters.find(trimmed);
    if (it == verboseFilenameFilters.end())
        verboseFilenameFilters.insert(std::make_pair(trimmed, true));
    else if (!it->second)
        return slashDevslashNull;
	stream << std::fixed << std::setprecision(4) << TimeUtil::GetTime() << " VERB-" << level << ' ' << trimmed << ':' << line << " : ";
	return stream;
}

#endif
