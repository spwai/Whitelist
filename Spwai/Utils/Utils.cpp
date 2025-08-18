#include "Utils.hpp"
#define NOMINMAX
#include <Windows.h>
#include <iostream>
#include <vector>
#include <string>
#include <unordered_set>
#include <wininet.h>
#include <nlohmann/json.hpp>
#include <algorithm>
#include <cctype>

#pragma comment(lib, "wininet.lib")

extern std::unordered_set<std::string> g_msrPlayers;
extern std::unordered_set<std::string> g_qtPlayers;

std::string sanitizeName(const std::string& name) {
    std::string sanitized;
    sanitized.reserve(name.length());
    
    for (size_t i = 0; i < name.length(); ++i) {
        char c = name[i];
        
        if (c == '§') {
            if (i + 1 < name.length()) {
                i++;
            }
            continue;
        }
        
        if (c == '\n') {
            continue;
        }
        
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || 
            c == ' ' || c == '_' || c == '-') {
            sanitized += (c >= 'A' && c <= 'Z') ? (c + 32) : c;
        }
    }
    
    return sanitized;
}

std::string extractName(void** nameTag) {
    if (!nameTag || *nameTag == nullptr) return "";
    
    if (IsBadReadPtr(nameTag, sizeof(void*)) == FALSE && *nameTag) {
        uintptr_t ptrValue = reinterpret_cast<uintptr_t>(*nameTag);
        
        if (ptrValue < 0x1000) {
            return "";
        }
        
        if (ptrValue > 0x1000000000000ULL) {
            std::string name;
            name.reserve(16);
            
            for (int i = 0; i < 8; i++) {
                char c = static_cast<char>((ptrValue >> (i * 8)) & 0xFF);
                if (c == 0) break;
                name += c;
            }
            
            if (IsBadReadPtr(nameTag, 16) == FALSE) {
                uint64_t* nextWord = reinterpret_cast<uint64_t*>(nameTag) + 1;
                uint64_t nextValue = *nextWord;
                
                for (int i = 0; i < 8; i++) {
                    char c = static_cast<char>((nextValue >> (i * 8)) & 0xFF);
                    if (c == 0) break;
                    name += c;
                }
            }
            
            return name;
        }
        
        if (IsBadReadPtr(*nameTag, 32) == FALSE) {
            uint64_t* stringStruct = reinterpret_cast<uint64_t*>(*nameTag);
            std::string name;
            name.reserve(24);
            
            for (int word = 0; word < 3; word++) {
                uint64_t wordData = stringStruct[word];
                
                for (int i = 0; i < 8; i++) {
                    char c = static_cast<char>((wordData >> (i * 8)) & 0xFF);
                    if (c == 0) break;
                    name += c;
                }
            }
            
            if (name.length() > 0) {
                return name;
            }
        }
    }
    
    return "";
}

std::string downloadJson(const std::string& url) {
    std::string result;
    HINTERNET hInternet = InternetOpenA("Spwai", INTERNET_OPEN_TYPE_DIRECT, NULL, NULL, 0);
    
    if (hInternet) {
        HINTERNET hUrl = InternetOpenUrlA(hInternet, url.c_str(), NULL, 0, INTERNET_FLAG_RELOAD, 0);
        
        if (hUrl) {
            char buffer[1024];
            DWORD bytesRead;
            
            while (InternetReadFile(hUrl, buffer, sizeof(buffer) - 1, &bytesRead) && bytesRead > 0) {
                buffer[bytesRead] = 0;
                result += buffer;
            }
            
            InternetCloseHandle(hUrl);
        }
        
        InternetCloseHandle(hInternet);
    }
    
    return result;
}

void loadLists() {
    std::cout << "Loading player lists from database..." << std::endl;
    
    std::string jsonData = downloadJson("https://spwai.github.io/db/igns.json");
    
    if (!jsonData.empty()) {
        try {
            auto json = nlohmann::json::parse(jsonData);
            
            if (json.contains("msr") && json["msr"].is_array()) {
                g_msrPlayers.clear();
                g_msrPlayers.reserve(json["msr"].size());
                for (const auto& player : json["msr"]) {
                    std::string playerName = player.get<std::string>();
                    std::transform(playerName.begin(), playerName.end(), playerName.begin(), ::tolower);
                    g_msrPlayers.insert(playerName);
                }
                std::cout << "Loaded " << g_msrPlayers.size() << " MSR players" << std::endl;
            }
            
            if (json.contains("qt") && json["qt"].is_array()) {
                g_qtPlayers.clear();
                g_qtPlayers.reserve(json["qt"].size());
                for (const auto& player : json["qt"]) {
                    std::string playerName = player.get<std::string>();
                    std::transform(playerName.begin(), playerName.end(), playerName.begin(), ::tolower);
                    g_qtPlayers.insert(playerName);
                }
                std::cout << "Loaded " << g_qtPlayers.size() << " QT players" << std::endl;
            }
            
        } catch (const std::exception& e) {
            std::cout << "Error parsing JSON: " << e.what() << std::endl;
        }
    } else {
        std::cout << "Failed to download player database" << std::endl;
    }
}

bool isInList(const std::string& sanitizedName, const std::unordered_set<std::string>& playerList) {
    return playerList.find(sanitizedName) != playerList.end();
}

bool isSpecialName(const std::string& sanitizedName) {
    static const std::unordered_set<std::string> specialNames = {
        "spwai", "kyioly", "kyiolyi", "kyiolyy", "kyiolys", "kyioly9", 
        "kyioly00", "karniege", "sakovaeli", "ims wet fmbyy", "vzwri", "vzwra", "yvmli"
    };
    return specialNames.find(sanitizedName) != specialNames.end();
}