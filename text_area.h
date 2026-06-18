#pragma once
#include <SDL.h>
#include <SDL_ttf.h>
#include "RAII.h"
#include "Iplayable.h"
#include "engine_events.h"
#include "text_decoder.h"

namespace TextAreaImpl
{
    constexpr float LeftPadding = 20.0f;
    constexpr float SpeakerTopPadding = 12.0f;
    constexpr float BodyTopPaddingNoSpeaker = 16.0f;
    constexpr float BodyTopPaddingWithSpeaker = 54.0f;
    constexpr float BodyBottomPadding = 24.0f;

    bool ifcontain(const SDL_FRect& rect, float x, float y)
    {
        return x >= rect.x && x <= rect.x + rect.w &&
               y >= rect.y && y <= rect.y + rect.h;
    }

    bool IsAsciiWordChar(char ch)
    {
        return std::isalnum(static_cast<unsigned char>(ch)) || ch == '_';
    }

    std::string NormalizeRenderText(const std::string& text)
    {
        std::string result;
        result.reserve(text.size());

        for (size_t i = 0; i < text.size(); ++i)
        {
            char ch = text[i];
            if (ch != '\r' && ch != '\n')
            {
                result += ch;
                continue;
            }

            while (i + 1 < text.size() && (text[i + 1] == '\r' || text[i + 1] == '\n' ||
                   text[i + 1] == ' ' || text[i + 1] == '\t'))
            {
                ++i;
            }

            char prev = result.empty() ? '\0' : result.back();
            char next = (i + 1 < text.size()) ? text[i + 1] : '\0';
            if (IsAsciiWordChar(prev) && IsAsciiWordChar(next))
                result += ' ';
        }

        return result;
    }

    size_t NextUtf8(const std::string& s, size_t pos)
    {
        if (pos >= s.size()) return pos;

        unsigned char c = static_cast<unsigned char>(s[pos]);
        size_t n = 1;

        if ((c & 0x80) == 0) n = 1;
        else if ((c & 0xE0) == 0xC0) n = 2;
        else if ((c & 0xF0) == 0xE0) n = 3;
        else if ((c & 0xF8) == 0xF0) n = 4;
        else return pos + 1;

        if (pos + n > s.size()) return pos + 1;

        for (size_t i = 1; i < n; ++i)
        {
            c = static_cast<unsigned char>(s[pos + i]);
            if ((c & 0xC0) != 0x80)
                return pos + 1;
        }

        return pos + n;
    }

    char32_t DecodeUtf8Codepoint(const std::string& s, size_t pos)
    {
        if (pos >= s.size()) return U'\0';

        unsigned char c0 = static_cast<unsigned char>(s[pos]);
        size_t next = NextUtf8(s, pos);
        size_t n = next > pos ? next - pos : 1;

        if (n == 1) return c0;
        if (n == 2)
        {
            unsigned char c1 = static_cast<unsigned char>(s[pos + 1]);
            return ((c0 & 0x1F) << 6) | (c1 & 0x3F);
        }
        if (n == 3)
        {
            unsigned char c1 = static_cast<unsigned char>(s[pos + 1]);
            unsigned char c2 = static_cast<unsigned char>(s[pos + 2]);
            return ((c0 & 0x0F) << 12) | ((c1 & 0x3F) << 6) | (c2 & 0x3F);
        }
        if (n == 4)
        {
            unsigned char c1 = static_cast<unsigned char>(s[pos + 1]);
            unsigned char c2 = static_cast<unsigned char>(s[pos + 2]);
            unsigned char c3 = static_cast<unsigned char>(s[pos + 3]);
            return ((c0 & 0x07) << 18) | ((c1 & 0x3F) << 12) |
                   ((c2 & 0x3F) << 6) | (c3 & 0x3F);
        }

        return c0;
    }

    bool IsPunctuation(char32_t ch)
    {
        switch (ch)
        {
            case U'.': case U',': case U'?': case U'!':
            case U';': case U':':
            case U'。': case U'，': case U'？': case U'！':
            case U'；': case U'：': case U'、':
            case U'…':
                return true;
            default:
                return false;
        }
    }

