#include "download.hpp"
#include "appdata.hpp"
#include "json.hpp"
#include "http.hpp"
#include "sys.hpp"

extern "C"
{
#include "torrentfs.h"
}

#include <borealis.hpp>
#include <curl/curl.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <dirent.h>
#include <unistd.h>
#include <cstdio>
#include <cstring>
#include <chrono>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>

namespace download
{

namespace
{

constexpr const char* kTasksFile = APPDATA_DIR "/downloads.json";

std::mutex g_mtx;
std::condition_variable g_cv;
std::vector<Task> g_tasks;
std::thread g_workerThread;
std::atomic<bool> g_stop{false};
std::atomic<bool> g_workerActive{false};
std::string g_currentActiveId;
std::atomic<torrentfs*> g_activeTfs{nullptr};

std::string sanitizeFilename(const std::string& name)
{
    std::string clean;
    for (char c : name)
    {
        if (c == '/' || c == '\\' || c == ':' || c == '*' || c == '?' ||
            c == '"' || c == '<' || c == '>' || c == '|' || (unsigned char)c < 32)
        {
            clean += '_';
        }
        else
        {
            clean += c;
        }
    }
    while (!clean.empty() && (clean.back() == ' ' || clean.back() == '.'))
        clean.pop_back();
    return clean.empty() ? "download" : clean;
}

std::string deriveExtension(const std::string& source, const std::string& title)
{
    std::string s = source;
    size_t q = s.find('?');
    if (q != std::string::npos) s = s.substr(0, q);
    size_t dot = s.find_last_of('.');
    if (dot != std::string::npos && dot + 1 < s.size())
    {
        std::string ext = s.substr(dot);
        if (ext == ".mkv" || ext == ".mp4" || ext == ".avi" || ext == ".ts" || ext == ".webm")
            return ext;
    }
    size_t tdot = title.find_last_of('.');
    if (tdot != std::string::npos && tdot + 1 < title.size())
    {
        std::string ext = title.substr(tdot);
        if (ext == ".mkv" || ext == ".mp4" || ext == ".avi" || ext == ".ts" || ext == ".webm")
            return ext;
    }
    return ".mkv";
}

void saveTasksLocked()
{
    FILE* f = std::fopen(kTasksFile, "wb");
    if (!f) return;

    std::string json = "[\n";
    for (size_t i = 0; i < g_tasks.size(); i++)
    {
        const auto& t = g_tasks[i];
        json += "  {\n";
        json += "    \"id\": \"" + json::escape(t.id) + "\",\n";
        json += "    \"title\": \"" + json::escape(t.title) + "\",\n";
        json += "    \"source\": \"" + json::escape(t.source) + "\",\n";
        json += "    \"fileIndex\": " + std::to_string(t.fileIndex) + ",\n";
        json += "    \"savePath\": \"" + json::escape(t.savePath) + "\",\n";
        json += "    \"posterId\": \"" + json::escape(t.posterId) + "\",\n";
        json += "    \"posterUrl\": \"" + json::escape(t.posterUrl) + "\",\n";
        json += "    \"state\": " + std::to_string((int)t.state) + ",\n";
        json += "    \"downloadedBytes\": " + std::to_string(t.downloadedBytes) + ",\n";
        json += "    \"totalBytes\": " + std::to_string(t.totalBytes) + "\n";
        json += "  }" + std::string(i + 1 < g_tasks.size() ? ",\n" : "\n");
    }
    json += "]\n";
    std::fwrite(json.data(), 1, json.size(), f);
    std::fclose(f);
}

void loadTasksLocked()
{
    g_tasks.clear();
    FILE* f = std::fopen(kTasksFile, "rb");
    if (!f) return;

    std::fseek(f, 0, SEEK_END);
    long sz = std::ftell(f);
    std::fseek(f, 0, SEEK_SET);
    if (sz <= 0) { std::fclose(f); return; }

    std::string body(sz, '\0');
    std::fread(&body[0], 1, sz, f);
    std::fclose(f);

    for (const auto& o : json::objects(body, ""))
    {
        Task t;
        t.id = json::str(o, "id");
        t.title = json::str(o, "title");
        t.source = json::str(o, "source");
        t.fileIndex = (int)json::integer(o, "fileIndex");
        t.savePath = json::str(o, "savePath");
        t.posterId = json::str(o, "posterId");
        t.posterUrl = json::str(o, "posterUrl");
        t.state = (State)json::integer(o, "state");
        t.downloadedBytes = json::integer(o, "downloadedBytes");
        t.totalBytes = json::integer(o, "totalBytes");

        if (t.state == State::DOWNLOADING)
            t.state = State::PAUSED; // resume manually on next startup

        if (!t.id.empty())
            g_tasks.push_back(t);
    }
}

struct ProgressContext
{
    std::string taskId;
    int64_t initialOffset = 0;
    int64_t lastBytes = 0;
    std::chrono::steady_clock::time_point lastTime;
};

static int xferCallback(void* clientp, curl_off_t dltotal, curl_off_t dlnow, curl_off_t, curl_off_t)
{
    if (g_stop) return 1;
    auto* ctx = static_cast<ProgressContext*>(clientp);
    if (!ctx) return 0;

    auto now = std::chrono::steady_clock::now();
    double dt = std::chrono::duration<double>(now - ctx->lastTime).count();
    int64_t delta = dlnow - ctx->lastBytes;

    if (dt < 0.25 && delta < 256 * 1024)
        return 0;

    std::lock_guard<std::mutex> lock(g_mtx);
    bool found = false;
    for (auto& t : g_tasks)
    {
        if (t.id == ctx->taskId)
        {
            found = true;
            if (t.state == State::PAUSED) return 1; // abort curl to pause

            t.downloadedBytes = ctx->initialOffset + dlnow;
            if (dltotal > 0)
                t.totalBytes = ctx->initialOffset + dltotal;

            if (dt > 0.0 && delta >= 0)
                t.speed = (double)delta / dt;

            ctx->lastBytes = dlnow;
            ctx->lastTime = now;
            break;
        }
    }
    if (!found) return 1; // task was removed: abort curl
    return 0;
}

static int sockoptCallback(void* clientp, curl_socket_t curlfd, curlsocktype purpose)
{
    if (purpose == CURLSOCKTYPE_IPCXN)
    {
        int rcvbuf = 512 * 1024;
        setsockopt(curlfd, SOL_SOCKET, SO_RCVBUF, (char*)&rcvbuf, sizeof(rcvbuf));
        int nodelay = 1;
        setsockopt(curlfd, IPPROTO_TCP, TCP_NODELAY, (char*)&nodelay, sizeof(nodelay));
    }
    return CURL_SOCKOPT_OK;
}

static size_t writeDataCallback(void* ptr, size_t size, size_t nmemb, void* stream)
{
    FILE* f = static_cast<FILE*>(stream);
    return std::fwrite(ptr, size, nmemb, f);
}

void runHttpDownload(Task& task)
{
    std::string partPath = task.savePath + ".part";
    int64_t existingSize = 0;
    struct stat st;
    if (stat(partPath.c_str(), &st) == 0 && st.st_size > 0)
    {
        existingSize = st.st_size;
    }

    FILE* out = std::fopen(partPath.c_str(), existingSize > 0 ? "ab" : "wb");
    if (!out)
    {
        std::lock_guard<std::mutex> lock(g_mtx);
        task.state = State::FAILED;
        task.error = "Cannot open file for writing on SD card";
        saveTasksLocked();
        return;
    }

    // 512KB I/O buffer to eliminate Switch SD write bottlenecks
    setvbuf(out, nullptr, _IOFBF, 512 * 1024);

    CURL* curl = curl_easy_init();
    if (!curl)
    {
        std::fclose(out);
        std::lock_guard<std::mutex> lock(g_mtx);
        task.state = State::FAILED;
        task.error = "cURL initialization failed";
        saveTasksLocked();
        return;
    }

    ProgressContext pctx;
    pctx.taskId = task.id;
    pctx.initialOffset = existingSize;
    pctx.lastBytes = 0;
    pctx.lastTime = std::chrono::steady_clock::now();

    curl_easy_setopt(curl, CURLOPT_URL, task.source.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeDataCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, out);
    curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
    curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, xferCallback);
    curl_easy_setopt(curl, CURLOPT_XFERINFODATA, &pctx);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
    curl_easy_setopt(curl, CURLOPT_USERAGENT,
                     "Mozilla/5.0 (Nintendo Switch; ShareApplet) AppleWebKit/537.36");
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 15L);
    curl_easy_setopt(curl, CURLOPT_LOW_SPEED_LIMIT, 1024L);
    curl_easy_setopt(curl, CURLOPT_LOW_SPEED_TIME, 30L);

