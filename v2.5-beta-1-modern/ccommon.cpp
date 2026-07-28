#include "stdafx.h"
#include "winnls.h"

#undef ASSERT

// Forward slashes, not backslashes: MSVC accepts either, clang treats a backslash as a
// literal filename character. Same fix as urlutil.cpp.
#include "../core/ccommon.cpp"