    // The return is in sec
    double CharDelay(double charsPerSecond, double punctuationDelay, char32_t ch)
    {
        if (charsPerSecond <= 0.0) return 0.0;

        double delay = 1.0 / charsPerSecond;
        if (IsPunctuation(ch)) delay += punctuationDelay;
        return delay;
    }

    float BodyTopPadding(bool hasSpeaker){ return hasSpeaker ? BodyTopPaddingWithSpeaker : BodyTopPaddingNoSpeaker; }

    int MeasureTextWidth(TTF_Font* font, const std::string& text)
    {
        if (!font || text.empty()) return 0;

        int width = 0;
        if (!TTF_MeasureString(font, text.c_str(), text.size(), 0, &width, nullptr))
            return 0;
        return width;
    }

    std::vector<std::string> WrapTextByCodepoint(TTF_Font* font, const std::string& text, int maxWidth)
    {
        std::vector<std::string> lines;
        std::string line;

        size_t pos = 0;
        while (pos < text.size())
        {
            size_t next = NextUtf8(text, pos);
            if (next <= pos) next = pos + 1;

            std::string glyph = text.substr(pos, next - pos);
            std::string candidate = line + glyph;

            if (!line.empty() && MeasureTextWidth(font, candidate) > maxWidth)
            {
                lines.push_back(line);
                line.clear();

                if (glyph != " " && glyph != "\t")
                    line = glyph;
            }
            else
            {
                line = std::move(candidate);
            }

            pos = next;
        }

        if (!line.empty()) lines.push_back(line);
        return lines;
    }

    SDL_Texture* BuildMultilineTextTexture(
        SDL_Renderer* ren,
        TTF_Font* font,
        const std::string& text,
        const SDL_Color& color,
        const SDL_FRect& areaRect,
        bool hasSpeaker)
    {
        if (!ren || !font || text.empty()) return nullptr;

        int wrapWidth = static_cast<int>(std::max(1.0f, areaRect.w - LeftPadding * 2.0f));
        std::vector<std::string> lines = WrapTextByCodepoint(font, text, wrapWidth);
        if (lines.empty()) return nullptr;

        std::vector<Surface_ptr> lineSurfaces;
        lineSurfaces.reserve(lines.size());

        int textureWidth = 1;
        int lineSkip = TTF_GetFontLineSkip(font);
        if (lineSkip <= 0) lineSkip = TTF_GetFontHeight(font);
        if (lineSkip <= 0) lineSkip = 1;

        for (const std::string& line : lines)
        {
            SDL_Surface* lineSurface = TTF_RenderText_Blended(font, line.c_str(), line.size(), color);
            if (!lineSurface) continue;

            textureWidth = std::max(textureWidth, lineSurface->w);
            lineSurfaces.emplace_back(lineSurface);
        }

        if (lineSurfaces.empty()) return nullptr;

        int textureHeight = lineSkip * static_cast<int>(lineSurfaces.size());
        float maxBodyHeight = areaRect.h - BodyTopPadding(hasSpeaker) - BodyBottomPadding;
        if (textureHeight > static_cast<int>(maxBodyHeight))
            SDL_Log("TextArea text exceeds target height: %d > %.0f", textureHeight, maxBodyHeight);

        Surface_ptr composedSurface(SDL_CreateSurface(textureWidth, textureHeight, SDL_PIXELFORMAT_ARGB8888));
        if (!composedSurface) return nullptr;

        SDL_FillSurfaceRect(composedSurface.get(), nullptr, 0x00000000);

        int y = 0;
        for (const Surface_ptr& lineSurface : lineSurfaces)
        {
            SDL_Rect dst{0, y, lineSurface->w, lineSurface->h};
            SDL_BlitSurface(lineSurface.get(), nullptr, composedSurface.get(), &dst);
            y += lineSkip;
        }

        return SDL_CreateTextureFromSurface(ren, composedSurface.get());
    }

