#pragma once
#include <Epub.h>

#include <functional>
#include <memory>
#include <vector>

#include "TocTab.h"

class ChaptersTab final : public TocTab {
  std::shared_ptr<Epub> epub;
  int currentSpineIndex;
  int selectorIndex = 0;
  bool updateRequired = false;
  std::vector<int> filteredSpineIndices;

  const std::function<void(int newSpineIndex)> onSelectSpineIndex;
  const std::function<void()> onLaunchSync;

  int getPageItems(int contentTop, int contentHeight) const;
  int getTotalItems() const;
  bool hasSyncOption() const;
  bool isSyncItem(int index) const;
  int tocIndexFromItemIndex(int itemIndex) const;
  void buildFilteredChapterList();

 public:
  ChaptersTab(GfxRenderer& renderer, MappedInputManager& mappedInput, const std::shared_ptr<Epub>& epub,
              int currentSpineIndex, std::function<void(int)> onSelectSpineIndex, std::function<void()> onLaunchSync)
      : TocTab(renderer, mappedInput),
        epub(epub),
        currentSpineIndex(currentSpineIndex),
        onSelectSpineIndex(onSelectSpineIndex),
        onLaunchSync(onLaunchSync) {}

  void onEnter() override;
  void loop() override;
  void render(int contentTop, int contentHeight) override;

  int getCurrentPage() const override;
  int getTotalPages() const override;
  bool isUpdateRequired() const override { return updateRequired; }
  void clearUpdateRequired() override { updateRequired = false; }
};
