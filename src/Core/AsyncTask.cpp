#include "AsyncTask.h"

AsyncTask::~AsyncTask() {
    if (thread.joinable()) thread.join();
}

void AsyncTask::Start(WorkFn work) {
    if (thread.joinable()) thread.join();

    running = true;
    completed = false;
    {
        std::lock_guard<std::mutex> lock(mutex);
        latestLine.clear();
        fullLog.clear();
    }

    thread = std::thread([this, work]() {
        auto reportLine = [this](const std::string& line) {
            std::lock_guard<std::mutex> lock(mutex);
            latestLine = line;
            fullLog += line;
            if (line.empty() || line.back() != '\n') fullLog += "\n";
        };

        std::string finalMsg;
        bool ok = work(reportLine, finalMsg);

        {
            std::lock_guard<std::mutex> lock(mutex);
            success = ok;
            finalMessage = finalMsg;
        }
        completed = true;
        running = false;
    });
}

bool AsyncTask::IsRunning() const {
    return running;
}

bool AsyncTask::PollCompletion(bool& outSuccess, std::string& outFinalMessage) {
    if (!completed) return false;

    {
        std::lock_guard<std::mutex> lock(mutex);
        outSuccess = success;
        outFinalMessage = finalMessage;
    }
    completed = false;
    if (thread.joinable()) thread.join();
    return true;
}

std::string AsyncTask::GetLatestLine() const {
    std::lock_guard<std::mutex> lock(mutex);
    return latestLine;
}

std::string AsyncTask::GetFullLog() const {
    std::lock_guard<std::mutex> lock(mutex);
    return fullLog;
}
