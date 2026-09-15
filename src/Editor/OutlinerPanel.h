#pragma once
#include "Core/Panel.h"
#include "Runtime/StarMap.h"

class Widget;
class SearchListElement;

class OutlinerPanel : public Panel {
public:
    void Initialize() override;
    void Render() override;
    ~OutlinerPanel() override;

    const StarMap* starMap = nullptr;
    int* selectedActorIndex = nullptr;

private:
    Widget* chrome = nullptr;
    SearchListElement* list = nullptr;
};