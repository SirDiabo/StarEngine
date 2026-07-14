#pragma once
#include <string>

namespace SourceVendoring {

std::string Vendor(const std::string& contentRoot, const std::string& absoluteSourcePath,
                    const std::string& assetName);

}
