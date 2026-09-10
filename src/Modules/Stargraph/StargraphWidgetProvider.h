#pragma once
#include "Core/ProgrammingInterfaceRegistry.h"
#include "Stargraph.h"

class StargraphBehaviorContent : public IBehaviorContent {
public:
    Stargraph graph;
};

void RegisterStargraphWidgetProvider();
