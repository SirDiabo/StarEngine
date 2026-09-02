#include "WidgetDebug.h"
#include "Widget.h"
#include "VerticalBoxElement.h"
#include "ButtonElement.h"
#include "TextElement.h"
#include "ProgressBarElement.h"
#include "ImageElement.h"
#include "SpacerElement.h"
#include "Runtime/World.h"
#include "Runtime/Actor.h"
#include "Runtime/Components/WidgetComponent.h"

namespace WidgetDebug {

void SpawnTestWidget() {
    Actor* actor = World::Get().SpawnActor("", "WidgetDebug");
    WidgetComponent* widgetComponent = new WidgetComponent();
    actor->AddComponent(widgetComponent);

    VerticalBoxElement* root = new VerticalBoxElement();
    root->anchor = {0.0f, 0.0f};
    root->pivot = {0.0f, 0.0f};
    root->offset = {40.0f, 40.0f};
    root->size = {260.0f, 340.0f};
    root->childSpacing = 10.0f;
    root->backgroundColor = {30, 30, 30, 200};

    TextElement* label = new TextElement();
    label->size = {260.0f, 30.0f};
    label->text = "This is a Test Message";
    label->fontSize = 18.0f;
    label->color = WHITE;
    root->AddChild(label);

    ButtonElement* buttonOne = new ButtonElement();
    buttonOne->size = {260.0f, 40.0f};
    root->AddChild(buttonOne);

    ButtonElement* buttonTwo = new ButtonElement();
    buttonTwo->size = {260.0f, 40.0f};
    buttonTwo->disabled = true;
    root->AddChild(buttonTwo);

    ButtonElement* buttonThree = new ButtonElement();
    buttonThree->size = {260.0f, 40.0f};
    root->AddChild(buttonThree);

    SpacerElement* spacer = new SpacerElement();
    spacer->size = {260.0f, 16.0f};
    root->AddChild(spacer);

    ProgressBarElement* progressBar = new ProgressBarElement();
    progressBar->size = {260.0f, 24.0f};
    progressBar->value = 0.6f;
    root->AddChild(progressBar);

    ImageElement* image = new ImageElement();
    image->size = {260.0f, 100.0f};
    image->texturePath = "C:/Users/sirdiabo/Documents/StarProjects/My Linux Game/Content/Meshes/Map/SimpleSkyHDRI.startex";
    root->AddChild(image);

    Widget* widget = new Widget();
    widget->rootElement = root;
    widgetComponent->SetWidget(widget);
}

}
