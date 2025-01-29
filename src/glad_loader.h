#pragma once

namespace aim {

// Calls gladLoadGLLoader. This hides glad.h in a separate compilation unit to prevent conflicts
// over headers.
int LoadGlad();

}  // namespace aim