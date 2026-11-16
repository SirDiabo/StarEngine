#include "ProjectTrust.h"
#include "StarEngineHome.h"

namespace ProjectTrust {

TrustStore& Default() {
    static TrustStore store(StarEngineHome::Directory());
    return store;
}

bool IsTrusted(const std::string& projectRoot) {
    return Default().IsTrusted(projectRoot);
}

bool Trust(const std::string& projectRoot) {
    return Default().Trust(projectRoot);
}

std::string NotTrustedMessage() {
    return "This project is not trusted yet. Building and packaging compile the project's modules into StarEngine, "
           "so they only run for projects you have marked as trusted. Start the action from the editor to review and trust the project.";
}

}