    SDL_Texture* BuildSingleLineTextTexture(
        SDL_Renderer* ren,
        TTF_Font* font,
        const std::string& text,
        const SDL_Color& color)
    {
        if (!ren || !font || text.empty()) return nullptr;

        SDL_Surface* surface = TTF_RenderText_Blended(font, text.c_str(), text.size(), color);
        if (!surface) return nullptr;

        Surface_ptr lineSurface(surface);
        return SDL_CreateTextureFromSurface(ren, lineSurface.get());
    }
}

class TextArea : public Iplayable
{
public:
    enum class Action
    {
        None,
        Consumed,
        Skip,
        PrevPage,
        NextPage
    };

protected:
    enum class PlayState
    {
        Stopped,
        Playing,
        Paused,
        Finished
    };

    Text areaText;
    const stdData* scriptData = nullptr;
    chapterPage currentPage{1, 1};
    TTF_Font* font1 = nullptr;
    TTF_Font* font2 = nullptr;
    SDL_FRect areaRect{120.0f, 760.0f, 1680.0f, 240.0f};
    SDL_Color textColor{255, 255, 255, 255};
    SDL_Color backgroundColor{16, 16, 24, 210};

    std::string visibleText;
    std::string currentSegment;
    size_t eventIndex = 0;
    size_t charByteIndex = 0;

    double charsPerSecond = 35.0;
    double punctuationDelay = 0.14;
    double charTimer = 0.0;

    PlayState playState = PlayState::Stopped;
    double hideTimer = 0.0;
    double cursorBlinkTimer = 0.0;

    SDL_Texture* textTexture = nullptr;
    SDL_Texture* speakerTexture = nullptr;
    bool textDirty = true;
    bool speakerDirty = true;

    bool IsFinished() const
    {
        return playState == PlayState::Finished;
    }

    void MarkAllTextDirty()
    {
        textDirty = true;
        speakerDirty = true;
    }

    std::string CurrentRenderText() const
    {
        return TextAreaImpl::NormalizeRenderText(visibleText + currentSegment.substr(0, charByteIndex));
    }

    void RebuildTextTexture(SDL_Renderer* ren)
    {
        if(!textTexture){ SDL_DestroyTexture(textTexture); textTexture = nullptr; }
        textDirty = false;
        textTexture = TextAreaImpl::BuildMultilineTextTexture(
            ren, font1, CurrentRenderText(), textColor, areaRect, !areaText.speaker.empty());
    }

    void RebuildSpeakerTexture(SDL_Renderer* ren)
    {
        if(!speakerTexture){ SDL_DestroyTexture(speakerTexture); speakerTexture = nullptr; }
        speakerDirty = false;
        speakerTexture = TextAreaImpl::BuildSingleLineTextTexture(ren, font2, areaText.speaker, textColor);
    }

    void DispatchEvents(const std::vector<TextEvent>& events)
    {
        for (const TextEvent& textEvent : events)
        {
            stdEngineEvents::Dispatch(textEvent);

            if (textEvent.getEvent() == EVENT::hide)
            {
                hideTimer = std::max(0.0, textEvent.getParam() / 1000.0);
            }
        }
    }

    // Page end
    void FinishPage()
    {
        playState = PlayState::Finished;
        textDirty = true;
    }

    void ResetPageState(const Text& text)
    {
        areaText = text;
        visibleText.clear();
        currentSegment.clear();
        eventIndex = 0;
        charByteIndex = 0;
        charTimer = 0.0;
        hideTimer = 0.0;
        cursorBlinkTimer = 0.0;
        playState = PlayState::Playing;
        MarkAllTextDirty();
    }

    //
    bool FindAdjacentPage(chapterPage& out, bool forward) const
    {
        if (!scriptData) return false;

        bool found = false;
        for (const auto& [page, text] : *scriptData)
        {
            (void)text;

            bool isCandidate = forward ? currentPage < page
                                       : page < currentPage;
            if (!isCandidate) continue;

            bool isBetter = !found ||
                            (forward ? page < out
                                     : out < page);
            if (isBetter)
            {
                out = page;
                found = true;
            }
        }

        return found;
    }

