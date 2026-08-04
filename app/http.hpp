#pragma once

#include <functional>
#include <string>

// The app's HTTP calls. Blocking: every caller here already runs them off the
// UI thread (brls::async) and syncs the result back.
//
// TLS certificates ARE verified, against a CA store the app ships itself at
// romfs:/cacert.pem -- the console mounts none of its own. A rejected chain on
// device usually means the clock, not an attacker: see verifyTls/tlsAwareErr in
// http.cpp. The tracker announces in engine/torrent.c deliberately stay
// unverified; the comment there says why.
namespace http
{

// GET `url` into `resp`. False on failure, with a readable reason in `err`.
// `accept` is an optional full header line ("Accept: image/jpeg").
bool get(const std::string& url, std::string& resp, std::string& err,
         const char* accept = nullptr);

// POST `body` as application/json.
bool postJson(const char* url, const std::string& body, std::string& resp,
              std::string& err);

// GET `url` straight to `path`, so a large file never sits in RAM. `progress`
// is called from the transfer thread with (bytes so far, total or 0 if the
// server did not say); returning false from it aborts the download. The file is
// removed on failure, so a half-written one is never left behind.
bool download(const std::string& url, const std::string& path, std::string& err,
              std::function<bool(int64_t, int64_t)> progress = nullptr);

// Percent-encodes a URL path segment.
std::string urlEncode(const std::string& s);

} // namespace http
