#pragma once
#include <cstring>
#include <functional>

#include "../../lib/Epub/Epub/FootnoteEntry.h"
#include "TocTab.h"

class FootnotesData {
 private:
  FootnoteEntry entries[16];
  int count;

 public:
  FootnotesData() : count(0) {
    for (int i = 0; i < 16; i++) {
      entries[i].number[0] = '\0';
      entries[i].href[0] = '\0';
    }
  }

  void addFootnote(const char* number, const char* href) {
    if (count < 16 && number && href) {
      strncpy(entries[count].number, number, 2);
      entries[count].number[2] = '\0';
      strncpy(entries[count].href, href, 63);
      entries[count].href[63] = '\0';
      count++;
    }
  }

  void clear() {
    count = 0;
    for (int i = 0; i < 16; i++) {
      entries[i].number[0] = '\0';
      entries[i].href[0] = '\0';
    }
  }

  int getCount() const { return count; }

  const FootnoteEntry* getEntry(int index) const {
    if (index >= 0 && index < count) {
      return &entries[index];
    }
    return nullptr;
  }
};

class FootnotesTab final : public TocTab {
  const FootnotesData& footnotes;
  int selectedIndex = 0;
  bool updateRequired = false;

  const std::function<void(const char* href)> onSelectFootnote;

 public:
  FootnotesTab(GfxRenderer& renderer, MappedInputManager& mappedInput, const FootnotesData& footnotes,
               std::function<void(const char*)> onSelectFootnote)
      : TocTab(renderer, mappedInput), footnotes(footnotes), onSelectFootnote(onSelectFootnote) {}

  void onEnter() override;
  void loop() override;
  void render(int contentTop, int contentHeight) override;

  int getCurrentPage() const override;
  int getTotalPages() const override;
  bool isUpdateRequired() const override { return updateRequired; }
  void clearUpdateRequired() override { updateRequired = false; }
};
