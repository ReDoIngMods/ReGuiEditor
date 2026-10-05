#include "NotificationManager.hpp"

#include <ImGui/imgui.h>

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <string>

const float SLIDE_IN_TIME = 0.75f;
const float SLIDE_OUT_TIME = 0.75f;

const float PADDING_SCREEN = 16.f;
const float PADDING_X = 10.f;
const float PADDING_Y = 10.f;

const float ACCENT_WIDTH = 4.f;
const float SPACING_ITEMS = 10.f;

const float PROGRESS_BAR_HEIGHT = 10.f;
const float PROGRESS_BAR_GAP = 2.f;

const float MIN_WIDTH = 200.f;
const float MAX_WIDTH = 400.f;

const float SLIDE_IN_DISTANCE = MAX_WIDTH * 2.f;
const float SLIDE_OUT_DISTANCE = MAX_WIDTH * 2.f;

inline static float easeInCubic(float t) {
    t = std::clamp(t, 0.0f, 1.0f);
    return t * t * t;
}

inline static float easeOutBack(float t) {
    t = std::clamp(t, 0.0f, 1.0f);

    const float c1 = 1.70158f;
    const float c3 = c1 + 1.0f;
    const float u = t - 1.0f;
    return 1.0f + c3 * u * u * u + c1 * u * u;
}

inline static float easeInBack(float t) {
    t = std::clamp(t, 0.0f, 1.0f);

    const float c1 = 1.70158f;
    const float c3 = c1 + 1.0f;
    return c3 * t * t * t - c1 * t * t;
}

inline static float lerp(float a, float b, float t) {
    return a + (b - a) * t;
}

inline static ImVec4 getNotificationColor(NotificationType type) {
    switch (type) {
    case NOTIFICATION_INFO:    return { 0.20f, 0.55f, 1.00f, 1.00f };
    case NOTIFICATION_WARNING: return { 1.00f, 0.65f, 0.15f, 1.00f };
    case NOTIFICATION_ERROR:   return { 1.00f, 0.25f, 0.25f, 1.00f };
    default:                   return { 1.00f, 1.00f, 1.00f, 1.00f };
    }
}

void NotificationManager::sendNotification(NotificationType type, const std::string& title, const std::string& message, float duration) {
    this->notifications_.push_back({ this->nextId_++, type, title, message, duration, 0.0f });
}

void NotificationManager::update(float deltaTime) {
    for (auto iterator = this->notifications_.begin(); iterator != this->notifications_.end();) {
        const float totalLifetime = SLIDE_IN_TIME + iterator->duration + SLIDE_OUT_TIME;

        iterator->timer += deltaTime;
        if (iterator->dismissRequested && (totalLifetime - iterator->timer) > SLIDE_OUT_TIME)
            iterator->timer = totalLifetime - SLIDE_OUT_TIME;

        if (iterator->timer >= totalLifetime)
            iterator = this->notifications_.erase(iterator);
        else
            ++iterator;
    }
}

