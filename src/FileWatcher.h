#pragma once
#include <string>
#include <functional>
#include <thread>
#include <atomic>
#include <Windows.h>

class FileWatcher {
public:
    using Callback = std::function<void(const std::wstring&)>;
    
    FileWatcher();
    ~FileWatcher();
    
    // Imposta la callback da chiamare quando viene rilevato un nuovo PDF
    void SetCallback(Callback callback);
    
    // Avvia il monitoraggio della directory specificata
    bool Start(const std::wstring& directory);
    
    // Ferma il monitoraggio
    void Stop();
    
    // Verifica se il watcher è attivo
    bool IsRunning() const;
    
    // Restituisce l'ultimo errore
    std::wstring GetLastError() const;
    
private:
    void WatchThread();
    
    std::wstring watchDirectory;
    std::wstring lastError;
    Callback callback;
    std::thread watcherThread;
    std::atomic<bool> running;
    HANDLE stopEvent;
};