    // High throughput optimizations for Nintendo Switch Wi-Fi
    curl_easy_setopt(curl, CURLOPT_BUFFERSIZE, 512 * 1024L);
    curl_easy_setopt(curl, CURLOPT_SOCKOPTFUNCTION, sockoptCallback);
    curl_easy_setopt(curl, CURLOPT_TCP_NODELAY, 1L);
    curl_easy_setopt(curl, CURLOPT_TCP_KEEPALIVE, 1L);
    curl_easy_setopt(curl, CURLOPT_PIPEWAIT, 1L);
    curl_easy_setopt(curl, CURLOPT_HTTP_VERSION, (long)CURL_HTTP_VERSION_1_1);
    curl_easy_setopt(curl, CURLOPT_MAXREDIRS, 10L);
    curl_easy_setopt(curl, CURLOPT_AUTOREFERER, 1L);

    if (existingSize > 0)
    {
        std::string range = std::to_string(existingSize) + "-";
        curl_easy_setopt(curl, CURLOPT_RANGE, range.c_str());
    }

    sys::preventSleep(true);
    sys::setCpuBoost(true);
    CURLcode res = curl_easy_perform(curl);
    sys::setCpuBoost(false);
    sys::preventSleep(false);

    long httpCode = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpCode);
    curl_easy_cleanup(curl);
    std::fflush(out);
    int fd = fileno(out);
    if (fd >= 0) fsync(fd);
    std::fclose(out);

    std::lock_guard<std::mutex> lock(g_mtx);
    if (g_stop) return;

    bool found = false;
    for (auto& t : g_tasks)
    {
        if (t.id == task.id)
        {
            found = true;
            t.speed = 0.0;
            if (t.state == State::PAUSED)
            {
                // paused by user
                saveTasksLocked();
                return;
            }

            if (res == CURLE_OK && (httpCode == 200 || httpCode == 206))
            {
                std::rename(partPath.c_str(), t.savePath.c_str());
                t.state = State::COMPLETED;
                t.downloadedBytes = t.totalBytes;
                brls::Logger::info("[download] task completed: {}", t.savePath);
            }
            else
            {
                t.state = State::FAILED;
                t.error = "Download interrupted (" + std::to_string(httpCode) + "): " +
                          curl_easy_strerror(res);
                brls::Logger::warning("[download] task failed: {} - {}", t.title, t.error);
            }
            saveTasksLocked();
            break;
        }
    }

    if (!found)
    {
        std::remove(partPath.c_str());
        std::remove(task.savePath.c_str());
    }
}

