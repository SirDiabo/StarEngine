#pragma once
#include <functional>

namespace PopupLayer {

void Add(const void* owner, std::function<void()> render);
void Remove(const void* owner);
void RenderAll();

}