void NotificationManager::render() {
    if (this->notifications_.empty())
        return;

    const ImVec2 windowPos = ImGui::GetWindowPos();
    const ImVec2 windowSize = ImGui::GetWindowSize();
    ImDrawList* drawList = ImGui::GetForegroundDrawList();

    const ImVec2 windowMax = {
        windowPos.x + windowSize.x,
        windowPos.y + windowSize.y
    };
    drawList->PushClipRect(windowPos, windowMax, true);

    ImFont* font = ImGui::GetFont();
    const float fontSize = ImGui::GetFontSize();
    const ImGuiIO& io = ImGui::GetIO();

    const ImVec2 panelTopRight = {
        windowPos.x + windowSize.x - PADDING_SCREEN,
        windowPos.y + PADDING_SCREEN
    };

    const ImVec2 mousePosition = io.MousePos;

    const float currentMaxWidth = std::min(MAX_WIDTH, windowSize.x - PADDING_SCREEN * 2.0f);
    if (currentMaxWidth < MIN_WIDTH) {
        drawList->PopClipRect();
        return;
    }

    float targetYOffset = 0;
    for (auto& notification : this->notifications_) {
        const float totalLifetime = SLIDE_IN_TIME + notification.duration + SLIDE_OUT_TIME;

        const float maxTextWidth = currentMaxWidth - (PADDING_X * 2.0f + ACCENT_WIDTH);

        const ImVec2 titleSize = font->CalcTextSizeA(fontSize, FLT_MAX, maxTextWidth, notification.title.c_str());
        const ImVec2 messageSize = notification.message.empty() ? ImVec2{ 0.0f, 0.0f } : font->CalcTextSizeA(fontSize, FLT_MAX, maxTextWidth, notification.message.c_str());

        const float contentWidth = std::max(titleSize.x, messageSize.x);
        const float toastWidth = std::clamp(contentWidth + PADDING_X * 2.0f + ACCENT_WIDTH, MIN_WIDTH, currentMaxWidth);

        float contentHeight = titleSize.y;
        if (!notification.message.empty())
            contentHeight += 6.0f + messageSize.y;

        contentHeight += PROGRESS_BAR_GAP + PROGRESS_BAR_HEIGHT;
        notification.height = contentHeight + PADDING_Y * 2.0f;

        const float openTime = std::clamp(notification.timer / SLIDE_IN_TIME, 0.0f, 1.0f);
        const float closeTime = std::clamp(1.0f - (totalLifetime - notification.timer) / SLIDE_OUT_TIME, 0.0f, 1.0f);

        float slideX = 0.0f;
        if (openTime < 1.0f)
            slideX = (1.0f - easeOutBack(openTime)) * SLIDE_IN_DISTANCE;
        else if (closeTime > 0.0f)
            slideX = easeInBack(closeTime) * SLIDE_OUT_DISTANCE;

        if (!notification.positionInitialized) {
            notification.currentY = targetYOffset;
            notification.positionInitialized = true;
        } else {
            const float blend = 1.0f - std::exp(-18.0f * io.DeltaTime);
            notification.currentY = lerp(notification.currentY, targetYOffset, blend);
        }

        const ImVec2 toastMin = {
            panelTopRight.x - toastWidth + slideX,
            panelTopRight.y + notification.currentY
        };

        const ImVec2 toastMax = {
            toastMin.x + toastWidth,
            toastMin.y + notification.height
        };

        const bool isHovered = (mousePosition.x >= toastMin.x && mousePosition.x <= toastMax.x && mousePosition.y >= toastMin.y && mousePosition.y <= toastMax.y);
        if (isHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
            notification.dismissRequested = true;

        const ImU32 colorBackground = ImGui::GetColorU32(ImGuiCol_PopupBg);
        const ImU32 colorText = ImGui::GetColorU32(ImGuiCol_Text);

        const ImVec4 typeColorVec = getNotificationColor(notification.type);
        const ImU32 colorAccent = ImGui::ColorConvertFloat4ToU32(typeColorVec);

        drawList->AddRectFilled(toastMin, toastMax, colorBackground, 0);
        drawList->AddRect(toastMin, toastMax, colorAccent, 0, 1.5);

        const float progress = std::clamp((notification.timer - SLIDE_IN_TIME) / notification.duration, 0.0f, 1.0f);
        const float progressX = toastMin.x + ACCENT_WIDTH + (toastWidth - ACCENT_WIDTH) * (1.0f - progress);

        const ImVec2 progressStart = { progressX, toastMax.y - PROGRESS_BAR_HEIGHT };
        const ImVec2 progressEnd = { toastMax.x, toastMax.y };

        drawList->AddRectFilled(progressStart, progressEnd, colorAccent);

        const ImVec2 titlePos = { toastMin.x + ACCENT_WIDTH + PADDING_X, toastMin.y + PADDING_Y };
        drawList->AddText(font, fontSize, titlePos, colorAccent, notification.title.c_str(), nullptr, maxTextWidth);

        if (!notification.message.empty()) {
            const ImVec2 messagePos = { titlePos.x, titlePos.y + titleSize.y + 6.0f };
            drawList->AddText(font, fontSize, messagePos, colorText, notification.message.c_str(), nullptr, maxTextWidth);
        }

        targetYOffset += (notification.height + SPACING_ITEMS);
    }

    drawList->PopClipRect();
}