    // Skip logic
    void SkipToPageEnd()
    {
        if (IsFinished()) return;

        if (!currentSegment.empty())
        {
            visibleText += currentSegment;
            currentSegment.clear();
            charByteIndex = 0;
            ++eventIndex;
        }

        while (eventIndex < areaText.eventList.size())
        {
            const auto& item = areaText.eventList[eventIndex];
            if (std::holds_alternative<std::string>(item))
            {
                visibleText += std::get<std::string>(item);
            }
            else
            {
                DispatchEvents(std::get<std::vector<TextEvent>>(item));
            }
            ++eventIndex;
        }

        FinishPage();
    }

    // Display next character
    void AdvanceText(double dt)
    {
        if (charsPerSecond <= 0.0)
        {
            charByteIndex = currentSegment.size();
            textDirty = true;
            return;
        }

        charTimer += dt;
        while (charByteIndex < currentSegment.size())
        {
            size_t next = TextAreaImpl::NextUtf8(currentSegment, charByteIndex);
            if (next <= charByteIndex) next = charByteIndex + 1;

            char32_t ch = TextAreaImpl::DecodeUtf8Codepoint(currentSegment, charByteIndex);
            double delay = TextAreaImpl::CharDelay(charsPerSecond, punctuationDelay, ch);
            if (charTimer < delay) break;

            charByteIndex = next;
            charTimer -= delay;
            textDirty = true;
        }
    }

    // Skip Action confirm
    Action ToggleAdvance()
    {
        if (hideTimer > 0.0) return Action::None;
        if (playState == PlayState::Stopped) return Action::None;

        if (!IsFinished())
        {
            SkipToPageEnd();
            return Action::Skip;
        }

        return NextPage() ? Action::NextPage : Action::None;
    }

    // Pause Action confirm
    Action TogglePause()
    {
        if (playState == PlayState::Playing) playState = PlayState::Paused;
        else if (playState == PlayState::Paused) playState = PlayState::Playing;
        else return Action::None;

        return Action::Consumed;
    }

public:
    TextArea() = default;
    ~TextArea() override { Close(); }

    void SetFont(TTF_Font* newFont1, TTF_Font* newFont2 = nullptr)
    {
        if(!newFont1) return;
        font1 = newFont1;
        font2 = newFont2 ? newFont2 : newFont1;
        MarkAllTextDirty();
    }

    void SetArea(const SDL_FRect& rect)
    {
        if(rect.w <= 0.0f || rect.h <= 0.0f) return;
        areaRect = rect;
        MarkAllTextDirty();
    }

    void SetColor(const SDL_Color& color)
    {
        if(color.a == 0) return;
        textColor = color;
        MarkAllTextDirty();
    }

    void SetSpeed(double cps)
    {
        if(cps <= 0.0) return;
        charsPerSecond = cps;
    }

    void SetPunctuationDelay(double seconds)
    {
        if(seconds <= 0.0) return;
        punctuationDelay = std::max(0.0, seconds);
    }

    bool LoadData(const stdData& data, chapterPage start = {1, 1})
    {
        scriptData = &data;
        return LoadPage(start);
    }

    bool LoadPage(chapterPage page, bool revealAll = false)
    {
        if (!scriptData) return false;

        auto it = scriptData->find(page);
        if (it == scriptData->end()) return false;

        currentPage = page;
        ResetPageState(it->second);
        if (revealAll) SkipToPageEnd();
        return true;
    }

    void LoadPage(const Text& text)
    {
        scriptData = nullptr;
        ResetPageState(text);
    }

    bool NextPage()
    {
        chapterPage next{};
        return FindAdjacentPage(next, true) && LoadPage(next);
    }

    bool PrevPage()
    {
        chapterPage prev{};
        return FindAdjacentPage(prev, false) && LoadPage(prev, true);
    }

    Action HandleEvent(const SDL_Event& e)
    {
        if (e.type == SDL_EVENT_MOUSE_BUTTON_UP && e.button.button == SDL_BUTTON_LEFT)
        {
            if (!TextAreaImpl::ifcontain(areaRect, e.button.x, e.button.y))
                return Action::None;
            return ToggleAdvance();
        }

        if (e.type == SDL_EVENT_KEY_DOWN)
        {
            if (e.key.key == SDLK_SPACE)
                return TogglePause();

            if (IsFinished())
            {
                if (e.key.key == SDLK_LEFT)
                    return PrevPage() ? Action::PrevPage : Action::None;
                if (e.key.key == SDLK_RIGHT)
                    return NextPage() ? Action::NextPage : Action::None;
            }
        }

        return Action::None;
    }

