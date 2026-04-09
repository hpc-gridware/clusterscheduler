/*___INFO__MARK_BEGIN_NEW__*/
/***************************************************************************
 *
 *  Copyright 2026 HPC-Gridware GmbH
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *
 ***************************************************************************/
/*___INFO__MARK_END_NEW__*/

#include <algorithm>
#include <cctype>
#include <chrono>
#include <fstream>
#include <iostream>
#include <optional>
#include <string>
#include <vector>

#include <curl/curl.h>

enum class CloudProvider {
    Aws,
    Gcp,
    Azure,
    Unknown
};

static const char* to_string(CloudProvider p) {
    switch (p) {
        case CloudProvider::Aws:   return "aws";
        case CloudProvider::Gcp:   return "gcp";
        case CloudProvider::Azure: return "azure";
        default:                   return "unknown";
    }
}

static std::string read_first_line(const char* path) {
    std::ifstream in(path);
    if (!in) {
        return {};
    }
    std::string s;
    std::getline(in, s);
    return s;
}

static std::string to_lower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return s;
}

static bool contains_ci(const std::string& haystack, const std::string& needle) {
    return to_lower(haystack).find(to_lower(needle)) != std::string::npos;
}

// -----------------------------------------------------------------------------
// Mechanismus 1: DMI / SMBIOS
// -----------------------------------------------------------------------------
static CloudProvider detect_from_dmi() {
    const std::string vendor = read_first_line("/sys/class/dmi/id/sys_vendor");
    const std::string product = read_first_line("/sys/class/dmi/id/product_name");

    if (contains_ci(vendor, "amazon") || contains_ci(product, "amazon ec2")) {
        return CloudProvider::Aws;
    }
    if (contains_ci(vendor, "google") || contains_ci(product, "google compute engine")) {
        return CloudProvider::Gcp;
    }
    if (contains_ci(vendor, "microsoft")) {
        return CloudProvider::Azure;
    }

    return CloudProvider::Unknown;
}

// -----------------------------------------------------------------------------
// Mechanismus 2: libcurl / Metadata Service
// -----------------------------------------------------------------------------
#if OCS_WITH_CURL
struct HttpResult {
    long status = 0;
    std::string body;
    bool ok = false;
};

static size_t string_write(char* ptr, size_t size, size_t nmemb, void* userdata) {
    auto* out = static_cast<std::string*>(userdata);
    out->append(ptr, size * nmemb);
    return size * nmemb;
}

static HttpResult http_get(const std::string& url,
                           const std::vector<std::string>& headers = {},
                           long timeout_ms = 800) {
    HttpResult result;
    CURL* curl = curl_easy_init();
    if (!curl) {
        return result;
    }

    curl_slist* header_list = nullptr;
    for (const auto& h : headers) {
        header_list = curl_slist_append(header_list, h.c_str());
    }

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, header_list);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, timeout_ms);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT_MS, timeout_ms);
    curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
    curl_easy_setopt(curl, CURLOPT_NOPROXY, "*");
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, string_write);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &result.body);

    const CURLcode rc = curl_easy_perform(curl);
    if (rc == CURLE_OK) {
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &result.status);
        result.ok = true;
    }

    curl_slist_free_all(header_list);
    curl_easy_cleanup(curl);
    return result;
}

static std::optional<std::string> aws_imdsv2_token(long timeout_ms = 800) {
    CURL* curl = curl_easy_init();
    if (!curl) {
        return std::nullopt;
    }

    std::string body;
    curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, "X-aws-ec2-metadata-token-ttl-seconds: 60");

    curl_easy_setopt(curl, CURLOPT_URL, "http://169.254.169.254/latest/api/token");
    curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "PUT");
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, timeout_ms);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT_MS, timeout_ms);
    curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
    curl_easy_setopt(curl, CURLOPT_NOPROXY, "*");
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, string_write);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &body);

    long status = 0;
    const CURLcode rc = curl_easy_perform(curl);
    if (rc == CURLE_OK) {
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status);
    }

    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    if (rc == CURLE_OK && status == 200 && !body.empty()) {
        return body;
    }
    return std::nullopt;
}

static CloudProvider detect_from_metadata() {
    {
        const auto r = http_get(
            "http://169.254.169.254/computeMetadata/v1/instance/id",
            {"Metadata-Flavor: Google"});
        if (r.ok && r.status == 200 && !r.body.empty()) {
            return CloudProvider::Gcp;
        }
    }

    {
        const auto r = http_get(
            "http://169.254.169.254/metadata/instance?api-version=2021-02-01",
            {"Metadata: true"});
        if (r.ok && r.status == 200 && !r.body.empty()) {
            return CloudProvider::Azure;
        }
    }

    {
        const auto token = aws_imdsv2_token();
        if (token) {
            const auto r = http_get(
                "http://169.254.169.254/latest/meta-data/instance-id",
                {std::string("X-aws-ec2-metadata-token: ") + *token});
            if (r.ok && r.status == 200 && !r.body.empty()) {
                return CloudProvider::Aws;
            }
        }

        const auto r = http_get("http://169.254.169.254/latest/meta-data/instance-id");
        if (r.ok && r.status == 200 && !r.body.empty()) {
            return CloudProvider::Aws;
        }
    }

    return CloudProvider::Unknown;
}
#else
static CloudProvider detect_from_metadata() {
    return CloudProvider::Unknown;
}
#endif

