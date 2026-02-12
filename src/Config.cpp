#include "Config.h"
#include <fstream>
#include <shlobj.h>

namespace Config {

std::wstring GetExecutableDir() {
    wchar_t path[MAX_PATH];
    GetModuleFileNameW(NULL, path, MAX_PATH);
    std::wstring fullPath(path);
    size_t pos = fullPath.find_last_of(L"\\/");
    return fullPath.substr(0, pos);
}

bool LoadConfig() {
    std::wstring configPath = GetExecutableDir() + L"\\" + CONFIG_FILE;
    std::wifstream file(configPath);
    
    if (!file.is_open()) {
        return false;
    }
    
    std::wstring line;
    while (std::getline(file, line)) {
        size_t pos = line.find(L'=');
        if (pos != std::wstring::npos) {
            std::wstring key = line.substr(0, pos);
            std::wstring value = line.substr(pos + 1);
            
            if (key == L"WatchDirectory") {
                watchDirectory = value;
            }
            else if (key == L"OutputDirectory") {
                outputDirectory = value;
            }
            else if (key == L"PdfToTextPath") {
                pdftotextPath = value;
            }
            else if (key == L"ClaudeEnabled") {
                claudeEnabled = (value == L"1");
            }
            else if (key == L"ClaudeTimeoutMs") {
                try { claudeTimeoutMs = std::stoul(value); } catch (...) {}
            }
        }
    }
    
    file.close();
    return !watchDirectory.empty();
}

bool SaveConfig() {
    std::wstring configPath = GetExecutableDir() + L"\\" + CONFIG_FILE;
    std::wofstream file(configPath);
    
    if (!file.is_open()) {
        return false;
    }
    
    file << L"WatchDirectory=" << watchDirectory << std::endl;
    file << L"OutputDirectory=" << outputDirectory << std::endl;
    file << L"PdfToTextPath=" << pdftotextPath << std::endl;
    file << L"ClaudeEnabled=" << (claudeEnabled ? L"1" : L"0") << std::endl;
    file << L"ClaudeTimeoutMs=" << claudeTimeoutMs << std::endl;

    file.close();
    return true;
}

bool SetAutoStart(bool enable) {
    HKEY hKey;
    LONG result = RegOpenKeyExW(
        HKEY_CURRENT_USER,
        L"Software\\Microsoft\\Windows\\CurrentVersion\\Run",
        0,
        KEY_SET_VALUE,
        &hKey
    );
    
    if (result != ERROR_SUCCESS) {
        return false;
    }
    
    if (enable) {
        wchar_t exePath[MAX_PATH];
        GetModuleFileNameW(NULL, exePath, MAX_PATH);
        
        result = RegSetValueExW(
            hKey,
            APP_NAME,
            0,
            REG_SZ,
            (BYTE*)exePath,
            (DWORD)((wcslen(exePath) + 1) * sizeof(wchar_t))
        );
    }
    else {
        result = RegDeleteValueW(hKey, APP_NAME);
    }
    
    RegCloseKey(hKey);
    return result == ERROR_SUCCESS;
}

bool IsAutoStartEnabled() {
    HKEY hKey;
    LONG result = RegOpenKeyExW(
        HKEY_CURRENT_USER,
        L"Software\\Microsoft\\Windows\\CurrentVersion\\Run",
        0,
        KEY_QUERY_VALUE,
        &hKey
    );
    
    if (result != ERROR_SUCCESS) {
        return false;
    }
    
    result = RegQueryValueExW(hKey, APP_NAME, NULL, NULL, NULL, NULL);
    RegCloseKey(hKey);
    
    return result == ERROR_SUCCESS;
}

} // namespace Config
