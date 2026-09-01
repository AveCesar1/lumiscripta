#ifndef UTILS_H
#define UTILS_H

#include <string>
#include <vector>
#include <algorithm>
#include <cctype>
#include <sstream>

using std::string;
using std::vector;

// Trim whitespace from both ends of a string.
inline string trim(const string& s) {
	size_t start = 0;
	while (start < s.size() && std::isspace(static_cast<unsigned char>(s[start]))) ++start;
	size_t end = s.size();
	while (end > start && std::isspace(static_cast<unsigned char>(s[end-1]))) --end;
	return s.substr(start, end - start);
}

// Split a string by a delimiter into a vector of strings.
inline vector<string> split(const string& str, char delimiter) {
	vector<string> out;
	std::stringstream ss(str);
	string item;

	while (std::getline(ss, item, delimiter)) {
		out.push_back(item);
	}

	return out;
}

// Get the file extension from a path, in lowercase. Returns empty string if no extension.
inline string fileExtension(const string& path) {
	auto pos = path.find_last_of('.');
	if (pos == string::npos) return string();
	string ext = path.substr(pos + 1);

	std::transform(
        ext.begin(), 
        ext.end(), 
        ext.begin(),
        [](unsigned char c)
        { return static_cast<char>(std::tolower(c)); }
    );

	return ext;
}

// Format a byte size into a human-readable string (e.g., "1.5 MB").
inline string formatBytes(size_t bytes) {
	const double KB = 1024.0;
	char buf[64];

	if (bytes < 1024) {
		std::snprintf(buf, sizeof(buf), "%zu B", bytes);
		return string(buf);
	} else if (bytes < 1024 * 1024) {
		double v = bytes / KB;
		std::snprintf(buf, sizeof(buf), "%.1f KB", v);
		return string(buf);
	} else if (bytes < 1024ull * 1024ull * 1024ull) {
		double v = bytes / (KB * KB);
		std::snprintf(buf, sizeof(buf), "%.1f MB", v);
		return string(buf);
	} else {
		double v = bytes / (KB * KB * KB);
		std::snprintf(buf, sizeof(buf), "%.1f GB", v);
		return string(buf);
	}
}

#endif /* UTILS_H */