    bool Update(double dt) override
    {
        cursorBlinkTimer += dt;

        if (hideTimer > 0.0)
        {
            hideTimer -= dt;
            if (hideTimer > 0.0) return false;
            hideTimer = 0.0;
        }

        if (playState == PlayState::Stopped || playState == PlayState::Finished) return false;
        if (playState == PlayState::Paused) return false;

        while (eventIndex < areaText.eventList.size() && currentSegment.empty())
        {
            const auto& item = areaText.eventList[eventIndex];
            if (std::holds_alternative<std::string>(item))
            {
                const std::string& segment = std::get<std::string>(item);
                if (segment.empty())
                {
                    ++eventIndex;
                    continue;
                }

                currentSegment = segment;
                charByteIndex = 0;
                charTimer = 0.0;
                textDirty = true;
                break;
            }

            DispatchEvents(std::get<std::vector<TextEvent>>(item));
            ++eventIndex;
            if (hideTimer > 0.0) return false;
        }

        if (eventIndex >= areaText.eventList.size() && currentSegment.empty())
        {
            FinishPage();
            return false;
        }

        if (!currentSegment.empty())
        {
            AdvanceText(dt);
            if (charByteIndex >= currentSegment.size())
            {
                visibleText += currentSegment;
                currentSegment.clear();
                charByteIndex = 0;
                ++eventIndex;
                textDirty = true;
            }
        }

        return IsPlaying();
    }

    void Render(SDL_Renderer* ren, const SDL_FRect* dstRect = nullptr) override
    {
        if (!ren || hideTimer > 0.0) return;

        SDL_FRect rect = dstRect ? *dstRect : areaRect;
        SDL_SetRenderDrawColor(
            ren,
            backgroundColor.r,
            backgroundColor.g,
            backgroundColor.b,
            backgroundColor.a);
        SDL_RenderFillRect(ren, &rect);

        if (speakerDirty) RebuildSpeakerTexture(ren);
        if (speakerTexture)
        {
            float speakerW = 0.0f;
            float speakerH = 0.0f;
            SDL_GetTextureSize(speakerTexture, &speakerW, &speakerH);
            SDL_FRect speakerDst{
                rect.x + TextAreaImpl::LeftPadding,
                rect.y + TextAreaImpl::SpeakerTopPadding,
                speakerW,
                speakerH
            };
            SDL_RenderTexture(ren, speakerTexture, nullptr, &speakerDst);
        }

        if (textDirty) RebuildTextTexture(ren);
        if (textTexture)
        {
            float texW = 0.0f;
            float texH = 0.0f;
            SDL_GetTextureSize(textTexture, &texW, &texH);

            SDL_FRect textDst{
                rect.x + TextAreaImpl::LeftPadding,
                rect.y + TextAreaImpl::BodyTopPadding(!areaText.speaker.empty()),
                texW,
                texH
            };
            SDL_RenderTexture(ren, textTexture, nullptr, &textDst);
        }

        if (IsFinished())
        {
            float blink = 0.5f + 0.5f * std::sin(static_cast<float>(cursorBlinkTimer) * 8.0f);
            Uint8 alphaValue = static_cast<Uint8>(70.0f + blink * 185.0f);
            SDL_SetRenderDrawColor(ren, 255, 255, 255, alphaValue);
            SDL_FRect mark{rect.x + rect.w - 42.0f, rect.y + rect.h - 34.0f, 16.0f, 16.0f};
            SDL_RenderFillRect(ren, &mark);
        }
    }

    bool IsPlaying() const override
    {
        return playState == PlayState::Playing || playState == PlayState::Paused;
    }

    void Close() override
    {
        if(!textTexture){ SDL_DestroyTexture(textTexture); textTexture = nullptr; }
        if(!speakerTexture){ SDL_DestroyTexture(speakerTexture); speakerTexture = nullptr; }
        playState = PlayState::Stopped;
    }
  
};