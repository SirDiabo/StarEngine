#pragma once
#include <string>

namespace AuthChallenge {

std::string GenerateNonceHex();
std::string ComputeHmacHex(const std::string& password, const std::string& nonceHex);

}