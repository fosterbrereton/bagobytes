/******************************************************************************/
// Boilerplate.
/******************************************************************************/

// stdc++
#include <cmath>
#include <fstream>
#include <iostream>

// zlib
#include <zlib.h>

/******************************************************************************/

void base64_round(const char* s, char* d, std::size_t n) {
    constexpr char table_k[]{"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/"};

    *d++ = table_k[(s[0] >> 2) & 0x3f];
    *d++ = table_k[(s[0] << 4 | s[1] >> 4) & 0x3f];

    if (n == 1) {
        *d++ = '=';
        *d++ = '=';
    } else {
        *d++ = table_k[(s[1] << 2 | s[2] >> 6) & 0x3f];

        if (n == 2) {
            *d++ = '=';
        } else {
            *d++ = table_k[(s[2]) & 0x3f];
        }
    }
}

/******************************************************************************/

std::string base64(const std::string& src_str) {
    std::size_t l(src_str.size());
    std::string result(std::ceil(l / 3.) * 4, '?');
    auto        src{src_str.data()};
    auto        dst{&result[0]};

    while (true) {
        std::size_t round_size(std::min<std::size_t>(l, 3));

        base64_round(src, dst, round_size);

        if (l < 4)
            break;

        l   -= 3;
        src += 3;
        dst += 4;
    }

    return result;
}

/******************************************************************************/

void zassert(int err) {
    if (err != Z_OK)
        throw std::runtime_error("deflate: " + std::to_string(err));
}

/******************************************************************************/

std::string deflate(const std::string& src_str) {
    constexpr std::size_t chunk_size_k{16384};

    z_stream      stream;
    unsigned char out[chunk_size_k];
    std::string   result;

    stream.zalloc = Z_NULL;
    stream.zfree = Z_NULL;
    stream.opaque = Z_NULL;

    auto err = deflateInit(&stream, Z_BEST_COMPRESSION);

    if (err != Z_OK)
        throw std::runtime_error("deflate: " + std::to_string(err));

    std::size_t l(src_str.size());
    auto        src{src_str.data()};

    while (true) {
        std::size_t avail{std::min(l, chunk_size_k)};

        l -= avail;

        stream.avail_in = avail;
        stream.next_in = reinterpret_cast<Bytef*>(const_cast<char*>(src));

        do {
            stream.avail_out = chunk_size_k;
            stream.next_out = out;

            err = deflate(&stream, l ? Z_NO_FLUSH : Z_FINISH);

            if (err != Z_OK && err != Z_STREAM_END)
                throw std::runtime_error("deflate: " + std::to_string(err));

            auto have = chunk_size_k - stream.avail_out;

            result += std::string(&out[0], &out[have]);
        } while(stream.avail_out == 0);

        if (!l)
            break;

        src += avail;
    };

    deflateEnd(&stream);

    return result;
}

/******************************************************************************/

std::string inflate(const std::string& src_str) {
    constexpr std::size_t chunk_size_k{16384};

    z_stream      stream;
    unsigned char out[chunk_size_k];
    std::string   result;

    stream.zalloc = Z_NULL;
    stream.zfree = Z_NULL;
    stream.opaque = Z_NULL;

    auto err = inflateInit(&stream);

    if (err != Z_OK)
        throw std::runtime_error("inflate: " + std::to_string(err));

    std::size_t l(src_str.size());
    auto        src{src_str.data()};

    while (true) {
        std::size_t avail{std::min(l, chunk_size_k)};

        l -= avail;

        stream.avail_in = avail;
        stream.next_in = reinterpret_cast<Bytef*>(const_cast<char*>(src));

        do {
            stream.avail_out = chunk_size_k;
            stream.next_out = out;

            err = inflate(&stream, l ? Z_NO_FLUSH : Z_FINISH);

            if (err != Z_OK && err != Z_STREAM_END)
                throw std::runtime_error("inflate: " + std::to_string(err));

            auto have = chunk_size_k - stream.avail_out;

            result += std::string(&out[0], &out[have]);
        } while(stream.avail_out == 0);

        if (!l)
            break;

        src += avail;
    };

    deflateEnd(&stream);

    return result;
}

/******************************************************************************/

void run_tests() {
    auto test_base64([](const std::string& plain, const std::string& expected) {
        if (base64(plain) == expected)
            return;

        throw std::runtime_error("test_base64 `" + plain + "` failed");
    });

    test_base64("Man", "TWFu");
    test_base64("Ma", "TWE=");
    test_base64("M", "TQ==");

    test_base64("pleasure.", "cGxlYXN1cmUu");
    test_base64("leasure.", "bGVhc3VyZS4=");
    test_base64("easure.", "ZWFzdXJlLg==");
    test_base64("asure.", "YXN1cmUu");
    test_base64("sure.", "c3VyZS4=");

    auto test_deflate([](const std::string& plain) {
        auto deflated(deflate(plain));

        if (inflate(deflated) == plain)
            return;

        throw std::runtime_error("test_deflate `" + plain + "` failed");
    });

    test_deflate("Hello Hello Hello Hello Hello Hello!");
}

/******************************************************************************/

int main(int argc, char** argv) try {
    if (argc < 2) {
        std::cerr << "Usage:\n";
        std::cerr << "    bagobytes file\n";
        std::cerr << "\n";
        std::cerr << "Produces to stdout a zlib compressed, base64 encoded rendition of the file's contents.\n";

        run_tests();

        return 1;
    }

    std::ifstream file(argv[1], std::ios::binary | std::ios::ate);

    if (!file)
        throw std::runtime_error("Could not open input file for reading");

    auto size{file.tellg()};
    file.seekg(0, std::ios::beg);

    std::string buffer(size, 0);

    if (!file.read(&buffer[0], size))
        throw std::runtime_error("File read failure");

    std::cout << base64(deflate(buffer)) << '\n';

    return 0;
} catch (const std::exception& error) {
    std::cerr << "Fatal error: " << error.what() << '\n';
    return 1;
} catch (...) {
    std::cerr << "Fatal error: unknown\n";
    return 1;
}

/******************************************************************************/
