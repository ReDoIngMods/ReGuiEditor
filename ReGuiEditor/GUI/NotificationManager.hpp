#pragma once

#include <UTILS/Singleton.hpp>
#include <string>
#include <vector>
#include <cstdint>

enum NotificationType {
	NOTIFICATION_INFO,
	NOTIFICATION_WARNING,
	NOTIFICATION_ERROR
};

class NotificationManager : public UTILS::Singleton<NotificationManager> {
	struct Notification {
		uint32_t id = 0;

		NotificationType type;

		std::string title;
		std::string message;

		float duration = 3.0f;
		float timer = 0.0f;
		float currentY = 0.0f;
		float height = 0.0f;

		bool dismissRequested = false;
		bool positionInitialized = false;
	};

	std::vector<Notification> notifications_;
	uint32_t nextId_ = 1;
public:
	NotificationManager() = default;
	~NotificationManager() = default;

	void sendNotification(NotificationType type, const std::string& title, const std::string& message, float duration = 3.0f);

	void update(float deltaTime);
	void render();
};