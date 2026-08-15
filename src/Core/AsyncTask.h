#pragma once
#include <string>
#include <functional>
#include <thread>
#include <mutex>
#include <atomic>

class AsyncTask {
public:
    ~AsyncTask();

    using WorkFn = std::function<bool(const std::function<void(const std::string&)>& reportLine,
                                       std::string& outFinalMessage)>;

    void Start(WorkFn work);
    bool IsRunning() const;
    bool PollCompletion(bool& outSuccess, std::string& outFinalMessage);

    std::string GetLatestLine() const;
    std::string GetFullLog() const;

private:
    std::thread thread;
    mutable std::mutex mutex;
    std::string latestLine;
    std::string fullLog;
    std::atomic<bool> running{false};
    std::atomic<bool> completed{false};
    bool success = false;
    std::string finalMessage;
};
