#include "ChapterHtmlSlimParser.h"

#include <algorithm>

#include <GfxRenderer.h>
#include <HardwareSerial.h>
#include <SDCardManager.h>
#include <expat.h>

#include "../Page.h"

const char* HEADER_TAGS[] = {"h1", "h2", "h3", "h4", "h5", "h6"};
constexpr int NUM_HEADER_TAGS = sizeof(HEADER_TAGS) / sizeof(HEADER_TAGS[0]);

// Minimum file size (in bytes) to show progress bar - smaller chapters don't benefit from it
constexpr size_t MIN_SIZE_FOR_PROGRESS = 50 * 1024;  // 50KB

const char* BLOCK_TAGS[] = {"p", "li", "div", "br", "blockquote"};
constexpr int NUM_BLOCK_TAGS = sizeof(BLOCK_TAGS) / sizeof(BLOCK_TAGS[0]);

const char* BOLD_TAGS[] = {"b", "strong"};
constexpr int NUM_BOLD_TAGS = sizeof(BOLD_TAGS) / sizeof(BOLD_TAGS[0]);

const char* ITALIC_TAGS[] = {"i", "em"};
constexpr int NUM_ITALIC_TAGS = sizeof(ITALIC_TAGS) / sizeof(ITALIC_TAGS[0]);

const char* IMAGE_TAGS[] = {"img"};
constexpr int NUM_IMAGE_TAGS = sizeof(IMAGE_TAGS) / sizeof(IMAGE_TAGS[0]);

const char* SKIP_TAGS[] = {"head"};
constexpr int NUM_SKIP_TAGS = sizeof(SKIP_TAGS) / sizeof(SKIP_TAGS[0]);

bool isWhitespace(const char c) { return c == ' ' || c == '\r' || c == '\n' || c == '\t'; }

// given the start and end of a tag, check to see if it matches a known tag
bool matches(const char* tag_name, const char* possible_tags[], const int possible_tag_count) {
  for (int i = 0; i < possible_tag_count; i++) {
    if (strcmp(tag_name, possible_tags[i]) == 0) {
      return true;
    }
  }
  return false;
}

void ChapterHtmlSlimParser::addFootnoteToCurrentPage(const char* number, const char* href) {
  if (currentFootnoteCount < 16) {
    FootnoteEntry& entry = currentFootnotes[currentFootnoteCount++];
    strncpy(entry.number, number, sizeof(entry.number) - 1);
    entry.number[sizeof(entry.number) - 1] = '\0';

    // Rewrite href if it points to an inline footnote or paragraph note
    std::string hrefStr(href);
    size_t hashPos = hrefStr.find('#');
    if (hashPos != std::string::npos) {
      std::string anchor = hrefStr.substr(hashPos + 1);

      // Check if it's an inline footnote
      bool found = false;
      for (const auto& fn : inlineFootnotes) {
        if (fn.id == anchor) {
          snprintf(entry.href, sizeof(entry.href), "inline_%s.html#%s", anchor.c_str(), anchor.c_str());
          entry.isInline = true;
          found = true;
          break;
        }
      }

      // Check if it's a paragraph note
      if (!found) {
        for (const auto& fn : paragraphNotes) {
          if (fn.id == anchor) {
            snprintf(entry.href, sizeof(entry.href), "pnote_%s.html#%s", anchor.c_str(), anchor.c_str());
            entry.isInline = true;
            found = true;
            break;
          }
        }
      }

      if (!found) {
        strncpy(entry.href, href, sizeof(entry.href) - 1);
        entry.href[sizeof(entry.href) - 1] = '\0';
        entry.isInline = false;
      }
    } else {
      strncpy(entry.href, href, sizeof(entry.href) - 1);
      entry.href[sizeof(entry.href) - 1] = '\0';
      entry.isInline = false;
    }
  }
}

// start a new text block if needed
void ChapterHtmlSlimParser::startNewTextBlock(const TextBlock::Style style) {
  if (currentTextBlock) {
    // already have a text block running and it is empty - just reuse it
    if (currentTextBlock->isEmpty()) {
      currentTextBlock->setStyle(style);
      return;
    }

    makePages();
  }
  currentTextBlock.reset(new ParsedText(style, extraParagraphSpacing, hyphenationEnabled));
}

