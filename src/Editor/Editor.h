#pragma once
#include "Core/Panel.h"
#include <vector>

class Editor {
public:
    void Initialize();
    void Render();

private:
    std::vector<Panel*> panels;
};