#pragma once
#include <functional>

namespace MainThreadQueue {

void MarkMainThread();
bool IsMainThread();
bool Run(const std::function<void()>& task);
void Pump(double maxSeconds = 0.008);
void Shutdown();

}