void XMLCALL ChapterHtmlSlimParser::startElement(void* userData, const XML_Char* name, const XML_Char** atts) {
  auto* self = static_cast<ChapterHtmlSlimParser*>(userData);

  // Middle of skip
  if (self->skipUntilDepth < self->depth) {
    self->depth += 1;
    return;
  }

  // Detect noterefs
  if (strcmp(name, "a") == 0 && atts != nullptr) {
    bool isNoteref = false;
    const char* href = nullptr;

    for (int i = 0; atts[i]; i += 2) {
      if ((strcmp(atts[i], "class") == 0 && strcmp(atts[i + 1], "noteref") == 0) ||
          (strcmp(atts[i], "epub:type") == 0 && strcmp(atts[i + 1], "noteref") == 0)) {
        isNoteref = true;
      } else if (strcmp(atts[i], "href") == 0) {
        href = atts[i + 1];
      }
    }

    if (isNoteref && href) {
      self->insideNoteref = true;
      self->currentNoterefTextLen = 0;
      self->currentNoterefText[0] = '\0';
      strncpy(self->currentNoterefHref, href, 127);
      self->currentNoterefHref[127] = '\0';
      self->depth += 1;
      return;
    }
  }

  // Detect <aside epub:type="footnote"> for inline footnotes
  if (strcmp(name, "aside") == 0 && atts != nullptr) {
    bool isFootnote = false;
    const char* id = nullptr;

    for (int i = 0; atts[i]; i += 2) {
      if (strcmp(atts[i], "epub:type") == 0 && strcmp(atts[i + 1], "footnote") == 0) {
        isFootnote = true;
      } else if (strcmp(atts[i], "id") == 0) {
        id = atts[i + 1];
      }
    }

    if (isFootnote && id && self->inlineFootnoteCount < 16) {
      self->insideAsideFootnote = true;
      self->asideDepth = self->depth;
      strncpy(self->currentAsideId, id, 15);
      self->currentAsideId[15] = '\0';
      self->currentAsideTextLen = 0;
      self->currentAsideText[0] = '\0';
      self->depth += 1;
      return;
    }
  }

  // Detect <p class="note"> for some other types of footnotes
  if (strcmp(name, "p") == 0 && atts != nullptr) {
    bool isNote = false;
    for (int i = 0; atts[i]; i += 2) {
      if (strcmp(atts[i], "class") == 0 && strcmp(atts[i + 1], "note") == 0) {
        isNote = true;
        break;
      }
    }

    if (isNote && self->paragraphNoteCount < 32) {
      self->insideParagraphNote = true;
      self->paragraphNoteDepth = self->depth;
      self->currentParagraphNoteId[0] = '\0';
      self->currentParagraphNoteTextLen = 0;
      self->currentParagraphNoteText[0] = '\0';

      // Look for ID in nested <a> tags later or right here if available
      for (int i = 0; atts[i]; i += 2) {
        if (strcmp(atts[i], "id") == 0) {
          strncpy(self->currentParagraphNoteId, atts[i + 1], 15);
          self->currentParagraphNoteId[15] = '\0';
        }
      }
    }
  }

  // Special handling for <a> with id inside a paragraph note if id not yet found
  if (strcmp(name, "a") == 0 && self->insideParagraphNote && self->currentParagraphNoteId[0] == '\0' &&
      atts != nullptr) {
    for (int i = 0; atts[i]; i += 2) {
      if (strcmp(atts[i], "id") == 0 || strcmp(atts[i], "name") == 0) {
        strncpy(self->currentParagraphNoteId, atts[i + 1], 15);
        self->currentParagraphNoteId[15] = '\0';
        break;
      }
    }
  }

  // Special handling for tables - show placeholder text instead of dropping silently
  if (strcmp(name, "table") == 0) {
    // Add placeholder text
    self->startNewTextBlock(TextBlock::CENTER_ALIGN);
    if (self->currentTextBlock) {
      self->currentTextBlock->addWord("[Table omitted]", EpdFontFamily::ITALIC);
    }

    // Skip table contents
    self->skipUntilDepth = self->depth;
    self->depth += 1;
    return;
  }

  if (matches(name, IMAGE_TAGS, NUM_IMAGE_TAGS)) {
    // TODO: Start processing image tags
    std::string alt;
    if (atts != nullptr) {
      for (int i = 0; atts[i]; i += 2) {
        if (strcmp(atts[i], "alt") == 0) {
          alt = "[Image: " + std::string(atts[i + 1]) + "]";
        }
      }
      Serial.printf("[%lu] [EHP] Image alt: %s\n", millis(), alt.c_str());

      self->startNewTextBlock(TextBlock::CENTER_ALIGN);
      self->italicUntilDepth = min(self->italicUntilDepth, self->depth);
      self->depth += 1;
      self->characterData(userData, alt.c_str(), alt.length());

    } else {
      // Skip for now
      self->skipUntilDepth = self->depth;
      self->depth += 1;
      return;
    }
  }

  if (matches(name, SKIP_TAGS, NUM_SKIP_TAGS)) {
    // start skip
    self->skipUntilDepth = self->depth;
    self->depth += 1;
    return;
  }

  // Skip blocks with role="doc-pagebreak" and epub:type="pagebreak"
  if (atts != nullptr) {
    for (int i = 0; atts[i]; i += 2) {
      if (strcmp(atts[i], "role") == 0 && strcmp(atts[i + 1], "doc-pagebreak") == 0 ||
          strcmp(atts[i], "epub:type") == 0 && strcmp(atts[i + 1], "pagebreak") == 0) {
        self->skipUntilDepth = self->depth;
        self->depth += 1;
        return;
      }
    }
  }

  if (matches(name, HEADER_TAGS, NUM_HEADER_TAGS)) {
    self->startNewTextBlock(TextBlock::CENTER_ALIGN);
    self->boldUntilDepth = std::min(self->boldUntilDepth, self->depth);
  } else if (matches(name, BLOCK_TAGS, NUM_BLOCK_TAGS)) {
    if (strcmp(name, "br") == 0) {
      self->startNewTextBlock(self->currentTextBlock->getStyle());
    } else {
      self->startNewTextBlock((TextBlock::Style)self->paragraphAlignment);
      if (strcmp(name, "li") == 0) {
        self->currentTextBlock->addWord("\xe2\x80\xa2", EpdFontFamily::REGULAR);
      }
    }
  } else if (matches(name, BOLD_TAGS, NUM_BOLD_TAGS)) {
    self->boldUntilDepth = std::min(self->boldUntilDepth, self->depth);
  } else if (matches(name, ITALIC_TAGS, NUM_ITALIC_TAGS)) {
    self->italicUntilDepth = std::min(self->italicUntilDepth, self->depth);
  }

  self->depth += 1;
}