void runTorrentDownload(Task& task)
{
    std::string partPath = task.savePath + ".part";

    // If the final file already exists (re-download), remove it
    std::remove(task.savePath.c_str());

    // Check for a previous partial download
    int64_t existingSize = 0;
    struct stat st;
    if (stat(partPath.c_str(), &st) == 0 && st.st_size > 0)
    {
        existingSize = st.st_size;
    }

    sys::preventSleep(true);
    sys::setCpuBoost(true);

    char err[256] = { 0 };
    torrentfs_set_ram_stream(0);
    torrentfs* tfs = torrentfs_open_file(task.source.c_str(), APPDATA_DIR "/dwn_cache", task.fileIndex, err, sizeof(err));
    if (!tfs)
    {
        sys::setCpuBoost(false);
        sys::preventSleep(false);
        std::lock_guard<std::mutex> lock(g_mtx);
        task.state = State::FAILED;
        task.error = err[0] ? err : "Failed to open torrent / metadata lookup timed out";
        saveTasksLocked();
        return;
    }

    g_activeTfs.store(tfs);

    int64_t totalSize = torrentfs_size(tfs);
    {
        std::lock_guard<std::mutex> lock(g_mtx);
        for (auto& t : g_tasks)
        {
            if (t.id == task.id)
            {
                t.totalBytes = totalSize;
                break;
            }
        }
    }

    // If the existing .part file is larger than or equal to the torrent size,
    // or if totalSize changed since last attempt, start fresh.
    if (existingSize >= totalSize || existingSize < 0)
    {
        std::remove(partPath.c_str());
        existingSize = 0;
    }

    // Verify the existing partial data by reading the last chunk from torrent
    // and comparing. If it doesn't match, discard and start fresh.
    if (existingSize > 0)
    {
        constexpr size_t kVerifySize = 64 * 1024;
        int64_t verifyOff = existingSize > (int64_t)kVerifySize
                                ? existingSize - (int64_t)kVerifySize
                                : 0;
        size_t verifyLen = (size_t)(existingSize - verifyOff);

        std::vector<char> tBuf(verifyLen), fBuf(verifyLen);

        // Read from torrent at the end of the existing portion
        int64_t tRead = torrentfs_read(tfs, verifyOff, tBuf.data(), (int64_t)verifyLen);
        bool match = false;
        if (tRead == (int64_t)verifyLen)
        {
            FILE* chk = std::fopen(partPath.c_str(), "rb");
            if (chk)
            {
                if (std::fseek(chk, (long)verifyOff, SEEK_SET) == 0 &&
                    std::fread(fBuf.data(), 1, verifyLen, chk) == verifyLen)
                {
                    match = (std::memcmp(tBuf.data(), fBuf.data(), verifyLen) == 0);
                }
                std::fclose(chk);
            }
        }

        if (!match)
        {
            brls::Logger::warning("[download] P2P resume verification failed at offset {}, restarting",
                                  existingSize);
            std::remove(partPath.c_str());
            existingSize = 0;
        }
    }

    FILE* out = std::fopen(partPath.c_str(), existingSize > 0 ? "ab" : "wb");
    if (!out)
    {
        torrentfs* toClose = g_activeTfs.exchange(nullptr);
        if (toClose) torrentfs_close(toClose);
        sys::setCpuBoost(false);
        sys::preventSleep(false);
        std::lock_guard<std::mutex> lock(g_mtx);
        task.state = State::FAILED;
        task.error = "Cannot open file on SD card for writing";
        saveTasksLocked();
        return;
    }

    setvbuf(out, nullptr, _IOFBF, 512 * 1024);

    int64_t offset = existingSize;
    constexpr size_t kChunkSize = 256 * 1024;
    std::vector<char> buffer(kChunkSize);

    auto lastTime = std::chrono::steady_clock::now();
    int64_t lastBytes = offset;

    bool errorOccurred = false;
    std::string failReason;

    while (offset < totalSize && !g_stop)
    {
        // Check if task paused or removed
        bool pausedOrRemoved = false;
        {
            std::lock_guard<std::mutex> lock(g_mtx);
            bool found = false;
            for (const auto& t : g_tasks)
            {
                if (t.id == task.id)
                {
                    found = true;
                    if (t.state == State::PAUSED || t.state == State::FAILED)
                        pausedOrRemoved = true;
                    break;
                }
            }
            if (!found) pausedOrRemoved = true;
        }
        if (pausedOrRemoved) break;

        int64_t toRead = totalSize - offset;
        if (toRead > (int64_t)kChunkSize) toRead = kChunkSize;

        int64_t n = torrentfs_read(tfs, offset, buffer.data(), toRead);
        if (n <= 0)
        {
            if (g_stop) break;
            // Retry once after a short wait (peers may reconnect)
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            n = torrentfs_read(tfs, offset, buffer.data(), toRead);
            if (n <= 0)
            {
                errorOccurred = true;
                failReason = "P2P read interrupted or peer connection lost";
                break;
            }
        }

        size_t written = std::fwrite(buffer.data(), 1, (size_t)n, out);
        if (written != (size_t)n)
        {
            errorOccurred = true;
            failReason = "SD card write error or storage full";
            break;
        }

        offset += n;

        auto now = std::chrono::steady_clock::now();
        double dt = std::chrono::duration<double>(now - lastTime).count();
        if (dt >= 0.5)
        {
            double speed = (double)(offset - lastBytes) / dt;
            lastBytes = offset;
            lastTime = now;

            std::lock_guard<std::mutex> lock(g_mtx);
            for (auto& t : g_tasks)
            {
                if (t.id == task.id)
                {
                    t.downloadedBytes = offset;
                    t.speed = speed;
                    break;
                }
            }
        }
    }

    // Flush all buffered data and sync to SD card
    std::fflush(out);
    int fd = fileno(out);
    if (fd >= 0) fsync(fd);
    std::fclose(out);

    torrentfs* toClose = g_activeTfs.exchange(nullptr);
    if (toClose)
        torrentfs_close(toClose);

    sys::setCpuBoost(false);
    sys::preventSleep(false);

    std::lock_guard<std::mutex> lock(g_mtx);
    if (g_stop) return;

    bool found = false;
    for (auto& t : g_tasks)
    {
        if (t.id == task.id)
        {
            found = true;
            t.speed = 0.0;
            if (t.state == State::PAUSED)
            {
                t.downloadedBytes = offset;
                saveTasksLocked();
                return;
            }

            // Verify the file on disk is actually the right size
            struct stat finalSt;
            bool sizeOk = (stat(partPath.c_str(), &finalSt) == 0 &&
                           finalSt.st_size == totalSize);

            if (offset >= totalSize && !errorOccurred && sizeOk)
            {
                std::rename(partPath.c_str(), t.savePath.c_str());
                t.state = State::COMPLETED;
                t.downloadedBytes = totalSize;
                brls::Logger::info("[download] P2P torrent completed: {} ({} bytes verified)",
                                   t.savePath, totalSize);
            }
            else
            {
                t.state = State::FAILED;
                if (errorOccurred)
                    t.error = failReason;
                else if (!sizeOk)
                    t.error = "File size mismatch after download (" +
                              std::to_string(finalSt.st_size) + " vs " +
                              std::to_string(totalSize) + ")";
                else
                    t.error = "Download incomplete";
                brls::Logger::warning("[download] P2P task failed: {} - {}", t.title, t.error);
            }
            saveTasksLocked();
            break;
        }
    }

    if (!found)
    {
        std::remove(partPath.c_str());
        std::remove(task.savePath.c_str());
    }
}

