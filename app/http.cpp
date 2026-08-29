#include "http.hpp"

#include <curl/curl.h>

#include <cstdio>

#include <borealis.hpp>

namespace http
{

static size_t writeCb(char* ptr, size_t size, size_t nmemb, void* userdata)
{
    ((std::string*)userdata)->append(ptr, size * nmemb);
    return size * nmemb;
}

// Turn certificate checking on, against the CA store we ship ourselves. The
// console mounts none, so the roots ride along in the nro's romfs as
// cacert.pem (Mozilla's, from curl.se/ca -- see the CMakeLists copy).
//
// It has to be a *file*: curl here is 7.69 with the mbedTLS backend, so
// CURLOPT_CAINFO_BLOB (7.77+) does not exist. romfs goes through newlib's
// devoptab, so mbedtls_x509_crt_parse_file fopen()s it like any other path.
// It is re-parsed on every handshake -- an ASN.1 walk over ~120 roots, no
// signature maths, which is small next to the handshake it precedes.
static void verifyTls(CURL* curl)
{
#ifdef __SWITCH__
    curl_easy_setopt(curl, CURLOPT_CAINFO, "romfs:/cacert.pem");
#endif
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2L);
}

// curl's own words for a rejected chain ("SSL peer certificate ... not OK") send
// people looking for a network fault. On a console the overwhelmingly likely
// cause is the clock: a certificate is only valid between two dates, and a
// Switch that lost its RTC charge boots in 2000 and fails every host at once.
static std::string tlsAwareErr(CURLcode rc, const char* errbuf)
{
    if (rc == CURLE_PEER_FAILED_VERIFICATION)
        return "could not verify the site's certificate -- check the console's "
               "date and time, a wrong clock rejects every site";
    // Not a network fault at all: the bundle is missing from the romfs, so the
    // nro was built without the CMakeLists copy of assets/cacert.pem.
    if (rc == CURLE_SSL_CACERT_BADFILE)
        return "the bundled certificate store is missing or unreadable";
    return errbuf[0] ? errbuf : curl_easy_strerror(rc);
}

// Shared POST-JSON helper. Returns the body, or sets err.
bool postJson(const char* url, const std::string& body, std::string& resp,
              std::string& err)
{
    CURL* curl = curl_easy_init();
    if (!curl)
    {
        err = "cannot initialise the network";
        return false;
    }

    char errbuf[CURL_ERROR_SIZE] = { 0 };
    struct curl_slist* hdrs      = nullptr;
    hdrs = curl_slist_append(hdrs, "Content-Type: application/json");

    curl_easy_setopt(curl, CURLOPT_URL, url);
    // GitHub answers 403 to a request with no User-Agent.
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "NX-torrent-player");
    curl_easy_setopt(curl, CURLOPT_ACCEPT_ENCODING, "");
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, hdrs);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &resp);
    curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, errbuf);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 25L);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    verifyTls(curl);  // the Stremio password goes through here

    CURLcode rc = curl_easy_perform(curl);
    curl_slist_free_all(hdrs);
    curl_easy_cleanup(curl);

    if (rc != CURLE_OK)
    {
        err = tlsAwareErr(rc, errbuf);
        return false;
    }
    return true;
}

// Addon resources are plain GETs off the addon's own host. `accept` is opt-in:
// image hosts content-negotiate, and with no Accept header at all they are free
// to answer with WebP -- which stb_image (nanovg's decoder) cannot read, so the
// poster silently never appeared.
bool get(const std::string& url, std::string& resp, std::string& err,
         const char* accept)
{
    CURL* curl = curl_easy_init();
    if (!curl)
    {
        err = "cannot initialise the network";
        return false;
    }
    char errbuf[CURL_ERROR_SIZE] = { 0 };
    struct curl_slist* hdrs      = nullptr;
    if (accept)
    {
        hdrs = curl_slist_append(hdrs, accept);
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, hdrs);
    }
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    // GitHub answers 403 to a request with no User-Agent.
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "NX-torrent-player");
    curl_easy_setopt(curl, CURLOPT_ACCEPT_ENCODING, "");
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &resp);
    curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, errbuf);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 25L);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    verifyTls(curl);
    CURLcode rc = curl_easy_perform(curl);
    if (hdrs) curl_slist_free_all(hdrs);
    curl_easy_cleanup(curl);
    if (rc != CURLE_OK)
    {
        err = tlsAwareErr(rc, errbuf);
        return false;
    }
    return true;
}

