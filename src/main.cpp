/*
 * ============================================================================
 *     LumiScripta v1.1.2 - A simple markdown viewer written in modern C++.
 *
 *  Author : Laus Deo (AveCesar1 on GitHub)
 *  License: MIT (https://opensource.org/licenses/MIT)
 *  Main Library: OpenSSL (https://www.openssl.org/)
 *
 *  This project is a personal endeavor to learn and demonstrate the use of
 *  modern graphics in C++ via GLFW and ImGui. It is designed to be a simple
 *  markdown viewer with a focus on clarity, maintainability, and educational
 *  value. The implementation intentionally prioritizes clarity over cleverness,
 *  with extensive comments explaining both the "what" and the "why" behind
 *  each step.
 *
 *  Fair warning: the comments occasionally contain jokes, excessive
 *  enthusiasm, unexpected literature (expect classics only), and mild
 *  verbal acts of violence against the standard library. 
 *  This is a feature.
 *
 *  This software is provided "as is", without warranty of any kind.
 *
 *  With love,
 *  Laus Deo
 * ============================================================================
*/

// Who said C++ can't have a good-looking graphic interface?

#include <iostream>
#include <cstdlib>

#include "lumiscripta/app.h"

int main(int argc, char* argv[]) {
    LumiscriptaApp app;

    if (!app.init()) {
        std::cerr << "Failed to initialize Lumiscripta\n";
        return EXIT_FAILURE;
    }

    // If invoked with a file argument, try to open it.
    if (argc > 1) {
        if (!app.loadFile(argv[1])) {
            std::cerr << "Warning: could not load file: " << argv[1] << "\n";
        }
    }

    app.run();
    app.shutdown();

    return EXIT_SUCCESS;
}