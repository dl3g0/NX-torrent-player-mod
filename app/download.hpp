#pragma once

#include <string>
#include <vector>
#include <memory>

namespace download
{

enum class State
{
    QUEUED = 0,
    DOWNLOADING = 1,
    PAUSED = 2,
    COMPLETED = 3,
    FAILED = 4,
};

struct Task
{
    std::string id;
    std::string title;
    std::string source;
    int fileIndex = -1;
    std::string savePath;
    std::string posterId;
    std::string posterUrl;
    
    State state = State::QUEUED;
    int64_t downloadedBytes = 0;
    int64_t totalBytes = 0;
    double speed = 0.0; // bytes/sec
    std::string error;
};

void init();
void shutdown();

bool addTask(const std::string& title, const std::string& source, int fileIndex,
             const std::string& posterId, const std::string& posterUrl);

std::vector<Task> getTasks();
bool getTask(const std::string& id, Task& out);

void pauseTask(const std::string& id);
void resumeTask(const std::string& id);
void removeTask(const std::string& id, bool deleteFile = true);

int activeCount();
std::string currentActiveId();

} // namespace download
