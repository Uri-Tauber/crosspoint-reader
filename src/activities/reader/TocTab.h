#pragma once

class GfxRenderer;
class MappedInputManager;

class TocTab {
 protected:
  GfxRenderer& renderer;
  MappedInputManager& mappedInput;

 public:
  TocTab(GfxRenderer& renderer, MappedInputManager& mappedInput) : renderer(renderer), mappedInput(mappedInput) {}
  virtual ~TocTab() = default;

  virtual void onEnter() = 0;
  virtual void onExit() {}
  virtual void loop() = 0;
  virtual void render(int contentTop, int contentHeight) = 0;

  virtual int getCurrentPage() const = 0;
  virtual int getTotalPages() const = 0;
  virtual bool isUpdateRequired() const = 0;
  virtual void clearUpdateRequired() = 0;
};
