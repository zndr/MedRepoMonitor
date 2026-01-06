#include "FileWatcher.h"
#include <algorithm>
#include <filesystem>
#include <map>
#include <chrono>
#include <mutex>

// Mappa per tenere traccia dei file già processati (path -> timestamp)
static std::map<std::wstring, std::chrono::steady_clock::time_point> processedFiles;
static std::mutex processedFilesMutex;
static const int DEDUP_SECONDS = 300; // Ignora stesso file per 5 minuti

FileWatcher::FileWatcher() : running(false), stopEvent(NULL) {
}

FileWatcher::~FileWatcher() {
    Stop();
}

void FileWatcher::SetCallback(Callback cb) {
    callback = cb;
}

bool FileWatcher::Start(const std::wstring& directory) {
    if (running) {
        return true;
    }
    
    if (!std::filesystem::exists(directory) || !std::filesystem::is_directory(directory)) {
        lastError = L"Directory non valida: " + directory;
        return false;
    }
    
    watchDirectory = directory;
    stopEvent = CreateEventW(NULL, TRUE, FALSE, NULL);
    
    if (!stopEvent) {
        lastError = L"Impossibile creare l'evento di stop";
        return false;
    }
    
    running = true;
    watcherThread = std::thread(&FileWatcher::WatchThread, this);
    
    return true;
}

void FileWatcher::Stop() {
    if (!running) {
        return;
    }
    
    running = false;
    
    if (stopEvent) {
        SetEvent(stopEvent);
    }
    
    if (watcherThread.joinable()) {
        watcherThread.join();
    }
    
    if (stopEvent) {
        CloseHandle(stopEvent);
        stopEvent = NULL;
    }
}

bool FileWatcher::IsRunning() const {
    return running;
}

std::wstring FileWatcher::GetLastError() const {
    return lastError;
}

void FileWatcher::WatchThread() {
    HANDLE hDir = CreateFileW(
        watchDirectory.c_str(),
        FILE_LIST_DIRECTORY,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        NULL,
        OPEN_EXISTING,
        FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED,
        NULL
    );
    
    if (hDir == INVALID_HANDLE_VALUE) {
        lastError = L"Impossibile aprire la directory per il monitoraggio";
        running = false;
        return;
    }
    
    OVERLAPPED overlapped = { 0 };
    overlapped.hEvent = CreateEventW(NULL, TRUE, FALSE, NULL);
    
    if (!overlapped.hEvent) {
        CloseHandle(hDir);
        lastError = L"Impossibile creare l'evento overlapped";
        running = false;
        return;
    }
    
    const DWORD bufferSize = 4096;
    std::vector<BYTE> buffer(bufferSize);
    
    while (running) {
        ResetEvent(overlapped.hEvent);
        
        BOOL success = ReadDirectoryChangesW(
            hDir,
            buffer.data(),
            bufferSize,
            FALSE, // Non monitorare sottodirectory
            FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_LAST_WRITE,
            NULL,
            &overlapped,
            NULL
        );
        
        if (!success) {
            break;
        }
        
        // Attendi evento o stop
        HANDLE handles[] = { overlapped.hEvent, stopEvent };
        DWORD waitResult = WaitForMultipleObjects(2, handles, FALSE, INFINITE);
        
        if (waitResult == WAIT_OBJECT_0 + 1) {
            // Stop richiesto
            CancelIo(hDir);
            break;
        }
        
        if (waitResult != WAIT_OBJECT_0) {
            continue;
        }
        
        DWORD bytesReturned;
        if (!GetOverlappedResult(hDir, &overlapped, &bytesReturned, FALSE)) {
            continue;
        }
        
        // Processa i risultati
        FILE_NOTIFY_INFORMATION* pNotify = reinterpret_cast<FILE_NOTIFY_INFORMATION*>(buffer.data());
        
        do {
            if (pNotify->Action == FILE_ACTION_ADDED || 
                pNotify->Action == FILE_ACTION_MODIFIED) {
                
                std::wstring fileName(pNotify->FileName, pNotify->FileNameLength / sizeof(wchar_t));
                
                // Verifica se è un file PDF
                std::wstring ext = fileName.substr(fileName.find_last_of(L'.') + 1);
                std::transform(ext.begin(), ext.end(), ext.begin(), ::towlower);
                
                if (ext == L"pdf") {
                    std::wstring fullPath = watchDirectory + L"\\" + fileName;
                    auto now = std::chrono::steady_clock::now();
                    bool skipFile = false;
                    
                    // Lock per thread safety
                    {
                        std::lock_guard<std::mutex> lock(processedFilesMutex);
                        
                        // Controlla se il file è stato processato di recente
                        auto it = processedFiles.find(fullPath);
                        if (it != processedFiles.end()) {
                            auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - it->second).count();
                            if (elapsed < DEDUP_SECONDS) {
                                skipFile = true;
                            }
                        }
                        
                        // Registra SUBITO il file come in elaborazione (PRIMA della callback)
                        if (!skipFile) {
                            processedFiles[fullPath] = now;
                            
                            // Pulisci vecchie entry (più di 10 minuti)
                            for (auto iter = processedFiles.begin(); iter != processedFiles.end(); ) {
                                auto age = std::chrono::duration_cast<std::chrono::seconds>(now - iter->second).count();
                                if (age > 600) {
                                    iter = processedFiles.erase(iter);
                                } else {
                                    ++iter;
                                }
                            }
                        }
                    }
                    
                    if (!skipFile) {
                        // Attendi che il file sia completamente scritto
                        Sleep(1000);
                        
                        // Verifica che il file sia accessibile
                        HANDLE hFile = CreateFileW(
                            fullPath.c_str(),
                            GENERIC_READ,
                            FILE_SHARE_READ,
                            NULL,
                            OPEN_EXISTING,
                            FILE_ATTRIBUTE_NORMAL,
                            NULL
                        );
                        
                        if (hFile != INVALID_HANDLE_VALUE) {
                            CloseHandle(hFile);
                            
                            if (callback) {
                                callback(fullPath);
                            }
                        }
                    }
                }
            }
            
            if (pNotify->NextEntryOffset == 0) {
                break;
            }
            
            pNotify = reinterpret_cast<FILE_NOTIFY_INFORMATION*>(
                reinterpret_cast<BYTE*>(pNotify) + pNotify->NextEntryOffset
            );
            
        } while (true);
    }
    
    CloseHandle(overlapped.hEvent);
    CloseHandle(hDir);
}