void workerLoop()
{
    g_workerActive = true;
    while (!g_stop)
    {
        Task current;
        bool found = false;

        {
            std::unique_lock<std::mutex> lock(g_mtx);
            g_cv.wait(lock, []() {
                if (g_stop) return true;
                for (const auto& t : g_tasks)
                {
                    if (t.state == State::DOWNLOADING) return true;
                }
                return false;
            });

            if (g_stop) break;

            for (auto& t : g_tasks)
            {
                if (t.state == State::DOWNLOADING)
                {
                    current = t;
                    g_currentActiveId = t.id;
                    found = true;
                    break;
                }
            }
        }

        if (found)
        {
            bool isHttp = (current.source.rfind("http://", 0) == 0 || current.source.rfind("https://", 0) == 0);
            if (isHttp)
                runHttpDownload(current);
            else
                runTorrentDownload(current);

            {
                std::lock_guard<std::mutex> lock(g_mtx);
                g_currentActiveId.clear();
            }
        }
    }
    g_workerActive = false;
}

} // namespace

void init()
{
    mkdir(APPDATA_DIR, 0777);
    mkdir(APPDATA_DOWNLOADS, 0777);

    {
        std::lock_guard<std::mutex> lock(g_mtx);
        loadTasksLocked();
    }

    g_stop = false;
    g_workerThread = std::thread(workerLoop);
}