void XMLCALL ChapterHtmlSlimParser::characterData(void* userData, const XML_Char* s, const int len) {
  auto* self = static_cast<ChapterHtmlSlimParser*>(userData);

  // If we are collecting an inline footnote or paragraph note
  if (self->insideAsideFootnote) {
    int toCopy = std::min(len, MAX_ASIDE_BUFFER - self->currentAsideTextLen - 1);
    if (toCopy > 0) {
      memcpy(self->currentAsideText + self->currentAsideTextLen, s, toCopy);
      self->currentAsideTextLen += toCopy;
      self->currentAsideText[self->currentAsideTextLen] = '\0';
    }
    return;
  }

  if (self->insideParagraphNote) {
    int toCopy = std::min(len, MAX_PNOTE_BUFFER - self->currentParagraphNoteTextLen - 1);
    if (toCopy > 0) {
      memcpy(self->currentParagraphNoteText + self->currentParagraphNoteTextLen, s, toCopy);
      self->currentParagraphNoteTextLen += toCopy;
      self->currentParagraphNoteText[self->currentParagraphNoteTextLen] = '\0';
    }
    // Also continue to process if not in Pass 1, so it appears in the text too
  }

  if (self->insideNoteref) {
    int toCopy = std::min(len, 15 - self->currentNoterefTextLen);
    if (toCopy > 0) {
      memcpy(self->currentNoterefText + self->currentNoterefTextLen, s, toCopy);
      self->currentNoterefTextLen += toCopy;
      self->currentNoterefText[self->currentNoterefTextLen] = '\0';
    }
  }

  // During pass 1, we only care about asides and paragraph notes
  if (self->isPass1CollectingAsides) {
    return;
  }

  // Middle of skip
  if (self->skipUntilDepth < self->depth) {
    return;
  }

  EpdFontFamily::Style fontStyle = EpdFontFamily::REGULAR;
  if (self->boldUntilDepth < self->depth && self->italicUntilDepth < self->depth) {
    fontStyle = EpdFontFamily::BOLD_ITALIC;
  } else if (self->boldUntilDepth < self->depth) {
    fontStyle = EpdFontFamily::BOLD;
  } else if (self->italicUntilDepth < self->depth) {
    fontStyle = EpdFontFamily::ITALIC;
  }

  for (int i = 0; i < len; i++) {
    if (isWhitespace(s[i])) {
      // Currently looking at whitespace, if there's anything in the partWordBuffer, flush it
      if (self->partWordBufferIndex > 0) {
        self->partWordBuffer[self->partWordBufferIndex] = '\0';
        self->currentTextBlock->addWord(self->partWordBuffer, fontStyle);
        self->partWordBufferIndex = 0;
      }
      // Skip the whitespace char
      continue;
    }

    // Skip Zero Width No-Break Space / BOM (U+FEFF) = 0xEF 0xBB 0xBF
    const XML_Char FEFF_BYTE_1 = static_cast<XML_Char>(0xEF);
    const XML_Char FEFF_BYTE_2 = static_cast<XML_Char>(0xBB);
    const XML_Char FEFF_BYTE_3 = static_cast<XML_Char>(0xBF);

    if (s[i] == FEFF_BYTE_1) {
      // Check if the next two bytes complete the 3-byte sequence
      if ((i + 2 < len) && (s[i + 1] == FEFF_BYTE_2) && (s[i + 2] == FEFF_BYTE_3)) {
        // Sequence 0xEF 0xBB 0xBF found!
        i += 2;    // Skip the next two bytes
        continue;  // Move to the next iteration
      }
    }

    // If we're about to run out of space, then cut the word off and start a new one
    if (self->partWordBufferIndex >= MAX_WORD_SIZE) {
      self->partWordBuffer[self->partWordBufferIndex] = '\0';
      self->currentTextBlock->addWord(self->partWordBuffer, fontStyle);
      self->partWordBufferIndex = 0;
    }

    self->partWordBuffer[self->partWordBufferIndex++] = s[i];
  }

  // If we have > 750 words buffered up, perform the layout and consume out all but the last line
  // There should be enough here to build out 1-2 full pages and doing this will free up a lot of
  // memory.
  // Spotted when reading Intermezzo, there are some really long text blocks in there.
  if (self->currentTextBlock->size() > 750) {
    Serial.printf("[%lu] [EHP] Text block too long, splitting into multiple pages\n", millis());
    self->currentTextBlock->layoutAndExtractLines(
        self->renderer, self->fontId, self->viewportWidth,
        [self](const std::shared_ptr<TextBlock>& textBlock) { self->addLineToPage(textBlock); }, false);
  }
}

