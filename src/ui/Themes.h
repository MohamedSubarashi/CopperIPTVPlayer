#pragma once

namespace Themes {

enum class Kind { Dark, Light };

// Applies the named .qss theme application-wide (windows, dialogs, menus...).
void apply(Kind kind, bool reload = true);

inline Kind defaultKind() { return Kind::Dark; }

} // namespace Themes