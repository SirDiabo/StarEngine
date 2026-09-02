#include "ImageElement.h"
#include "Runtime/AssetCache.h"

void ImageElement::OnDrawSelf(Rectangle rect, float uiScale) {
    if (texturePath.empty()) return;
    Texture2D* texture = AssetCache::Get().GetOrLoadTexture(texturePath);
    if (!texture) return;
    Rectangle source{ 0.0f, 0.0f, (float)texture->width, (float)texture->height };
    DrawTexturePro(*texture, source, rect, {0.0f, 0.0f}, 0.0f, tint);
}