void XMLCALL ChapterHtmlSlimParser::endElement(void* userData, const XML_Char* name) {
  auto* self = static_cast<ChapterHtmlSlimParser*>(userData);

  // Handle end of aside footnote
  if (strcmp(name, "aside") == 0 && self->insideAsideFootnote && self->depth - 1 == self->asideDepth) {
    if (self->currentAsideTextLen > 0 && self->currentAsideId[0] != '\0') {
      self->inlineFootnotes.push_back({self->currentAsideId, self->currentAsideText});
      self->inlineFootnoteCount++;
    }
    self->insideAsideFootnote = false;
    self->depth -= 1;
    return;
  }

  // Handle end of paragraph note
  if (strcmp(name, "p") == 0 && self->insideParagraphNote && self->depth - 1 == self->paragraphNoteDepth) {
    if (self->currentParagraphNoteTextLen > 0 && self->currentParagraphNoteId[0] != '\0') {
      self->paragraphNotes.push_back({self->currentParagraphNoteId, self->currentParagraphNoteText});
      self->paragraphNoteCount++;
    }
    self->insideParagraphNote = false;
    self->depth -= 1;
    return;
  }

  // During pass 1, skip all other processing
  if (self->isPass1CollectingAsides) {
    self->depth -= 1;
    return;
  }

  // Handle end of noteref in pass 2
  if (strcmp(name, "a") == 0 && self->insideNoteref) {
    self->insideNoteref = false;
    if (self->currentNoterefTextLen > 0) {
      self->addFootnoteToCurrentPage(self->currentNoterefText, self->currentNoterefHref);
      if (self->noterefCallback && self->currentFootnoteCount > 0) {
        Noteref nr;
        strncpy(nr.number, self->currentNoterefText, 15);
        nr.number[15] = '\0';
        strncpy(nr.href, self->currentFootnotes[self->currentFootnoteCount - 1].href, 127);
        nr.href[127] = '\0';
        self->noterefCallback(nr);
      }
      char formattedNoteref[32];
      snprintf(formattedNoteref, sizeof(formattedNoteref), "[%s]", self->currentNoterefText);
      EpdFontFamily::Style fontStyle = EpdFontFamily::REGULAR;
      if (self->boldUntilDepth < self->depth && self->italicUntilDepth < self->depth) {
        fontStyle = EpdFontFamily::BOLD_ITALIC;
      } else if (self->boldUntilDepth < self->depth) {
        fontStyle = EpdFontFamily::BOLD;
      } else if (self->italicUntilDepth < self->depth) {
        fontStyle = EpdFontFamily::ITALIC;
      }
      if (self->currentTextBlock) {
        self->currentTextBlock->addWord(formattedNoteref, fontStyle);
      }
    }
    self->currentNoterefTextLen = 0;
    self->currentNoterefText[0] = '\0';
    self->currentNoterefHrefLen = 0;
    self->currentNoterefHref[0] = '\0';
    self->depth -= 1;
    return;
  }

  if (self->partWordBufferIndex > 0) {
    // Only flush out part word buffer if we're closing a block tag or are at the top of the HTML file.
    // We don't want to flush out content when closing inline tags like <span>.
    // Currently this also flushes out on closing <b> and <i> tags, but they are line tags so that shouldn't happen,
    // text styling needs to be overhauled to fix it.
    const bool shouldBreakText =
        matches(name, BLOCK_TAGS, NUM_BLOCK_TAGS) || matches(name, HEADER_TAGS, NUM_HEADER_TAGS) ||
        matches(name, BOLD_TAGS, NUM_BOLD_TAGS) || matches(name, ITALIC_TAGS, NUM_ITALIC_TAGS) || self->depth == 1;

    if (shouldBreakText) {
      EpdFontFamily::Style fontStyle = EpdFontFamily::REGULAR;
      if (self->boldUntilDepth < self->depth && self->italicUntilDepth < self->depth) {
        fontStyle = EpdFontFamily::BOLD_ITALIC;
      } else if (self->boldUntilDepth < self->depth) {
        fontStyle = EpdFontFamily::BOLD;
      } else if (self->italicUntilDepth < self->depth) {
        fontStyle = EpdFontFamily::ITALIC;
      }

      self->partWordBuffer[self->partWordBufferIndex] = '\0';
      self->currentTextBlock->addWord(self->partWordBuffer, fontStyle);
      self->partWordBufferIndex = 0;
    }
  }

  self->depth -= 1;

  // Leaving skip
  if (self->skipUntilDepth == self->depth) {
    self->skipUntilDepth = INT_MAX;
  }

  // Leaving bold
  if (self->boldUntilDepth == self->depth) {
    self->boldUntilDepth = INT_MAX;
  }

  // Leaving italic
  if (self->italicUntilDepth == self->depth) {
    self->italicUntilDepth = INT_MAX;
  }
}

