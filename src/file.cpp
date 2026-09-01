#include "lumiscripta/file.h"

#include <fstream>
#include <sstream>
#include <iostream>

File::File()
    : m_path(), m_content(), m_originalContent(), m_modified(false) {}

bool File::load(const string& path) {
    std::ifstream in(path, std::ios::in | std::ios::binary);
    if (!in) {
        std::cerr << "File::load: failed to open " << path << "\n";
        return false;
    }
    std::ostringstream ss;
    ss << in.rdbuf();
    m_content = ss.str();
    m_originalContent = m_content;
    m_path = path;
    m_modified = false;
    return true;
}

bool File::save(const string& path) {
    std::ofstream out(path, std::ios::out | std::ios::binary);
    if (!out) {
        std::cerr << "File::save: failed to open " << path << " for writing\n";
        return false;
    }
    out << m_content;
    if (!out) {
        std::cerr << "File::save: write failed for " << path << "\n";
        return false;
    }
    m_path = path;
    m_originalContent = m_content;
    m_modified = false;
    return true;
}

const string& File::getContent() const {
    return m_content;
}

void File::setContent(const string& content) {
    m_content = content;
    m_modified = (m_content != m_originalContent);
}

const string& File::getPath() const {
    return m_path;
}

bool File::hasUnsavedChanges() const {
    return m_modified;
}