#pragma once
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>

#include <memory>
#include <vector>

#include "../ActivityWithSubactivity.h"
#include "ChaptersTab.h"
#include "FootnotesTab.h"

class EpubReaderTocActivity final : public ActivityWithSubactivity {
 public:
  enum class Tab { CHAPTERS, FOOTNOTES };

 private:
  std::shared_ptr<Epub> epub;
  std::string epubPath;
  const FootnotesData& footnotes;

  TaskHandle_t displayTaskHandle = nullptr;
  SemaphoreHandle_t renderingMutex = nullptr;

  int currentSpineIndex = 0;
  int currentPage = 0;
  int totalPagesInSpine = 0;

  Tab currentTab = Tab::CHAPTERS;
  std::unique_ptr<ChaptersTab> chaptersTab;
  std::unique_ptr<FootnotesTab> footnotesTab;

  bool updateRequired = false;

  const std::function<void()> onGoBack;
  const std::function<void(int newSpineIndex)> onSelectSpineIndex;
  const std::function<void(const char* href)> onSelectFootnote;
  const std::function<void(int newSpineIndex, int newPage)> onSyncPosition;

  static void taskTrampoline(void* param);
  [[noreturn]] void displayTaskLoop();
  void renderScreen();
  TocTab* getCurrentTab() const;

 public:
  EpubReaderTocActivity(GfxRenderer& renderer, MappedInputManager& mappedInput, const std::shared_ptr<Epub>& epub,
                        const std::string& epubPath, int currentSpineIndex, int currentPage, int totalPagesInSpine,
                        const FootnotesData& footnotes, std::function<void()> onGoBack,
                        std::function<void(int)> onSelectSpineIndex, std::function<void(const char*)> onSelectFootnote,
                        std::function<void(int, int)> onSyncPosition)
      : ActivityWithSubactivity("EpubReaderToc", renderer, mappedInput),
        epub(epub),
        epubPath(epubPath),
        currentSpineIndex(currentSpineIndex),
        currentPage(currentPage),
        totalPagesInSpine(totalPagesInSpine),
        footnotes(footnotes),
        onGoBack(onGoBack),
        onSelectSpineIndex(onSelectSpineIndex),
        onSelectFootnote(onSelectFootnote),
        onSyncPosition(onSyncPosition) {
    chaptersTab = std::unique_ptr<ChaptersTab>(new ChaptersTab(
        renderer, mappedInput, epub, currentSpineIndex,
        [this](int spineIndex) { this->onSelectSpineIndex(spineIndex); },
        [this]() { this->launchSyncActivity(); }));
    footnotesTab = std::unique_ptr<FootnotesTab>(new FootnotesTab(
        renderer, mappedInput, footnotes, [this](const char* href) { this->onSelectFootnote(href); }));
  }

  void onEnter() override;
  void onExit() override;
  void loop() override;

  void launchSyncActivity();
};
