#pragma once

#include <expat.h>

#include <climits>
#include <functional>
#include <memory>

#include "../ParsedText.h"
#include "../blocks/TextBlock.h"

class Page;
class GfxRenderer;

#define MAX_WORD_SIZE 200

struct Noteref {
  char number[16];
  char href[128];
};

struct InlineFootnote {
  std::string id;
  std::string text;
};

struct ParagraphNote {
  std::string id;
  std::string text;
};

class ChapterHtmlSlimParser {
  const std::string& filepath;
  GfxRenderer& renderer;
  std::function<void(std::unique_ptr<Page>)> completePageFn;
  std::function<void(int)> progressFn;  // Progress callback (0-100)
  int depth = 0;
  int skipUntilDepth = INT_MAX;
  int boldUntilDepth = INT_MAX;
  int italicUntilDepth = INT_MAX;
  // buffer for building up words from characters, will auto break if longer than this
  // leave one char at end for null pointer
  char partWordBuffer[MAX_WORD_SIZE + 1] = {};
  int partWordBufferIndex = 0;
  std::unique_ptr<ParsedText> currentTextBlock = nullptr;
  std::unique_ptr<Page> currentPage = nullptr;
  int16_t currentPageNextY = 0;
  int fontId;
  float lineCompression;
  bool extraParagraphSpacing;
  uint8_t paragraphAlignment;
  uint16_t viewportWidth;
  uint16_t viewportHeight;
  bool hyphenationEnabled;

  // Noteref tracking
  bool insideNoteref = false;
  char currentNoterefText[16] = {0};
  int currentNoterefTextLen = 0;
  char currentNoterefHref[128] = {0};
  int currentNoterefHrefLen = 0;
  std::function<void(Noteref&)> noterefCallback = nullptr;

  // Inline footnotes (aside) tracking
  bool insideAsideFootnote = false;
  int asideDepth = 0;
  char currentAsideId[16] = {0};
  static constexpr int MAX_ASIDE_BUFFER = 2048;
  char currentAsideText[MAX_ASIDE_BUFFER] = {0};
  int currentAsideTextLen = 0;

  // Paragraph note tracking
  bool insideParagraphNote = false;
  int paragraphNoteDepth = 0;
  char currentParagraphNoteId[16] = {0};
  static constexpr int MAX_PNOTE_BUFFER = 512;
  char currentParagraphNoteText[MAX_PNOTE_BUFFER] = {0};
  int currentParagraphNoteTextLen = 0;

  bool isPass1CollectingAsides = false;

  // Footnote tracking for current page
  FootnoteEntry currentFootnotes[16];
  int currentFootnoteCount = 0;

  void addFootnoteToCurrentPage(const char* number, const char* href);
  void startNewTextBlock(TextBlock::Style style);
  void makePages();
  // XML callbacks
  static void XMLCALL startElement(void* userData, const XML_Char* name, const XML_Char** atts);
  static void XMLCALL characterData(void* userData, const XML_Char* s, int len);
  static void XMLCALL endElement(void* userData, const XML_Char* name);

 public:
  // inline footnotes
  std::vector<InlineFootnote> inlineFootnotes;
  int inlineFootnoteCount = 0;
  // paragraph notes
  std::vector<ParagraphNote> paragraphNotes;
  int paragraphNoteCount = 0;

  explicit ChapterHtmlSlimParser(const std::string& filepath, GfxRenderer& renderer, const int fontId,
                                 const float lineCompression, const bool extraParagraphSpacing,
                                 const uint8_t paragraphAlignment, const uint16_t viewportWidth,
                                 const uint16_t viewportHeight, const bool hyphenationEnabled,
                                 const std::function<void(std::unique_ptr<Page>)>& completePageFn,
                                 const std::function<void(int)>& progressFn = nullptr)
      : filepath(filepath),
        renderer(renderer),
        fontId(fontId),
        lineCompression(lineCompression),
        extraParagraphSpacing(extraParagraphSpacing),
        paragraphAlignment(paragraphAlignment),
        viewportWidth(viewportWidth),
        viewportHeight(viewportHeight),
        hyphenationEnabled(hyphenationEnabled),
        completePageFn(completePageFn),
        progressFn(progressFn) {}

  ~ChapterHtmlSlimParser() = default;

  bool parseAndBuildPages();
  void addLineToPage(std::shared_ptr<TextBlock> line);
  void setNoterefCallback(const std::function<void(Noteref&)>& callback) { noterefCallback = callback; }
};
