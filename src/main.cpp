/******************************************************************************/
// Bagobytes. Copyright 2017 Foster T. Brereton. All rights reserved.
/******************************************************************************/

// stdc++
#include <cmath>
#include <fstream>
#include <iostream>

// zlib
#include <zlib.h>

/******************************************************************************/

template <typename I, // models ForwardIterator
          typename O> // models OutputIterator
std::pair<I, O> base64_round(I s, O d, std::size_t n) {
    constexpr char table_k[]{"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/"};

    static_assert(sizeof(typename std::iterator_traits<I>::value_type) == 1, "bad value type");
    //static_assert(sizeof(typename std::iterator_traits<O>::value_type) == 1, "bad value type");

    auto s0{*s++};
    auto s1{*s++};

    *d++ = table_k[(s0 >> 2) & 0x3f];
    *d++ = table_k[(s0 << 4 | s1 >> 4) & 0x3f];

    if (n == 1) {
        *d++ = '=';
        *d++ = '=';
    } else {
        auto s2{*s++};

        *d++ = table_k[(s1 << 2 | s2 >> 6) & 0x3f];

        if (n == 2) {
            *d++ = '=';
        } else {
            *d++ = table_k[s2 & 0x3f];
        }
    }

    return std::make_pair(s, d);
}

/******************************************************************************/

template <typename I, // models ForwardIterator
          typename O> // models OutputIterator
O base64(I src, I last, O dst) {
    std::size_t l(std::distance(src, last));
    std::string result(std::ceil(l / 3.) * 4, '?');

    while (true) {
        std::tie(src, dst) = base64_round(src, dst, std::min<std::size_t>(l, 3));

        if (l < 4)
            break;

        l -= 3;
    }

    return dst;
}

/******************************************************************************/

template <typename R, // models ForwardRange
          typename O> // models OutputIterator
O base64(const R& r, O dst) {
    return base64(std::begin(r), std::end(r), dst);
}

/******************************************************************************/

constexpr std::size_t chunk_size_k{4*1024*1024}; // 4MB buffer for encoding

struct deleter_t {
    void operator()(void* x) const {
        std::free(x);
    }
};

typedef std::unique_ptr<void, deleter_t> buffer_t;

/******************************************************************************/

template <typename I, // models ForwardIterator
          typename O> // models OutputIterator
O deflate(I src, I last, O dst) {
    static_assert(sizeof(typename std::iterator_traits<I>::value_type) == 1, "bad value type");

    z_stream       stream;
    buffer_t       outb(std::malloc(chunk_size_k));
    unsigned char* out(reinterpret_cast<unsigned char*>(outb.get()));

    stream.zalloc = Z_NULL;
    stream.zfree = Z_NULL;
    stream.opaque = Z_NULL;

    auto err = deflateInit(&stream, Z_BEST_COMPRESSION);

    if (err != Z_OK)
        throw std::runtime_error("deflate: " + std::to_string(err));

    std::size_t l(std::distance(src, last));

    while (true) {
        std::size_t avail{std::min(l, chunk_size_k)};

        l -= avail;

        stream.avail_in = avail;
        stream.next_in = reinterpret_cast<Bytef*>(const_cast<char*>(&*src));

        do {
            stream.avail_out = chunk_size_k;
            stream.next_out = out;

            err = deflate(&stream, l ? Z_NO_FLUSH : Z_FINISH);

            if (err != Z_OK && err != Z_STREAM_END)
                throw std::runtime_error("deflate: " + std::to_string(err));

            auto have = chunk_size_k - stream.avail_out;

            dst = std::copy(&out[0], &out[have], dst);
        } while(stream.avail_out == 0);

        if (!l)
            break;

        src += avail;
    };

    deflateEnd(&stream);

    return dst;
}

/******************************************************************************/

template <typename R, // models ForwardRange
          typename O> // models OutputIterator
O deflate(R r, O dst) {
    return deflate(std::begin(r), std::end(r), dst);
}

/******************************************************************************/

template <typename I, // models ForwardIterator
          typename O> // models OutputIterator
O inflate(I src, I last, O dst) {
    static_assert(sizeof(typename std::iterator_traits<I>::value_type) == 1, "bad value type");

    z_stream       stream;
    buffer_t       outb(std::malloc(chunk_size_k));
    unsigned char* out(reinterpret_cast<unsigned char*>(outb.get()));

    stream.zalloc = Z_NULL;
    stream.zfree = Z_NULL;
    stream.opaque = Z_NULL;

    auto err = inflateInit(&stream);

    if (err != Z_OK)
        throw std::runtime_error("inflate: " + std::to_string(err));

    std::size_t l(std::distance(src, last));

    while (true) {
        std::size_t avail{std::min(l, chunk_size_k)};

        l -= avail;

        stream.avail_in = avail;
        stream.next_in = reinterpret_cast<Bytef*>(const_cast<char*>(&*src));

        do {
            stream.avail_out = chunk_size_k;
            stream.next_out = out;

            err = inflate(&stream, l ? Z_NO_FLUSH : Z_FINISH);

            if (err != Z_OK && err != Z_STREAM_END)
                throw std::runtime_error("inflate: " + std::to_string(err));

            auto have = chunk_size_k - stream.avail_out;

            dst = std::copy(&out[0], &out[have], dst);
        } while(stream.avail_out == 0);

        if (!l)
            break;

        src += avail;
    };

    deflateEnd(&stream);

    return dst;
}

/******************************************************************************/

template <typename R, // models ForwardRange
          typename O> // models OutputIterator
O inflate(R r, O dst) {
    return inflate(std::begin(r), std::end(r), dst);
}

/******************************************************************************/

void run_tests() {
    auto test_base64([](const std::string& plain, const std::string& expected) {
        std::string encoded;

        base64(plain, std::back_inserter(encoded));

        if (encoded == expected)
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
        std::string deflated;

        deflate(plain, std::back_inserter(deflated));

        std::string inflated;

        inflate(deflated, std::back_inserter(inflated));

        if (inflated == plain)
            return;

        throw std::runtime_error("test_deflate `" + plain + "` failed");
    });

    test_deflate("Hello, world!");
    test_deflate("Hello Hello Hello Hello Hello Hello!");
    test_deflate("Lorem ipsum dolor sit amet, consectetur adipiscing elit, sed do eiusmod tempor incididunt ut labore et dolore magna aliqua. Ut enim ad minim veniam, quis nostrud exercitation ullamco laboris nisi ut aliquip ex ea commodo consequat. Duis aute irure dolor in reprehenderit in voluptate velit esse cillum dolore eu fugiat nulla pariatur. Excepteur sint occaecat cupidatat non proident, sunt in culpa qui officia deserunt mollit anim id est laborum.");
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

    std::string deflated;

    deflate(buffer, std::back_inserter(deflated));
    base64(deflated, std::ostream_iterator<unsigned char>(std::cout));

    return 0;
} catch (const std::exception& error) {
    std::cerr << "Fatal error: " << error.what() << '\n';
    return 1;
} catch (...) {
    std::cerr << "Fatal error: unknown\n";
    return 1;
}

/******************************************************************************/