void shutdown()
{
    g_stop = true;
    g_cv.notify_all();
    if (g_workerThread.joinable())
        g_workerThread.join();
}

bool addTask(const std::string& title, const std::string& source, int fileIndex,
             const std::string& posterId, const std::string& posterUrl)
{
    std::lock_guard<std::mutex> lock(g_mtx);
    std::string cleanName = sanitizeFilename(title);
    std::string ext = deriveExtension(source, title);

    // Deduplicate only if exact same source URL/magnet is already queued
    for (auto& t : g_tasks)
    {
        if (t.source == source)
        {
            if (t.state != State::DOWNLOADING && t.state != State::COMPLETED)
            {
                t.state = State::DOWNLOADING;
                saveTasksLocked();
                g_cv.notify_one();
            }
            return true;
        }
    }

    // Generate unique savePath to avoid colliding with other downloads of the same title
    std::string baseFile = std::string(APPDATA_DOWNLOADS) + "/" + cleanName;
    std::string saveFile = baseFile + ext;
    int suffix = 1;
    bool collision = true;
    while (collision)
    {
        collision = false;
        for (const auto& t : g_tasks)
        {
            if (t.savePath == saveFile)
            {
                collision = true;
                saveFile = baseFile + " (" + std::to_string(suffix++) + ")" + ext;
                break;
            }
        }
    }

    Task t;
    t.id = std::to_string(std::chrono::system_clock::now().time_since_epoch().count());
    t.title = (suffix > 1) ? (title + " (" + std::to_string(suffix - 1) + ")") : title;
    t.source = source;
    t.fileIndex = fileIndex;
    t.savePath = saveFile;
    t.posterId = posterId;
    t.posterUrl = posterUrl;
    t.state = State::DOWNLOADING;
    t.downloadedBytes = 0;
    t.totalBytes = 0;
    t.speed = 0.0;

    g_tasks.push_back(t);
    saveTasksLocked();
    g_cv.notify_one();
    brls::Logger::info("[download] added task: {} -> {}", t.title, t.savePath);
    return true;
}