// Percent-encodes an id for use in a URL path. Episode ids carry ':' separators
// ("tt123:1:3") which some addon hosts reject unencoded.
// Streams the body straight to disk: an .nro is tens of MB, and holding that in
// RAM next to a running mpv is asking for trouble.
struct DlCtx
{
    FILE* f = nullptr;
    std::function<bool(int64_t, int64_t)> progress;
    bool aborted = false;
};

static size_t fileWriteCb(char* ptr, size_t size, size_t nmemb, void* userdata)
{
    auto* c = (DlCtx*)userdata;
    return std::fwrite(ptr, size, nmemb, c->f);
}

static int progressCb(void* userdata, curl_off_t total, curl_off_t now,
                      curl_off_t, curl_off_t)
{
    auto* c = (DlCtx*)userdata;
    if (!c->progress) return 0;
    if (!c->progress((int64_t)now, (int64_t)total))
    {
        c->aborted = true;
        return 1;  // non-zero aborts the transfer
    }
    return 0;
}

bool download(const std::string& url, const std::string& path, std::string& err,
              std::function<bool(int64_t, int64_t)> progress)
{
    DlCtx ctx;
    ctx.progress = std::move(progress);
    ctx.f        = std::fopen(path.c_str(), "wb");
    if (!ctx.f)
    {
        err = "cannot write " + path;
        return false;
    }

    CURL* curl = curl_easy_init();
    if (!curl)
    {
        std::fclose(ctx.f);
        std::remove(path.c_str());
        err = "cannot initialise the network";
        return false;
    }

    char errbuf[CURL_ERROR_SIZE] = { 0 };
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    // GitHub answers 403 to a request with no User-Agent.
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "NX-torrent-player");
    curl_easy_setopt(curl, CURLOPT_ACCEPT_ENCODING, "");
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, fileWriteCb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &ctx);
    curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, errbuf);
    // No CURLOPT_TIMEOUT here, unlike the other calls: this transfer is tens of
    // MB over hotel wifi. LOW_SPEED_* kills it when it actually stalls instead.
    curl_easy_setopt(curl, CURLOPT_LOW_SPEED_LIMIT, 1024L);
    curl_easy_setopt(curl, CURLOPT_LOW_SPEED_TIME, 30L);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);  // GitHub redirects to a CDN
    verifyTls(curl);  // this one lands an executable nro on the SD card
    curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
    curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, progressCb);
    curl_easy_setopt(curl, CURLOPT_XFERINFODATA, &ctx);

    CURLcode rc = curl_easy_perform(curl);
    long code   = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &code);
    curl_easy_cleanup(curl);
    std::fclose(ctx.f);

    if (rc != CURLE_OK)
    {
        std::remove(path.c_str());
        err = ctx.aborted ? "cancelled" : tlsAwareErr(rc, errbuf);
        return false;
    }
    // A 404 is a perfectly successful transfer of an error page, and would land
    // on disk as a "download" -- check what we were actually served.
    if (code >= 400)
    {
        std::remove(path.c_str());
        err = "HTTP " + std::to_string(code);
        return false;
    }
    return true;
}

std::string urlEncode(const std::string& s)
{
    static const char* hex = "0123456789ABCDEF";
    std::string out;
    for (unsigned char c : s)
    {
        if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~')
            out += (char)c;
        else
        {
            out += '%';
            out += hex[c >> 4];
            out += hex[c & 0xF];
        }
    }
    return out;
}

std::string resolveRedirect(const std::string& url)
{
    if (url.rfind("http://", 0) != 0 && url.rfind("https://", 0) != 0)
        return url;

    CURL* curl = curl_easy_init();
    if (!curl) return url;

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_MAXREDIRS, 5L);
    curl_easy_setopt(curl, CURLOPT_NOBODY, 1L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 3L);
    curl_easy_setopt(curl, CURLOPT_USERAGENT,
                     "Mozilla/5.0 (Nintendo Switch; ShareApplet) AppleWebKit/537.36");

    CURLcode rc = curl_easy_perform(curl);
    std::string finalUrl = url;
    if (rc == CURLE_OK)
    {
        char* eff = nullptr;
        if (curl_easy_getinfo(curl, CURLINFO_EFFECTIVE_URL, &eff) == CURLE_OK && eff && eff[0])
            finalUrl = eff;
    }

    curl_easy_cleanup(curl);
    brls::Logger::info("[http] resolveRedirect: {} -> {}", url, finalUrl);
    return finalUrl;
}

} // namespace http
