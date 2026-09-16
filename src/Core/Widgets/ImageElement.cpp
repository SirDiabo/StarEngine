#include "ImageElement.h"
#include "Runtime/AssetCache.h"

void ImageElement::OnDrawSelf(Rectangle rect, float uiScale) {
    const Texture2D* texture = directTexture;
    if (!texture) {
        if (texturePath.empty()) return;
        texture = AssetCache::Get().GetOrLoadTexture(texturePath);
    }
    if (!texture || texture->id == 0) return;
    Rectangle source{ 0.0f, 0.0f, (float)texture->width, (float)texture->height };
    DrawTexturePro(*texture, source, rect, {0.0f, 0.0f}, 0.0f, tint);
}
