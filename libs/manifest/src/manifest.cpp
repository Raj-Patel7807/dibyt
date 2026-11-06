#include "dibyt/manifest/manifest.hpp"
#include "dibyt/core/version.hpp"
#include <algorithm>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <chrono>

namespace dibyt::manifest {

namespace {

// Clean helper converting bytes to 64-character lowercase hex string
std::string to_hex_string(const uint8_t* data, size_t length) {
    std::ostringstream oss;
    for (size_t i = 0; i < length; ++i) {
        oss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(data[i]);
    }
    return oss.str();
}

// Portable SHA-256 implementation (FIPS PUB 180-4 standard) for zero-dependency standalone builds
class Sha256Ctx {
public:
    Sha256Ctx() { reset(); }

    void reset() {
        datalen_ = 0;
        bitlen_ = 0;
        state_[0] = 0x6a09e667;
        state_[1] = 0xbb67ae85;
        state_[2] = 0x3c6ef372;
        state_[3] = 0xa54ff53a;
        state_[4] = 0x510e527f;
        state_[5] = 0x9b05688c;
        state_[6] = 0x1f83d9ab;
        state_[7] = 0x5be0cd19;
    }

    void update(const uint8_t* data, size_t len) {
        for (size_t i = 0; i < len; ++i) {
            data_[datalen_] = data[i];
            datalen_++;
            if (datalen_ == 64) {
                transform();
                bitlen_ += 512;
                datalen_ = 0;
            }
        }
    }

    void final(uint8_t hash[32]) {
        size_t i = datalen_;
        if (datalen_ < 56) {
            data_[i++] = 0x80;
            while (i < 56) data_[i++] = 0x00;
        } else {
            data_[i++] = 0x80;
            while (i < 64) data_[i++] = 0x00;
            transform();
            for (size_t j = 0; j < 56; ++j) data_[j] = 0;
        }
        bitlen_ += datalen_ * 8;
        data_[63] = static_cast<uint8_t>(bitlen_);
        data_[62] = static_cast<uint8_t>(bitlen_ >> 8);
        data_[61] = static_cast<uint8_t>(bitlen_ >> 16);
        data_[60] = static_cast<uint8_t>(bitlen_ >> 24);
        data_[59] = static_cast<uint8_t>(bitlen_ >> 32);
        data_[58] = static_cast<uint8_t>(bitlen_ >> 40);
        data_[57] = static_cast<uint8_t>(bitlen_ >> 48);
        data_[56] = static_cast<uint8_t>(bitlen_ >> 56);
        transform();
        for (i = 0; i < 4; ++i) {
            for (size_t j = 0; j < 8; ++j) {
                hash[j * 4 + i] = static_cast<uint8_t>((state_[j] >> (24 - i * 8)) & 0x000000ff);
            }
        }
    }

private:
    static inline uint32_t rotl(uint32_t a, uint32_t b) { return (a << b) | (a >> (32 - b)); }
    static inline uint32_t rotr(uint32_t a, uint32_t b) { return (a >> b) | (a << (32 - b)); }
    static inline uint32_t ch(uint32_t x, uint32_t y, uint32_t z) { return (x & y) ^ (~x & z); }
    static inline uint32_t maj(uint32_t x, uint32_t y, uint32_t z) { return (x & y) ^ (x & z) ^ (y & z); }
    static inline uint32_t ep0(uint32_t x) { return rotr(x, 2) ^ rotr(x, 13) ^ rotr(x, 22); }
    static inline uint32_t ep1(uint32_t x) { return rotr(x, 6) ^ rotr(x, 11) ^ rotr(x, 25); }
    static inline uint32_t sig0(uint32_t x) { return rotr(x, 7) ^ rotr(x, 18) ^ (x >> 3); }
    static inline uint32_t sig1(uint32_t x) { return rotr(x, 17) ^ rotr(x, 19) ^ (x >> 10); }