std::vector<Task> getTasks()
{
    std::lock_guard<std::mutex> lock(g_mtx);
    return g_tasks;
}

bool getTask(const std::string& id, Task& out)
{
    std::lock_guard<std::mutex> lock(g_mtx);
    for (const auto& t : g_tasks)
    {
        if (t.id == id)
        {
            out = t;
            return true;
        }
    }
    return false;
}

void pauseTask(const std::string& id)
{
    std::lock_guard<std::mutex> lock(g_mtx);
    for (auto& t : g_tasks)
    {
        if (t.id == id)
        {
            t.state = State::PAUSED;
            t.speed = 0.0;
            if (g_currentActiveId == id && g_activeTfs.load())
            {
                torrentfs_cancel(g_activeTfs.load());
            }
            saveTasksLocked();
            break;
        }
    }
}

void resumeTask(const std::string& id)
{
    std::lock_guard<std::mutex> lock(g_mtx);
    for (auto& t : g_tasks)
    {
        if (t.id == id)
        {
            t.state = State::DOWNLOADING;
            saveTasksLocked();
            g_cv.notify_one();
            break;
        }
    }
}

void removeTask(const std::string& id, bool deleteFile)
{
    std::lock_guard<std::mutex> lock(g_mtx);
    for (auto it = g_tasks.begin(); it != g_tasks.end(); ++it)
    {
        if (it->id == id)
        {
            std::string savePath = it->savePath;
            bool isActive = (g_currentActiveId == id);
            if (isActive && g_activeTfs.load())
            {
                torrentfs_cancel(g_activeTfs.load());
            }

            g_tasks.erase(it);
            saveTasksLocked();

            if (deleteFile && !isActive)
            {
                std::remove(savePath.c_str());
                std::string part = savePath + ".part";
                std::remove(part.c_str());
            }
            break;
        }
    }
}

int activeCount()
{
    std::lock_guard<std::mutex> lock(g_mtx);
    int count = 0;
    for (const auto& t : g_tasks)
    {
        if (t.state == State::DOWNLOADING)
            count++;
    }
    return count;
}

std::string currentActiveId()
{
    std::lock_guard<std::mutex> lock(g_mtx);
    return g_currentActiveId;
}

} // namespace download
