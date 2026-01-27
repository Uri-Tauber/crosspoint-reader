#include "EpubReaderTocActivity.h"

#include <EpdFontFamily.h>
#include <GfxRenderer.h>

#include "KOReaderSyncActivity.h"
#include "MappedInputManager.h"
#include "ScreenComponents.h"
#include "fontIds.h"

namespace {
constexpr int TAB_BAR_Y = 15;
constexpr int CONTENT_START_Y = 60;
}  // namespace

void EpubReaderTocActivity::taskTrampoline(void* param) {
  auto* self = static_cast<EpubReaderTocActivity*>(param);
  self->displayTaskLoop();
}

void EpubReaderTocActivity::onEnter() {
  ActivityWithSubactivity::onEnter();
  renderingMutex = xSemaphoreCreateMutex();

  chaptersTab->onEnter();
  footnotesTab->onEnter();

  updateRequired = true;
  xTaskCreate(&EpubReaderTocActivity::taskTrampoline, "EpubReaderTocTask", 4096, this, 1, &displayTaskHandle);
}

void EpubReaderTocActivity::onExit() {
  ActivityWithSubactivity::onExit();
  xSemaphoreTake(renderingMutex, portMAX_DELAY);
  if (displayTaskHandle) {
    vTaskDelete(displayTaskHandle);
    displayTaskHandle = nullptr;
  }
  vSemaphoreDelete(renderingMutex);
  renderingMutex = nullptr;
}

void EpubReaderTocActivity::launchSyncActivity() {
  xSemaphoreTake(renderingMutex, portMAX_DELAY);
  exitActivity();
  enterNewActivity(new KOReaderSyncActivity(
      renderer, mappedInput, epub, epubPath, currentSpineIndex, currentPage, totalPagesInSpine,
      [this]() {
        // On cancel
        exitActivity();
        updateRequired = true;
      },
      [this](int newSpineIndex, int newPage) {
        // On sync complete
        exitActivity();
        onSyncPosition(newSpineIndex, newPage);
      }));
  xSemaphoreGive(renderingMutex);
}

void EpubReaderTocActivity::loop() {
  if (subActivity) {
    subActivity->loop();
    return;
  }

  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    onGoBack();
    return;
  }

  const bool leftReleased = mappedInput.wasReleased(MappedInputManager::Button::Left);
  const bool rightReleased = mappedInput.wasReleased(MappedInputManager::Button::Right);

  if (leftReleased && currentTab == Tab::FOOTNOTES) {
    currentTab = Tab::CHAPTERS;
    updateRequired = true;
    return;
  }
  if (rightReleased && currentTab == Tab::CHAPTERS) {
    currentTab = Tab::FOOTNOTES;
    updateRequired = true;
    return;
  }

  getCurrentTab()->loop();
  if (getCurrentTab()->isUpdateRequired()) {
    updateRequired = true;
  }
}

void EpubReaderTocActivity::displayTaskLoop() {
  while (true) {
    if (updateRequired && !subActivity) {
      updateRequired = false;
      xSemaphoreTake(renderingMutex, portMAX_DELAY);
      renderScreen();
      xSemaphoreGive(renderingMutex);
    }
    vTaskDelay(10 / portTICK_PERIOD_MS);
  }
}

void EpubReaderTocActivity::renderScreen() {
  renderer.clearScreen();

  // Draw tab bar
  std::vector<TabInfo> tabs = {{"Chapters", currentTab == Tab::CHAPTERS}, {"Footnotes", currentTab == Tab::FOOTNOTES}};
  ScreenComponents::drawTabBar(renderer, TAB_BAR_Y, tabs);

  const int screenHeight = renderer.getScreenHeight();
  const int contentHeight = screenHeight - CONTENT_START_Y - 60;

  getCurrentTab()->render(CONTENT_START_Y, contentHeight);

  // Draw scroll indicator
  ScreenComponents::drawScrollIndicator(renderer, getCurrentTab()->getCurrentPage(), getCurrentTab()->getTotalPages(),
                                        CONTENT_START_Y, contentHeight);

  // Draw button hints
  const auto labels = mappedInput.mapLabels("« Back", "Select", "< Tab", "Tab >");
  renderer.drawButtonHints(UI_10_FONT_ID, labels.btn1, labels.btn2, labels.btn3, labels.btn4);

  renderer.drawSideButtonHints(UI_10_FONT_ID, ">", "<");

  renderer.displayBuffer();
}

TocTab* EpubReaderTocActivity::getCurrentTab() const {
  return (currentTab == Tab::CHAPTERS) ? static_cast<TocTab*>(chaptersTab.get())
                                       : static_cast<TocTab*>(footnotesTab.get());
}
