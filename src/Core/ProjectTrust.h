#pragma once
#include "TrustStore.h"
#include <string>

namespace ProjectTrust {

TrustStore& Default();
bool IsTrusted(const std::string& projectRoot);
bool Trust(const std::string& projectRoot);
std::string NotTrustedMessage();

}
