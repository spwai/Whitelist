#include "Utils.hpp"
#define NOMINMAX
#include <Windows.h>
#include <iostream>
#include <vector>
#include <string>
#include <wininet.h>
#include <nlohmann/json.hpp>
#include <algorithm>
#include <cctype>

#pragma comment(lib, "wininet.lib")

extern std::vector<std::string> g_msrPlayers;
extern std::vector<std::string> g_qtPlayers;

std::string sanitizeName(const std::string& name) {
    std::string sanitized;
    sanitized.reserve(name.length());
    
    std::string tempName = name;
    size_t pos = 0;
    
    while ((pos = tempName.find('§', pos)) != std::string::npos) {
        if (pos + 1 < tempName.length()) {
            tempName.erase(pos, 2);
        } else {
            tempName.erase(pos, 1);
        }
    }
    
    pos = 0;
    while ((pos = tempName.find('\n', pos)) != std::string::npos) {
        tempName.erase(pos, 1);
    }
    
    for (char c : tempName) {
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || 
            c == ' ' || c == '_' || c == '-') {
            sanitized += c;
        }
    }
    std::transform(sanitized.begin(), sanitized.end(), sanitized.begin(), ::tolower);
    return sanitized;
}

std::string extractName(void** nameTag) {
    if (!nameTag || *nameTag == nullptr) return "";
    
    if (IsBadReadPtr(nameTag, sizeof(void*)) == FALSE && *nameTag) {
        uintptr_t ptrValue = reinterpret_cast<uintptr_t>(*nameTag);
        
        if (ptrValue < 0x1000) return "";
        
        if (ptrValue > 0x1000000000000ULL) {
            std::string name;
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
        
        if (IsBadReadPtr(*nameTag, 256) == FALSE) {
            uint64_t* stringStruct = reinterpret_cast<uint64_t*>(*nameTag);
            std::string name;
            
            for (int word = 0; word < 32; word++) {
                uint64_t wordData = stringStruct[word];
                
                for (int i = 0; i < 8; i++) {
                    char c = static_cast<char>((wordData >> (i * 8)) & 0xFF);
                    if (c == 0) {
                        if (name.length() > 0) return name;
                        break;
                    }
                    name += c;
                }
            }

            if (name.length() > 0) return name;
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
    std::string jsonData = downloadJson("https://spwai.github.io/db/igns.json");
    
    if (!jsonData.empty()) {
        try {
            auto json = nlohmann::json::parse(jsonData);
            
            if (json.contains("msr") && json["msr"].is_array()) {
                g_msrPlayers.clear();
                for (const auto& player : json["msr"]) {
                    std::string playerName = player.get<std::string>();
                    std::transform(playerName.begin(), playerName.end(), playerName.begin(), ::tolower);
                    g_msrPlayers.push_back(playerName);
                }
                std::cout << "Loaded " << g_msrPlayers.size() << " MSR players" << std::endl;
            }
            
            if (json.contains("qt") && json["qt"].is_array()) {
                g_qtPlayers.clear();
                for (const auto& player : json["qt"]) {
                    std::string playerName = player.get<std::string>();
                    std::transform(playerName.begin(), playerName.end(), playerName.begin(), ::tolower);
                    g_qtPlayers.push_back(playerName);
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

bool isInList(const std::string& sanitizedName, const std::vector<std::string>& playerList) {
    return std::find(playerList.begin(), playerList.end(), sanitizedName) != playerList.end();
}

bool isSpecialName(const std::string& sanitizedName) {
    return sanitizedName == "spwai" || sanitizedName == "kyioly" || sanitizedName == "kyiolyi" || 
           sanitizedName == "kyiolyy" || sanitizedName == "kyiolys" || sanitizedName == "kyioly9" || 
           sanitizedName == "kyioly00" || sanitizedName == "karniege" || sanitizedName == "sakovaeli" ||
           sanitizedName == "ims wet fmbyy" || sanitizedName == "vzwri" || sanitizedName == "vzwra" || 
           sanitizedName == "yvmli";
}

bool checkVersionAndExit() {
    std::string jsonData = downloadJson("https://spwai.github.io/db/version.json");
    
    if (!jsonData.empty()) {
        try {
            auto json = nlohmann::json::parse(jsonData);
            
            if (json.contains("version") && json.contains("id")) {
                std::string serverVersion = json["version"].get<std::string>();
                std::string serverId = json["id"].get<std::string>();
                
                std::cout << "Server version: " << serverVersion << std::endl;
                std::cout << "Server ID: " << serverId << std::endl;
                std::cout << "Local version: " << VERSION << std::endl;
                std::cout << "Local ID: " << ID << std::endl;

                if (serverId != ID) {
                    ExitProcess(0);
                    return false;
                }
                
                std::cout << "Version check passed!" << std::endl;
                return true;
            } else {
                ExitProcess(0);
                return false;
            }
            
        } catch (const std::exception& e) {
            ExitProcess(0);
            return false;
        }
    } else {
        ExitProcess(0);
        return false;
    }
}