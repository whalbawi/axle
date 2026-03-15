#include "axle/axle.h"

#include <string>

#include "version.h"

namespace axle {

std::string get_version() {
    return "version: " + version() + " - commit: " + git_commit_hash();
}

} // namespace axle
