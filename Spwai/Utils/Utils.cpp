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
    
    // Remove Minecraft color codes (§ followed by a character)
    std::string tempName = name;
    size_t pos = 0;
    
    while ((pos = tempName.find('§', pos)) != std::string::npos) {
        if (pos + 1 < tempName.length()) {
            tempName.erase(pos, 2);
        } else {
            tempName.erase(pos, 1);
        }
    }
    
    // Remove newlines (as shown in the client code)
    pos = 0;
    while ((pos = tempName.find('\n', pos)) != std::string::npos) {
        tempName.erase(pos, 1);
    }
    
    // Keep only alphanumeric characters, spaces, and common Minecraft name characters
    for (char c : tempName) {
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || 
            c == ' ' || c == '_' || c == '-') {
            sanitized += c;
        }
    }
    
    return sanitized;
}

std::string extractName(void** nameTag) {
    if (!nameTag || *nameTag == nullptr) return "";
    
    std::cout << "[DEBUG] nameTag: " << nameTag << ", *nameTag: " << *nameTag << std::endl;
    
    if (IsBadReadPtr(nameTag, sizeof(void*)) == FALSE && *nameTag) {
        uintptr_t ptrValue = reinterpret_cast<uintptr_t>(*nameTag);
        
        std::cout << "[DEBUG] ptrValue: 0x" << std::hex << ptrValue << std::dec << std::endl;
        
        // Check if it's an inline integer (not a valid pointer)
        if (ptrValue < 0x1000) {
            std::cout << "[DEBUG] ptrValue too low" << std::endl;
            return "";
        }
        
        // Check if it's an inline integer (looks like data, not a pointer)
        if (ptrValue > 0x1000000000000ULL) {
            std::cout << "[DEBUG] Detected inline integer" << std::endl;
            
            // Extract all characters from first word
            std::string name;
            for (int i = 0; i < 8; i++) {
                char c = static_cast<char>((ptrValue >> (i * 8)) & 0xFF);
                if (c == 0) break;
                // Extract all characters including § (0xA7) and other non-printable color codes
                name += c;
            }
            
            std::cout << "[DEBUG] First word name: '" << name << "'" << std::endl;
            
            // Check if there's more data in the next word
            if (IsBadReadPtr(nameTag, 16) == FALSE) {
                uint64_t* nextWord = reinterpret_cast<uint64_t*>(nameTag) + 1;
                uint64_t nextValue = *nextWord;
                std::cout << "[DEBUG] Next word: 0x" << std::hex << nextValue << std::dec << std::endl;
                
                // Extract all characters from second word
                for (int i = 0; i < 8; i++) {
                    char c = static_cast<char>((nextValue >> (i * 8)) & 0xFF);
                    if (c == 0) break;
                    // Extract all characters including § (0xA7) and other non-printable color codes
                    name += c;
                }
                
                std::cout << "[DEBUG] Combined name: '" << name << "'" << std::endl;
            }
            
            return name;
        }
        
        // If it's a valid pointer, try to read it as a string structure
        std::cout << "[DEBUG] Treating as pointer" << std::endl;
        if (IsBadReadPtr(*nameTag, 32) == FALSE) {
            uint64_t* stringStruct = reinterpret_cast<uint64_t*>(*nameTag);
            
            // Debug the entire structure
            std::cout << "[DEBUG] String structure dump:" << std::endl;
            for (int i = 0; i < 4; i++) {
                std::cout << "[DEBUG] [" << i << "]: 0x" << std::hex << stringStruct[i] << std::dec << std::endl;
            }
            
            // Extract all valid characters from the structure and find the actual length
            std::string name;
            
            // Extract characters from all three 64-bit words
            for (int word = 0; word < 3; word++) {
                uint64_t wordData = stringStruct[word];
                
                for (int i = 0; i < 8; i++) {
                    char c = static_cast<char>((wordData >> (i * 8)) & 0xFF);
                    if (c == 0) break;
                    // Extract all characters including § (0xA7) and other non-printable color codes
                    name += c;
                }
            }
            
            std::cout << "[DEBUG] Raw extracted name: '" << name << "'" << std::endl;
            
            // Return the name if we found any valid characters
            if (name.length() > 0) {
                return name;
            }
            
            std::cout << "[DEBUG] No valid layout found" << std::endl;
        }
    }
    
    std::cout << "[DEBUG] returning empty string" << std::endl;
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