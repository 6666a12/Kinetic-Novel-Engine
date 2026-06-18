/*
    NOT FULLY COMPLETE!

    Text DECODER specially designed for the project.
    '#chapter'     stands for chapter switch
    '|||'          stands for Page switch
    '|0|'          stands for Ending
    '|b{mode}|'    stands for Flashbang effect
    '|f|'          stands for FULLSCREEN TEXT MOD
    '|p{number}|'  stands for picture switch
    '|m{number}|'  stands for music switch
    '|mp|'         stands for music pause
    '|h{number}|'  stands for hide the TextArea and reshow after {interval} msec
    All number and mode have default value.

    |{}| events above are ADDABLE!
    for example: |p3m4h| means switch to picture 3, music 4, and hide.

    in the same page,
    & {text} & means the speaker
    U can use &0& or && as an EMPTY speaker (narration without speaker)
*/

#pragma once
#include <fstream>
#include <functional>
#include <algorithm>
#include <variant>
#include <string>
#include <vector>
#include <unordered_map>
#include "scc.h"

// Fantastic? C++
#define stdEvent std::vector<std::variant<std::string, std::vector<TextEvent>>>
#define stdData std::unordered_map<chapterPage, Text>

struct chapterPage
{
    int chapter;
    int page;

    bool operator==(const chapterPage& other) const = default;
    bool operator<(const chapterPage& a) const {
        return this->chapter < a.chapter ||
               (this->chapter == a.chapter && this->page < a.page);
    }

    bool operator>(const chapterPage& a) const {
        return this->chapter > a.chapter ||
               (this->chapter == a.chapter && this->page > a.chapter);
    }
};

enum class EVENT
{
    none = 0,
    Flash = 1,
    Fullscreen = 2,
    Pictures = 3,
    musics = 4,
    musicp = 5,
    hide = 6,
    ending = 7
};

class TextEvent
{
protected:
    EVENT e = EVENT::none;
    int interval = 0;

public:
    TextEvent() = default;
    TextEvent(EVENT ev, int intervalMs = 0)
        : e(ev), interval(intervalMs) {}

    EVENT getEvent() const { return this->e; }
    int   getParam() const { return this->interval; }

};

struct Text
{
    stdEvent eventList;  
    std::string speaker;
};

namespace std
{
    template<>
    struct hash<chapterPage>
    {
        std::size_t operator()(const chapterPage& cp) const noexcept { 
            return std::hash<int>{}(cp.chapter) ^ (std::hash<int>{}(cp.page) << 1); 
        }        
    };
};

namespace TextDecoderImpl
{
    inline std::string Trim(const std::string& s)
    {
        size_t start = 0;
        while (start < s.size() && std::isspace(static_cast<unsigned char>(s[start])))
            ++start;

        size_t end = s.size();
        while (end > start && std::isspace(static_cast<unsigned char>(s[end - 1])))
            --end;

        return s.substr(start, end - start);
    }

    inline std::vector<std::string> Split(const std::string& text, const std::string& delimiter)
    {
        std::vector<std::string> parts;
        size_t start = 0;
        size_t end = text.find(delimiter);

        while (end != std::string::npos)
        {
            parts.push_back(text.substr(start, end - start));
            start = end + delimiter.length();
            end = text.find(delimiter, start);
        }

        parts.push_back(text.substr(start));
        return parts;
    }

    inline std::vector<TextEvent> ParseEvent(const std::string& block)
    {
        std::vector<TextEvent> result;
        size_t i = 0;

        while(i < block.size())
        {
            char ch = block[i];
            std::string num;
            num.clear();

            size_t j = i+1;
            while(j < block.size() && std::isdigit(static_cast<unsigned char>(block[j])))
            {
                num += block[j];
                ++j;
            }

            int interval = num.empty() ? 0 : std::stoi(num);

            switch(ch){
                case '0':
                    result.emplace_back(EVENT::ending, interval);
                    break;

                case 'b':
                    result.emplace_back(EVENT::Flash, interval);
                    break;

                case 'f':
                    result.emplace_back(EVENT::Fullscreen, interval);
                    break;

                case 'p':
                    result.emplace_back(EVENT::Pictures, interval);
                    break;

                case 'm':
                    if(j < block.size() && block[j] == 'p' && ++j) result.emplace_back(EVENT::musicp, 0);
                    else result.emplace_back(EVENT::musics, interval);
                    
                    break;

                case 'h':
                    result.emplace_back(EVENT::hide, interval);
                    
            }
            i = j;
        }
        return result;

    }

    inline stdEvent ParseEventList(const std::string& s)
    {
        stdEvent result;
        std::string currentText;
        size_t i = 0;

        while(i < s.size())
        {
            size_t start = s.find('|', i);
            if(start == std::string::npos)
            {
                currentText += s.substr(i);
                break;
            }

            currentText += s.substr(i, start - i);

            size_t end = s.find('|', start+ + 1);
            if(end == std::string::npos)
            {
                currentText += s.substr(start);
                break;
            }

            if(!Trim(currentText).empty())
            {
                result.emplace_back(Trim(currentText));
                currentText.clear();
            }

            std::string block = s.substr(start + 1, end - start - 1);
            auto events = ParseEvent(block);
            if(!events.empty()) result.emplace_back(std::move(events));

            i = end+ + 1;
        }

        if(!Trim(currentText).empty()) result.emplace_back(Trim(currentText));

        return result;
    }
}

stdData TextDecoder(const char* path)
{
    // Result container
    std::unordered_map<chapterPage, Text> result;

    std::ifstream file(path, std::ios::binary);
    scc(file.is_open(), "File Open Failed:");

    std::string buffer((std::istreambuf_iterator<char>(file)),
                       std::istreambuf_iterator<char>());
    
    // UTF-8 BOM
    if(buffer.size() >= 3 &&
       static_cast<unsigned char>(buffer[0]) == 0xEF &&
       static_cast<unsigned char>(buffer[1]) == 0xBB &&
       static_cast<unsigned char>(buffer[2]) == 0xBF) buffer.erase(0, 3);

    size_t endPos = buffer.find("|0|");
    if(endPos != std::string::npos) buffer.erase(endPos);

    std::vector<std::string> pages = TextDecoderImpl::Split(buffer, "|||");

    int pageN = 1;
    int chapN = 1;
    for(const std::string& rawPage : pages)
    {
        std::string page = TextDecoderImpl::Trim(rawPage);
        if(page.empty()) continue;

        Text entry;
        entry.speaker.clear();

        // #chapter
        if(page.substr(0, 7) == "#chapter"){
            chapN++;
            pageN = 1;
            page.erase(0, 7);
            page = TextDecoderImpl::Trim(page);
        }
        
        if(page.front() == '&'){
            size_t next = page.find('&', 1);
            scc(next != std::string::npos, "File format error, please check your file! :");

            std::string speaker = page.substr(1, next - 1);
            if(speaker != "0" && !speaker.empty()) entry.speaker = std::move(speaker);
            entry.eventList = TextDecoderImpl::ParseEventList(TextDecoderImpl::Trim(page.substr(next + 1)));
        }

        if(!entry.eventList.empty() || !entry.speaker.empty()){
            result[{chapN, pageN}] = std::move(entry);
            ++pageN;
        }
    }

    return result;
}