bool ChapterHtmlSlimParser::parseAndBuildPages() {
  // ============================================================================
  // PASS 1: Extract all inline footnotes FIRST
  // ============================================================================
  Serial.printf("[%lu] [PARSER] === PASS 1: Extracting inline footnotes ===\n", millis());

  // Reset state for pass 1
  depth = 0;
  skipUntilDepth = INT_MAX;
  insideAsideFootnote = false;
  insideParagraphNote = false;
  inlineFootnoteCount = 0;
  paragraphNoteCount = 0;
  isPass1CollectingAsides = true;

  XML_Parser parser1 = XML_ParserCreate(nullptr);
  if (!parser1) {
    Serial.printf("[%lu] [EHP] Couldn't allocate memory for parser\n", millis());
    return false;
  }

  FsFile file1;
  if (!SdMan.openFileForRead("EHP", filepath, file1)) {
    XML_ParserFree(parser1);
    return false;
  }

  XML_SetUserData(parser1, this);
  XML_SetElementHandler(parser1, startElement, endElement);
  XML_SetCharacterDataHandler(parser1, characterData);

  int done;
  do {
    void* const buf = XML_GetBuffer(parser1, 1024);
    if (!buf) {
      XML_ParserFree(parser1);
      file1.close();
      return false;
    }
    const size_t len = file1.read(buf, 1024);
    done = file1.available() == 0;
    if (XML_ParseBuffer(parser1, static_cast<int>(len), done) == XML_STATUS_ERROR) {
      XML_ParserFree(parser1);
      file1.close();
      return false;
    }
  } while (!done);

  XML_ParserFree(parser1);
  file1.close();

  // ============================================================================
  // PASS 2: Build pages
  // ============================================================================
  Serial.printf("[%lu] [PARSER] === PASS 2: Building pages ===\n", millis());

  // Reset state for pass 2
  depth = 0;
  skipUntilDepth = INT_MAX;
  boldUntilDepth = INT_MAX;
  italicUntilDepth = INT_MAX;
  partWordBufferIndex = 0;
  insideNoteref = false;
  insideAsideFootnote = false;
  currentFootnoteCount = 0;
  isPass1CollectingAsides = false;

  startNewTextBlock((TextBlock::Style)this->paragraphAlignment);

  const XML_Parser parser2 = XML_ParserCreate(nullptr);
  if (!parser2) {
    Serial.printf("[%lu] [EHP] Couldn't allocate memory for parser\n", millis());
    return false;
  }

  FsFile file2;
  if (!SdMan.openFileForRead("EHP", filepath, file2)) {
    XML_ParserFree(parser2);
    return false;
  }

  const size_t totalSize = file2.size();
  size_t bytesRead = 0;
  int lastProgress = -1;

  XML_SetUserData(parser2, this);
  XML_SetElementHandler(parser2, startElement, endElement);
  XML_SetCharacterDataHandler(parser2, characterData);

  do {
    void* const buf = XML_GetBuffer(parser2, 1024);
    if (!buf) {
      XML_ParserFree(parser2);
      file2.close();
      return false;
    }

    const size_t len = file2.read(buf, 1024);

    bytesRead += len;
    if (progressFn && totalSize >= MIN_SIZE_FOR_PROGRESS) {
      const int progress = static_cast<int>((bytesRead * 100) / totalSize);
      if (lastProgress / 10 != progress / 10) {
        lastProgress = progress;
        progressFn(progress);
      }
    }

    done = file2.available() == 0;

    if (XML_ParseBuffer(parser2, static_cast<int>(len), done) == XML_STATUS_ERROR) {
      XML_ParserFree(parser2);
      file2.close();
      return false;
    }
  } while (!done);

  XML_ParserFree(parser2);
  file2.close();

  // Process last page if there is still text
  if (currentTextBlock) {
    makePages();
    if (currentPage) {
      for (int i = 0; i < currentFootnoteCount; i++) {
        currentPage->footnotes.push_back(currentFootnotes[i]);
      }
      currentFootnoteCount = 0;
    }
    completePageFn(std::move(currentPage));
    currentPage.reset();
    currentTextBlock.reset();
  }

  return true;
}

