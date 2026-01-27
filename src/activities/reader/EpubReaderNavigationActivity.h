#pragma once
#include <Epub.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>

#include <functional>
#include <memory>
#include <vector>

#include "../ActivityWithSubactivity.h"
#include "FootnotesData.h"

enum class NavTab { Chapters, Footnotes };

class EpubReaderNavigationActivity final : public ActivityWithSubactivity {
  std::shared_ptr<Epub> epub;
  std::string epubPath;
  const FootnotesData& footnotes;

  TaskHandle_t displayTaskHandle = nullptr;
  SemaphoreHandle_t renderingMutex = nullptr;

  int currentSpineIndex = 0;
  int currentPage = 0;
  int totalPagesInSpine = 0;

  NavTab currentTab = NavTab::Chapters;
  int selectorIndex = 0;
  bool updateRequired = false;

  const std::function<void()> onGoBack;
  const std::function<void(int newSpineIndex)> onSelectSpineIndex;
  const std::function<void(int newSpineIndex, int newPage)> onSyncPosition;
  const std::function<void(const char* href)> onSelectFootnote;

  // Pagination logic
  int getPageItems() const;
  int getTotalItems() const;
  int getTotalPages() const;
  int getCurrentPage() const;

  // Chapters logic
  bool hasSyncOption() const;
  bool isSyncItem(int index) const;
  int tocIndexFromItemIndex(int itemIndex) const;
  std::vector<int> filteredSpineIndices;
  void buildFilteredChapterList();
  void launchSyncActivity();

  // Rendering
  static void taskTrampoline(void* param);
  [[noreturn]] void displayTaskLoop();
  void renderScreen();
  void renderChaptersTab() const;
  void renderFootnotesTab() const;

 public:
  explicit EpubReaderNavigationActivity(GfxRenderer& renderer, MappedInputManager& mappedInput,
                                        const std::shared_ptr<Epub>& epub, const std::string& epubPath,
                                        const FootnotesData& footnotes, const int currentSpineIndex,
                                        const int currentPage, const int totalPagesInSpine,
                                        const std::function<void()>& onGoBack,
                                        const std::function<void(int newSpineIndex)>& onSelectSpineIndex,
                                        const std::function<void(int newSpineIndex, int newPage)>& onSyncPosition,
                                        const std::function<void(const char* href)>& onSelectFootnote)
      : ActivityWithSubactivity("EpubReaderNavigation", renderer, mappedInput),
        epub(epub),
        epubPath(epubPath),
        footnotes(footnotes),
        currentSpineIndex(currentSpineIndex),
        currentPage(currentPage),
        totalPagesInSpine(totalPagesInSpine),
        onGoBack(onGoBack),
        onSelectSpineIndex(onSelectSpineIndex),
        onSyncPosition(onSyncPosition),
        onSelectFootnote(onSelectFootnote) {}

  void onEnter() override;
  void onExit() override;
  void loop() override;
};
