#pragma once

#include <cstddef>

// One embedded interface file. The table is generated at configure time by
// cmake/EmbedUi.cmake from the contents of ui/.
struct UiResource {
    const char* path;     // virtual path, e.g. "css/app.css" (forward slashes)
    int id;               // RCDATA resource id
    const wchar_t* mime;  // Content-Type served for this file
};

extern const UiResource kUiResources[];
extern const size_t kUiResourceCount;