void ChapterHtmlSlimParser::addLineToPage(std::shared_ptr<TextBlock> line) {
  const int lineHeight = renderer.getLineHeight(fontId) * lineCompression;

  if (currentPageNextY + lineHeight > viewportHeight) {
    if (currentPage) {
      for (int i = 0; i < currentFootnoteCount; i++) {
        currentPage->footnotes.push_back(currentFootnotes[i]);
      }
      currentFootnoteCount = 0;
    }
    completePageFn(std::move(currentPage));
    currentPage.reset(new Page());
    currentPageNextY = 0;
  }

  currentPage->elements.push_back(std::make_shared<PageLine>(line, 0, currentPageNextY));
  currentPageNextY += lineHeight;
}

void ChapterHtmlSlimParser::makePages() {
  if (!currentTextBlock) {
    Serial.printf("[%lu] [EHP] !! No text block to make pages for !!\n", millis());
    return;
  }

  if (!currentPage) {
    currentPage.reset(new Page());
    currentPageNextY = 0;
  }

  const int lineHeight = renderer.getLineHeight(fontId) * lineCompression;
  currentTextBlock->layoutAndExtractLines(
      renderer, fontId, viewportWidth,
      [this](const std::shared_ptr<TextBlock>& textBlock) { addLineToPage(textBlock); });
  // Extra paragraph spacing if enabled
  if (extraParagraphSpacing) {
    currentPageNextY += lineHeight / 2;
  }
}
