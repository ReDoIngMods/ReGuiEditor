#include "../JsonGuiInstance.hpp"

#include "JsonGuiOverlay.hpp"

#include <MyGui/MyGUI_FactoryManager.h>
#include <MyGui/MyGUI_ScrollView.h>
#include <MyGui/MyGUI_Widget.h>

void JsonGuiInstance::registerWidgetFactories() {
	MyGUI::FactoryManager& factories = MyGUI::FactoryManager::getInstance();
	factories.registerFactory<MyGUI::ScrollView>("Widget", "ScrollViewPanel");

	for (const char* type : { "SubLayout", "EffectBox", "PreviewBox", "JsonContainerBox", "EditorMenu", "HorizontalSliderBox" })
		factories.registerFactory<MyGUI::Widget>("Widget", type);
}

void JsonGuiInstance::registerOverlayShader() {
	JsonGui::registerOverlayShader();
}