    void transform() {
        static const uint32_t k[64] = {
            0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,
            0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
            0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
            0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
            0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,
            0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
            0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,
            0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2
        };

        uint32_t m[64];
        for (size_t i = 0, j = 0; i < 16; ++i, j += 4) {
            m[i] = (static_cast<uint32_t>(data_[j]) << 24) |
                   (static_cast<uint32_t>(data_[j + 1]) << 16) |
                   (static_cast<uint32_t>(data_[j + 2]) << 8) |
                   (static_cast<uint32_t>(data_[j + 3]));
        }
        for (size_t i = 16; i < 64; ++i) {
            m[i] = sig1(m[i - 2]) + m[i - 7] + sig0(m[i - 15]) + m[i - 16];
        }

        uint32_t a = state_[0];
        uint32_t b = state_[1];
        uint32_t c = state_[2];
        uint32_t d = state_[3];
        uint32_t e = state_[4];
        uint32_t f = state_[5];
        uint32_t g = state_[6];
        uint32_t h = state_[7];

        for (size_t i = 0; i < 64; ++i) {
            uint32_t t1 = h + ep1(e) + ch(e, f, g) + k[i] + m[i];
            uint32_t t2 = ep0(a) + maj(a, b, c);
            h = g;
            g = f;
            f = e;
            e = d + t1;
            d = c;
            c = b;
            b = a;
            a = t1 + t2;
        }

        state_[0] += a;
        state_[1] += b;
        state_[2] += c;
        state_[3] += d;
        state_[4] += e;
        state_[5] += f;
        state_[6] += g;
        state_[7] += h;
    }

