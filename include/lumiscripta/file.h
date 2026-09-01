#ifndef FILE_H
#define FILE_H

#include <string>

using std::string;

class File {
public:
    File();
    ~File() = default; // C++ has a 'default' constructor/destructor. Saves you a lot of code.

    // Load file from disk. Returns true on success.
    bool load(const string& path);

    // Save file to disk. Returns true on success.
    bool save(const string& path);

    // Get current content.
    const string& getContent() const;

    // Replace content (e.g. from clipboard paste).
    void setContent(const string& content);

    // Get the loaded file path.
    const string& getPath() const;

    // True if content differs from last saved state.
    bool hasUnsavedChanges() const;

private:
    string m_path;
    string m_content;
    string m_originalContent;
    bool m_modified;
};

#endif /* FILE_H */