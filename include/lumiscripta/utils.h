#ifndef UTILS_H
#define UTILS_H

#include <string>
#include <vector>

using std::string;
using std::vector;

// Trim whitespace from both ends of a string.
string trim(const string& str);

// Split a string by a delimiter.
vector<string> split(const string& str, char delimiter);

// Extract lowercase file extension from a path (without the dot).
// Returns empty string if no extension found.
string fileExtension(const string& path);

// Format a byte count into a human-readable string (e.g. "1.2 KB").
string formatBytes(size_t bytes);

#endif /* UTILS_H */