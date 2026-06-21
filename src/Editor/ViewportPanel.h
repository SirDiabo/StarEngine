#pragma once
#include "Core/Panel.h"

class ViewportPanel : public Panel {
public:
    void Initialize() override;
    void Render() override;
};