    uint8_t data_[64];
    size_t datalen_;
    uint64_t bitlen_;
    uint32_t state_[8];
};

} // namespace

core::ErrorCode sanitize_relative_path(
    const std::filesystem::path& path, 
    std::string& out_normalized_path
) {
    if (path.empty() || path.is_absolute()) {
        return core::ErrorCode::kSecurityViolation;
    }

    // Reconstruct canonical path element-by-element
    std::vector<std::string> parts;
    for (const auto& elem : path) {
        std::string s = elem.string();
        if (s == "." || s.empty()) {
            continue;
        }
        if (s == "..") {
            // Traversal attempt escaping relative root boundary
            return core::ErrorCode::kSecurityViolation;
        }
        parts.push_back(std::move(s));
    }

    if (parts.empty()) {
        return core::ErrorCode::kInvalidArgument;
    }

    // Join using forward slash POSIX separator regardless of host OS
    std::string joined;
    for (size_t i = 0; i < parts.size(); ++i) {
        if (i > 0) joined += "/";
        joined += parts[i];
    }

    out_normalized_path = std::move(joined);
    return core::ErrorCode::kOk;
}

core::ErrorCode compute_file_sha256(
    const std::filesystem::path& absolute_path, 
    std::string& out_hex
) {
    std::ifstream file(absolute_path, std::ios::binary);
    if (!file.is_open()) {
        return core::ErrorCode::kStorageError;
    }

    Sha256Ctx ctx;
    std::vector<uint8_t> buffer(core::kDefaultChunkSizeBytes);

    while (file.good()) {
        file.read(reinterpret_cast<char*>(buffer.data()), static_cast<std::streamsize>(buffer.size()));
        std::streamsize bytes_read = file.gcount();
        if (bytes_read > 0) {
            ctx.update(buffer.data(), static_cast<size_t>(bytes_read));
        }
    }

    if (file.bad()) {
        return core::ErrorCode::kStorageError;
    }

    uint8_t digest[32];
    ctx.final(digest);
    out_hex = to_hex_string(digest, sizeof(digest));
    return core::ErrorCode::kOk;
}

core::ErrorCode build_collection_manifest(
    const std::filesystem::path& root_path,
    const std::string& collection_name,
    const ScannerOptions& options,
    Manifest& out_manifest
) {
    std::error_code ec;
    if (!std::filesystem::exists(root_path, ec) || !std::filesystem::is_directory(root_path, ec)) {
        return core::ErrorCode::kInvalidArgument;
    }

    out_manifest.collection_id = core::CollectionId("col_" + collection_name);
    out_manifest.collection_name = collection_name;
    out_manifest.version = 1;
    out_manifest.created_epoch_sec = static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()
        ).count()
    );
    out_manifest.total_size_bytes = 0;
    out_manifest.files.clear();

    uint32_t file_count = 0;

    // Use recursive directory iterator
    auto dir_options = std::filesystem::directory_options::none;
    if (options.symlink_policy == SymlinkPolicy::kFollowInternal) {
        dir_options = std::filesystem::directory_options::follow_directory_symlink;
    }

    for (std::filesystem::recursive_directory_iterator it(root_path, dir_options, ec), end; 
         it != end && !ec; it.increment(ec)) {
        
        const auto& entry = *it;
        const auto& path = entry.path();

        // Enforce symlink policy
        if (entry.is_symlink(ec)) {
            if (options.symlink_policy == SymlinkPolicy::kDisallow) {
                return core::ErrorCode::kSecurityViolation;
            }
        }

        // Hidden file filter
        std::string filename = path.filename().string();
        if (!options.include_hidden && !filename.empty() && filename[0] == '.') {
            if (entry.is_directory(ec)) {
                it.disable_recursion_pending();
            }
            continue;
        }

        if (entry.is_regular_file(ec)) {
            file_count++;
            if (file_count > options.max_file_count) {
                return core::ErrorCode::kStorageError;
            }

            // Derive relative path from root
            std::filesystem::path rel = std::filesystem::relative(path, root_path, ec);
            if (ec) {
                return core::ErrorCode::kStorageError;
            }

            std::string normalized;
            auto err = sanitize_relative_path(rel, normalized);
            if (err != core::ErrorCode::kOk) {
                return err;
            }

            if (normalized.length() > options.max_path_length) {
                return core::ErrorCode::kSecurityViolation;
            }

            uint64_t size = entry.file_size(ec);
            if (ec) {
                return core::ErrorCode::kStorageError;
            }

            out_manifest.total_size_bytes += size;
            if (out_manifest.total_size_bytes > options.max_total_size_bytes) {
                return core::ErrorCode::kStorageError;
            }

            auto last_write = entry.last_write_time(ec);
            uint64_t mtime = 0;
            if (!ec) {
                mtime = static_cast<uint64_t>(last_write.time_since_epoch().count());
            }

            std::string sha256;
            err = compute_file_sha256(path, sha256);
            if (err != core::ErrorCode::kOk) {
                return err;
            }

            FileEntry fe;
            fe.id = core::FileId("f_" + sha256.substr(0, 16));
            fe.relative_path = std::move(normalized);
            fe.size_bytes = size;
            fe.modified_epoch_sec = mtime;
            fe.sha256_hex = std::move(sha256);

            out_manifest.files.push_back(std::move(fe));
        }
    }

    if (ec) {
        return core::ErrorCode::kStorageError;
    }

    // Deterministic sorting: sort files alphabetically by relative path
    std::sort(out_manifest.files.begin(), out_manifest.files.end());

    return core::ErrorCode::kOk;
}

core::ErrorCode verify_source_integrity(
    const std::filesystem::path& root_path,
    const Manifest& manifest
) {
    std::error_code ec;
    for (const auto& expected : manifest.files) {
        std::filesystem::path full = root_path / std::filesystem::path(expected.relative_path);
        if (!std::filesystem::exists(full, ec) || !std::filesystem::is_regular_file(full, ec)) {
            return core::ErrorCode::kSourceMutated;
        }

        uint64_t current_size = std::filesystem::file_size(full, ec);
        if (ec || current_size != expected.size_bytes) {
            return core::ErrorCode::kSourceMutated;
        }

        std::string current_sha;
        auto err = compute_file_sha256(full, current_sha);
        if (err != core::ErrorCode::kOk || current_sha != expected.sha256_hex) {
            return core::ErrorCode::kSourceMutated;
        }
    }

    return core::ErrorCode::kOk;
}

} // namespace dibyt::manifest
