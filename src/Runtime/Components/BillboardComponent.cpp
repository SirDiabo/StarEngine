#include "BillboardComponent.h"

void BillboardComponent::GetEditorProperties(PropertyList& outProperties) {
    SceneComponent::GetEditorProperties(outProperties);
    outProperties.AddString("Texture", &texturePath);
    outProperties.AddFloat("Width", &width);
    outProperties.AddFloat("Height", &height);
    outProperties.AddBool("Hidden In Game", &hiddenInGame);
}

void BillboardComponent::Update(float deltaTime) {
    SceneComponent::Update(deltaTime);
}