// -----------------------------------------------------------------------------
// Mechanismus 3: externes curl-Tool
// -----------------------------------------------------------------------------
static int run_command_capture_success(const std::string& cmd) {
    return std::system(cmd.c_str());
}

static CloudProvider detect_from_curl_tool() {
    if (run_command_capture_success(
            "curl -fsS --noproxy '*' --max-time 1 "
            "-H 'Metadata-Flavor: Google' "
            "http://169.254.169.254/computeMetadata/v1/instance/id "
            "> /dev/null 2>&1") == 0) {
        return CloudProvider::Gcp;
    }

    if (run_command_capture_success(
            "curl -fsS --noproxy '*' --max-time 1 "
            "-H 'Metadata: true' "
            "'http://169.254.169.254/metadata/instance?api-version=2021-02-01' "
            "> /dev/null 2>&1") == 0) {
        return CloudProvider::Azure;
    }

    if (run_command_capture_success(
            "TOKEN=$(curl -fsS --noproxy '*' --max-time 1 -X PUT "
            "'http://169.254.169.254/latest/api/token' "
            "-H 'X-aws-ec2-metadata-token-ttl-seconds: 60' 2>/dev/null) && "
            "[ -n \"$TOKEN\" ] && "
            "curl -fsS --noproxy '*' --max-time 1 "
            "-H \"X-aws-ec2-metadata-token: $TOKEN\" "
            "http://169.254.169.254/latest/meta-data/instance-id "
            "> /dev/null 2>&1") == 0) {
        return CloudProvider::Aws;
    }

    if (run_command_capture_success(
            "curl -fsS --noproxy '*' --max-time 1 "
            "http://169.254.169.254/latest/meta-data/instance-id "
            "> /dev/null 2>&1") == 0) {
        return CloudProvider::Aws;
    }

    return CloudProvider::Unknown;
}

// -----------------------------------------------------------------------------
// CLI
// -----------------------------------------------------------------------------
struct Options {
    bool use_dmi = false;
    bool use_metadata = false;
    bool use_curl_tool = false;
};

static void print_usage(const char* argv0) {
    std::cerr
        << "Usage: " << argv0 << " [-dmi] [-metadata] [-curl]\n"
        << "       " << argv0 << " -all\n\n"
        << "Options:\n"
        << "  -dmi         Detect via DMI/SMBIOS files\n"
        << "  -metadata    Detect via metadata service using libcurl\n"
        << "  -curl        Detect via external curl command\n"
        << "  -all         Enable all detection mechanisms\n"
        << "  -help        Show this help\n";
}

static std::optional<Options> parse_args(int argc, char** argv) {
    Options opt;

    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "-dmi") {
            opt.use_dmi = true;
        } else if (arg == "-metadata") {
            opt.use_metadata = true;
        } else if (arg == "-curl") {
            opt.use_curl_tool = true;
        } else if (arg == "-all") {
            opt.use_dmi = true;
            opt.use_metadata = true;
            opt.use_curl_tool = true;
        } else if (arg == "-help" || arg == "-h") {
            print_usage(argv[0]);
            return std::nullopt;
        } else {
            std::cerr << "Unknown argument: " << arg << "\n";
            print_usage(argv[0]);
            return std::nullopt;
        }
    }

    if (!opt.use_dmi && !opt.use_metadata && !opt.use_curl_tool) {
        std::cerr << "No detection mechanism selected.\n";
        print_usage(argv[0]);
        return std::nullopt;
    }

    return opt;
}

int main(int argc, char** argv) {
    const auto options = parse_args(argc, argv);
    if (!options) {
        return 1;
    }

#if OCS_WITH_CURL
    curl_global_init(CURL_GLOBAL_DEFAULT);
#endif

    if (options->use_dmi) {
        std::cout << "dmi=" << to_string(detect_from_dmi()) << "\n";
    }

    if (options->use_metadata) {
        std::cout << "metadata=" << to_string(detect_from_metadata()) << "\n";
    }

    if (options->use_curl_tool) {
        std::cout << "curl=" << to_string(detect_from_curl_tool()) << "\n";
    }

#if OCS_WITH_CURL
    curl_global_cleanup();
#endif

    return 0;
}