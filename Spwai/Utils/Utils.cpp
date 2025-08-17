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
    
    for (char c : tempName) {
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == ' ') {
            sanitized += c;
        }
    }
    
    return sanitized;
}

std::string extractName(void** nameTag) {
    if (!nameTag) return "";
    
    if (IsBadReadPtr(nameTag, sizeof(void*)) == FALSE && *nameTag) {
        void* unionPtr = *nameTag;
        
        char buffer[17] = { 0 };
        
        if (IsBadReadPtr(unionPtr, 16) == FALSE) {
            memcpy(buffer, unionPtr, 16);
            
            bool validInline = true;
            int validLength = 0;
            
            for (int i = 0; i < 16; i++) {
                if (buffer[i] == 0) {
                    validLength = i;
                    break;
                }
                
                unsigned char ch = buffer[i];
                if (!(isprint(ch) || ch == 0xC2 || ch == 0xA7 || ch == 0xC3 || ch == 0xBA || ch == 0xE2 || ch == 0x94)) {
                    validInline = false;
                    break;
                }
                validLength = i + 1;
            }
            
            if (validInline && validLength > 0 && validLength < 16) {
                std::string name(buffer, validLength);
                name.erase(std::remove(name.begin(), name.end(), '§'), name.end());
                
                if (!name.empty()) {
                    return name;
                }
            }
        }
        
        if (reinterpret_cast<uintptr_t>(unionPtr) > 0x10000) {
            if (IsBadReadPtr(unionPtr, 1) == FALSE) {
                const char* potentialStr = reinterpret_cast<const char*>(unionPtr);
                size_t len = strnlen(potentialStr, 1024);
                
                if (len > 0) {
                    int printableCount = 0;
                    
                    for (size_t i = 0; i < len; i++) {
                        unsigned char ch = potentialStr[i];
                        if (isprint(ch) || ch == 0xC2 || ch == 0xA7 || ch == 0xC3 || ch == 0xBA || ch == 0xE2 || ch == 0x94) {
                            printableCount++;
                        }
                    }
                    
                    if (printableCount > len / 2) {
                        std::string name(potentialStr, len);
                        name.erase(std::remove(name.begin(), name.end(), '§'), name.end());
                        
                        if (!name.empty()) {
                            return name;
                        }
                    }
                }
            }
        }
        
        if (reinterpret_cast<uintptr_t>(unionPtr) > 0x10000) {
            if (IsBadReadPtr(unionPtr, sizeof(void*)) == FALSE) {
                const char** stringPtr = reinterpret_cast<const char**>(unionPtr);
                
                if (stringPtr && *stringPtr && IsBadReadPtr(*stringPtr, 1) == FALSE) {
                    const char* potentialStr = *stringPtr;
                    size_t len = strnlen(potentialStr, 1024);
                    
                    if (len > 0) {
                        int printableCount = 0;
                        
                        for (size_t i = 0; i < len; i++) {
                            unsigned char ch = potentialStr[i];
                            if (isprint(ch) || ch == 0xC2 || ch == 0xC3 || ch == 0xBA || ch == 0xE2 || ch == 0x94) {
                                printableCount++;
                            }
                        }
                        
                        if (printableCount > len / 2) {
                            std::string name(potentialStr, len);
                            name.erase(std::remove(name.begin(), name.end(), '§'), name.end());
                            
                            if (!name.empty()) {
                                return name;
                            }
                        }
                    }
                }
            }
        }
        
        uint64_t intValue = reinterpret_cast<uint64_t>(unionPtr);
        
        if (intValue > 0x100000000 && intValue < 0x7FFFFFFFFFFFFFFF) {
            char intBuffer[9] = { 0 };
            memcpy(intBuffer, &intValue, 8);
            
            std::string intName(intBuffer, 8);
            intName.erase(std::remove(intName.begin(), intName.end(), '\0'), intName.end());
            
            if (!intName.empty() && intName.length() <= 8) {
                bool allPrintable = true;
                for (char c : intName) {
                    if (!isprint(static_cast<unsigned char>(c))) {
                        allPrintable = false;
                        break;
                    }
                }
                
                if (allPrintable) {
                    return intName;
                }
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
                for (const auto& player : json["msr"]) {
                    g_msrPlayers.push_back(player.get<std::string>());
                }
                std::cout << "Loaded " << g_msrPlayers.size() << " MSR players" << std::endl;
            }
            
            if (json.contains("qt") && json["qt"].is_array()) {
                g_qtPlayers.clear();
                for (const auto& player : json["qt"]) {
                    g_qtPlayers.push_back(player.get<std::string>());
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