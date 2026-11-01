#pragma once
#include "raylib.h"
#include <algorithm>
#include <vector>

namespace ScissorStack {

inline std::vector<Rectangle>& Stack() {
    static std::vector<Rectangle> stack;
    return stack;
}

inline Rectangle Intersect(const Rectangle& a, const Rectangle& b) {
    float x0 = std::max(a.x, b.x);
    float y0 = std::max(a.y, b.y);
    float x1 = std::min(a.x + a.width, b.x + b.width);
    float y1 = std::min(a.y + a.height, b.y + b.height);
    return { x0, y0, std::max(0.0f, x1 - x0), std::max(0.0f, y1 - y0) };
}

inline void Push(Rectangle rect) {
    std::vector<Rectangle>& stack = Stack();
    if (!stack.empty()) rect = Intersect(stack.back(), rect);
    stack.push_back(rect);
    BeginScissorMode((int)rect.x, (int)rect.y, (int)rect.width, (int)rect.height);
}

inline void Pop() {
    std::vector<Rectangle>& stack = Stack();
    EndScissorMode();
    if (stack.empty()) return;
    stack.pop_back();
    if (!stack.empty()) {
        const Rectangle& top = stack.back();
        BeginScissorMode((int)top.x, (int)top.y, (int)top.width, (int)top.height);
    }
}

}
