#include "ExplorerPanel.hpp"

#include "../NotificationManager.hpp"

#include <ImGui/imgui.h>
#include <ImGui/imgui_internal.h>

void ExplorerPanel::render() {
	ImGuiWindowClass windowClass;
	windowClass.DockNodeFlagsOverrideSet = ImGuiDockNodeFlags_NoWindowMenuButton;

	ImGui::SetNextWindowClass(&windowClass);

	ImGui::Begin("Explorer");

	ImGui::Text("Explorer");

	ImGui::Separator();
	ImGui::Text("Notification Manager Test");
	ImGui::Separator();

	NotificationManager& notificationManager = NotificationManager::getInstance();
	static float duration = 3.0f;
	static char messageBuffer[4096] = "This is a notification message.";
	static char titleBuffer[256] = "Notification Title";
	if (ImGui::Button("Send Info Notification")) notificationManager.sendNotification(NOTIFICATION_INFO, titleBuffer, messageBuffer, duration);
	if (ImGui::Button("Send Warning Notification")) notificationManager.sendNotification(NOTIFICATION_WARNING, titleBuffer, messageBuffer, duration);
	if (ImGui::Button("Send Error Notification")) notificationManager.sendNotification(NOTIFICATION_ERROR, titleBuffer, messageBuffer, duration);
	ImGui::InputFloat("Notification Duration", &duration, 0.1f, 1.0f, "%.1f");
	ImGui::InputText("Notification Title", titleBuffer, sizeof(titleBuffer));
	ImGui::InputTextMultiline("Notification Message", messageBuffer, sizeof(messageBuffer));

	ImGui::End();
}
