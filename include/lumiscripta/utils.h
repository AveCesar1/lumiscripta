#ifndef UTILS_H
#define UTILS_H

#include <string>
#include <vector>
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <sstream>
#include <fstream>

// Platform header needed to ask the OS where our executable lives.
#if defined(_WIN32)
#	ifndef WIN32_LEAN_AND_MEAN
#		define WIN32_LEAN_AND_MEAN
#	endif
#	ifndef NOMINMAX
#		define NOMINMAX
#	endif
#	include <windows.h>
#elif defined(__APPLE__)
#	include <mach-o/dyld.h>
#	include <cstdint>
#	include <unistd.h>
#else
#	include <unistd.h>
#endif

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

// ---------------------------------------------------------------------------
// Paths
// ---------------------------------------------------------------------------

// Directory part of a path ("a/b/c.ttf" -> "a/b"). "." if there is none.
inline string parentDirectory(const string& path) {
	const size_t pos = path.find_last_of("/\\");
	if (pos == string::npos) return string(".");
	if (pos == 0) return string("/");
	return path.substr(0, pos);
}

// Join two path components, tolerating a trailing separator in 'dir'.
inline string joinPath(const string& dir, const string& name) {
	if (dir.empty()) return name;
	if (name.empty()) return dir;
	const char last = dir[dir.size() - 1];
	if (last == '/' || last == '\\') return dir + name;
	return dir + "/" + name;
}

// True if 'path' can be opened for reading.
inline bool pathExists(const string& path) {
	if (path.empty()) return false;
	std::ifstream in(path.c_str(), std::ios::in | std::ios::binary);
	return in.is_open();
}

// Absolute directory holding the running executable. Returns an empty string
// if the OS will not tell us — assetDirs() then simply skips those candidates
// instead of looking for assets in a bogus place.
inline string executableDir() {
#if defined(_WIN32)
	char buffer[MAX_PATH];
	const DWORD n = GetModuleFileNameA(NULL, buffer, (DWORD)sizeof(buffer));
	if (n == 0 || n >= sizeof(buffer)) return string();
	return parentDirectory(string(buffer, (size_t)n));
#elif defined(__APPLE__)
	char buffer[1024];
	uint32_t size = (uint32_t)sizeof(buffer);
	if (_NSGetExecutablePath(buffer, &size) != 0) return string();
	return parentDirectory(string(buffer));
#elif defined(__linux__) || defined(__FreeBSD__)
	char buffer[1024];
	const ssize_t n = readlink("/proc/self/exe", buffer, sizeof(buffer) - 1);
	if (n <= 0) return string();
	buffer[n] = '\0';
	return parentDirectory(string(buffer));
#else
	return string();
#endif
}

// ---------------------------------------------------------------------------
// Assets
//
// Assets (fonts, icons) ship in assets/ next to the source tree, but the
// working directory is whatever the shell, the file manager or the OS decided
// it should be: launching from a desktop icon usually gives "/" or $HOME, and
// the app then looks for assets in a folder that does not contain them. So we
// find the executable first and keep the working directory as a last resort.
// ---------------------------------------------------------------------------

// Ordered list of directories that may contain the asset files (fonts/, ...),
// best match first.
inline vector<string> assetDirs() {
	vector<string> dirs;

	// 1. Explicit override — packaging, tests, unusual installs.
	const char* env = std::getenv("LUMISCRIPTA_ASSETS");
	if (env && *env) dirs.push_back(env);

	// 2. Relative to the executable: installed beside it, installed with the
	//    binary in bin/ and the assets in ../share/lumiscripta/, or a binary
	//    sitting inside the project tree (build/, bin/).
	const string exe = executableDir();
	if (!exe.empty()) {
		dirs.push_back(joinPath(exe, "assets"));
		dirs.push_back(joinPath(parentDirectory(exe), "share/lumiscripta/assets"));
		dirs.push_back(joinPath(exe, "../assets"));
		dirs.push_back(joinPath(exe, "../../assets"));
	}

	// 3. System-wide installs.
	dirs.push_back("/usr/share/lumiscripta/assets");
	dirs.push_back("/usr/local/share/lumiscripta/assets");

	// 4. The working directory — the historical behaviour, kept for `make run`.
	dirs.push_back("assets");

	return dirs;
}

// Resolve a path relative to the asset folders, e.g. "fonts/Inter-Regular.ttf".
// Returns an empty string when nothing matches; callers are expected to cope.
inline string resolveAsset(const string& relative) {
	const vector<string> dirs = assetDirs();
	for (size_t i = 0; i < dirs.size(); ++i) {
		const string candidate = joinPath(dirs[i], relative);
		if (pathExists(candidate)) return candidate;
	}
	return string();
}

// The search list above, one indented entry per line — for error messages.
inline string assetSearchPaths() {
	const vector<string> dirs = assetDirs();
	string out;
	for (size_t i = 0; i < dirs.size(); ++i) {
		out += "    " + dirs[i] + "\n";
	}
	return out;
}

#endif /* UTILS_H */