#pragma once
#include <string>

namespace OSTrash {

bool MoveToTrash(const std::string& path, std::string& error);

}
