// A glideslope client written from docs/TRANSPORT.md alone, with the public
// Noise Protocol Framework (revision 34) for the handshake. It uses the C++20
// standard library, libsodium and the operating system's UDP sockets, and
// nothing from the glideslope code base.
//
//   doc_client HOST:PORT SERVER_PUBLIC_KEY_HEX SECONDS

#include <sodium.h>

#include <algorithm>
#include <array>
#include <bit>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <optional>
#include <span>
#include <string>
#include <vector>

#ifdef _WIN32
// Added after the client was written, when CI first compiled this branch:
// windows.h defines `min` and `max` as macros unless told not to, which breaks
// std::min and std::max. It changes nothing about the protocol.
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
using Socket = SOCKET;
static const Socket kNoSocket = INVALID_SOCKET;
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>
using Socket = int;
static const Socket kNoSocket = -1;
#endif

namespace {

using Bytes = std::vector<std::uint8_t>;
using Clock = std::chrono::steady_clock;

// ---------------------------------------------------------------- the wire

constexpr std::size_t kMaxDatagram = 1232;
constexpr std::array<std::uint8_t, 4> kMagic{0x47, 0x4C, 0x44, 0x53};
constexpr std::uint8_t kVersion = 0x01;

constexpr std::uint8_t kTypeInitiation = 0x01;
constexpr std::uint8_t kTypeResponse = 0x02;
constexpr std::uint8_t kTypeSealed = 0x03;
constexpr std::uint8_t kTypeRefusal = 0x04;

constexpr std::uint8_t kKindInputs = 0x02;
constexpr std::uint8_t kKindState = 0x03;
constexpr std::uint8_t kKindPing = 0x04;
constexpr std::uint8_t kKindPong = 0x05;

const char* refusal_name(std::uint8_t reason) {
    switch (reason) {
    case 0x00: return "UNKNOWN";
    case 0x01: return "NOT_THIS_PROTOCOL";
    case 0x02: return "WRONG_VERSION";
    case 0x03: return "UNKNOWN_TYPE";
    case 0x04: return "TOO_SHORT";
    case 0x05: return "SERVER_FULL";
    case 0x06: return "BAD_HANDSHAKE";
    default: return "UNKNOWN";  // a reason this version does not know
    }
}

class Writer {
public:
    void u8(std::uint8_t v) { out_.push_back(v); }
    void u16(std::uint16_t v) {
        for (int i = 0; i < 2; ++i) out_.push_back(static_cast<std::uint8_t>(v >> (8 * i)));
    }
    void u32(std::uint32_t v) {
        for (int i = 0; i < 4; ++i) out_.push_back(static_cast<std::uint8_t>(v >> (8 * i)));
    }
    void u64(std::uint64_t v) {
        for (int i = 0; i < 8; ++i) out_.push_back(static_cast<std::uint8_t>(v >> (8 * i)));
    }
    void i16(std::int16_t v) { u16(std::bit_cast<std::uint16_t>(v)); }
    void bytes(std::span<const std::uint8_t> b) { out_.insert(out_.end(), b.begin(), b.end()); }
    Bytes& data() { return out_; }

private:
    Bytes out_;
};

// Never reads past the end: the first read it cannot satisfy marks it broken,
// and from then on it answers zero. Asked once at the end whether it was real.
class Reader {
public:
    explicit Reader(std::span<const std::uint8_t> in) : in_(in) {}

    std::uint8_t u8() { return static_cast<std::uint8_t>(take(1)); }
    std::uint16_t u16() { return static_cast<std::uint16_t>(take(2)); }
    std::uint32_t u32() { return static_cast<std::uint32_t>(take(4)); }
    std::uint64_t u64() { return take(8); }
    double f64() {
        const double v = std::bit_cast<double>(u64());
        if (!std::isfinite(v)) broken_ = true;
        return v;
    }
    float f32() {
        const float v = std::bit_cast<float>(u32());
        if (!std::isfinite(v)) broken_ = true;
        return v;
    }
    void fail() { broken_ = true; }
    bool ok() const { return !broken_; }
    bool at_end() const { return pos_ == in_.size(); }
    // Whole, unbroken and with nothing left over.
    bool complete() const { return ok() && at_end(); }

private:
    std::uint64_t take(std::size_t n) {
        if (broken_ || in_.size() - pos_ < n) {
            broken_ = true;
            return 0;
        }
        std::uint64_t v = 0;
        for (std::size_t i = 0; i < n; ++i) v |= std::uint64_t{in_[pos_ + i]} << (8 * i);
        pos_ += n;
        return v;
    }

    std::span<const std::uint8_t> in_;
    std::size_t pos_ = 0;
    bool broken_ = false;
};

Bytes envelope(std::uint8_t type) {
    Bytes d(kMagic.begin(), kMagic.end());
    d.push_back(kVersion);
    d.push_back(type);
    return d;
}

// ------------------------------------------------------ Noise, revision 34

constexpr std::size_t kHashLen = 64;   // BLAKE2b
constexpr std::size_t kBlockLen = 128; // BLAKE2b
constexpr std::size_t kDhLen = 32;     // 25519
constexpr std::size_t kKeyLen = 32;
constexpr std::size_t kTagLen = 16;

using Digest = std::array<std::uint8_t, kHashLen>;
using Key = std::array<std::uint8_t, kKeyLen>;

Digest blake2b(std::initializer_list<std::span<const std::uint8_t>> parts) {
    crypto_generichash_blake2b_state st;
    crypto_generichash_blake2b_init(&st, nullptr, 0, kHashLen);
    for (auto p : parts) crypto_generichash_blake2b_update(&st, p.data(), p.size());
    Digest out{};
    crypto_generichash_blake2b_final(&st, out.data(), kHashLen);
    return out;
}

// HMAC-HASH(key, data), RFC 2104, as the Noise specification uses it.
Digest hmac(std::span<const std::uint8_t> key, std::initializer_list<std::span<const std::uint8_t>> data) {
    std::array<std::uint8_t, kBlockLen> k{};
    if (key.size() > kBlockLen) {
        const Digest kd = blake2b({key});
        std::copy(kd.begin(), kd.end(), k.begin());
    } else {
        std::copy(key.begin(), key.end(), k.begin());
    }
    std::array<std::uint8_t, kBlockLen> ipad{};
    std::array<std::uint8_t, kBlockLen> opad{};
    for (std::size_t i = 0; i < kBlockLen; ++i) {
        ipad[i] = static_cast<std::uint8_t>(k[i] ^ 0x36);
        opad[i] = static_cast<std::uint8_t>(k[i] ^ 0x5c);
    }
    crypto_generichash_blake2b_state st;
    crypto_generichash_blake2b_init(&st, nullptr, 0, kHashLen);
    crypto_generichash_blake2b_update(&st, ipad.data(), ipad.size());
    for (auto p : data) crypto_generichash_blake2b_update(&st, p.data(), p.size());
    Digest inner{};
    crypto_generichash_blake2b_final(&st, inner.data(), kHashLen);
    return blake2b({opad, inner});
}

// HKDF(chaining_key, input_key_material, 2).
std::pair<Digest, Digest> hkdf2(const Digest& ck, std::span<const std::uint8_t> ikm) {
    const Digest temp = hmac(ck, {ikm});
    const std::array<std::uint8_t, 1> one{0x01};
    const std::array<std::uint8_t, 1> two{0x02};
    const Digest out1 = hmac(temp, {one});
    const Digest out2 = hmac(temp, {out1, two});
    return {out1, out2};
}

std::array<std::uint8_t, 12> chacha_nonce(std::uint64_t n) {
    std::array<std::uint8_t, 12> nonce{};
    for (int i = 0; i < 8; ++i) nonce[static_cast<std::size_t>(4 + i)] = static_cast<std::uint8_t>(n >> (8 * i));
    return nonce;
}

Bytes aead_encrypt(const Key& k, std::uint64_t n, std::span<const std::uint8_t> ad,
                   std::span<const std::uint8_t> pt) {
    Bytes ct(pt.size() + kTagLen);
    unsigned long long clen = 0;
    const auto nonce = chacha_nonce(n);
    crypto_aead_chacha20poly1305_ietf_encrypt(ct.data(), &clen, pt.data(), pt.size(), ad.data(), ad.size(),
                                              nullptr, nonce.data(), k.data());
    ct.resize(static_cast<std::size_t>(clen));
    return ct;
}

std::optional<Bytes> aead_decrypt(const Key& k, std::uint64_t n, std::span<const std::uint8_t> ad,
                                  std::span<const std::uint8_t> ct) {
    if (ct.size() < kTagLen) return std::nullopt;
    Bytes pt(ct.size() - kTagLen);
    unsigned long long plen = 0;
    const auto nonce = chacha_nonce(n);
    if (crypto_aead_chacha20poly1305_ietf_decrypt(pt.data(), &plen, nullptr, ct.data(), ct.size(), ad.data(),
                                                  ad.size(), nonce.data(), k.data()) != 0) {
        return std::nullopt;
    }
    pt.resize(static_cast<std::size_t>(plen));
    return pt;
}

struct KeyPair {
    std::array<std::uint8_t, kDhLen> pub{};
    std::array<std::uint8_t, kDhLen> sec{};
};

KeyPair generate_keypair() {
    KeyPair kp;
    randombytes_buf(kp.sec.data(), kp.sec.size());
    crypto_scalarmult_base(kp.pub.data(), kp.sec.data());
    return kp;
}

std::optional<std::array<std::uint8_t, kDhLen>> dh(const KeyPair& kp, std::span<const std::uint8_t> pub) {
    std::array<std::uint8_t, kDhLen> out{};
    if (pub.size() != kDhLen || crypto_scalarmult(out.data(), kp.sec.data(), pub.data()) != 0) return std::nullopt;
    return out;
}

struct SymmetricState {
    Digest ck{};
    Digest h{};
    Key k{};
    bool has_key = false;
    std::uint64_t n = 0;

    void initialize(const std::string& protocol_name) {
        if (protocol_name.size() <= kHashLen) {
            h.fill(0);
            std::memcpy(h.data(), protocol_name.data(), protocol_name.size());
        } else {
            h = blake2b({std::span(reinterpret_cast<const std::uint8_t*>(protocol_name.data()),
                                   protocol_name.size())});
        }
        ck = h;
        has_key = false;
    }
    void mix_hash(std::span<const std::uint8_t> data) { h = blake2b({h, data}); }
    void mix_key(std::span<const std::uint8_t> ikm) {
        const auto [ck2, temp] = hkdf2(ck, ikm);
        ck = ck2;
        std::copy_n(temp.begin(), kKeyLen, k.begin());
        has_key = true;
        n = 0;
    }
    Bytes encrypt_and_hash(std::span<const std::uint8_t> pt) {
        Bytes ct = has_key ? aead_encrypt(k, n++, h, pt) : Bytes(pt.begin(), pt.end());
        mix_hash(ct);
        return ct;
    }
    std::optional<Bytes> decrypt_and_hash(std::span<const std::uint8_t> ct) {
        std::optional<Bytes> pt;
        if (has_key) {
            pt = aead_decrypt(k, n, h, ct);
            if (!pt) return std::nullopt;
            ++n;
        } else {
            pt = Bytes(ct.begin(), ct.end());
        }
        mix_hash(ct);
        return pt;
    }
    std::pair<Key, Key> split() const {
        const auto [t1, t2] = hkdf2(ck, {});
        Key k1{};
        Key k2{};
        std::copy_n(t1.begin(), kKeyLen, k1.begin());
        std::copy_n(t2.begin(), kKeyLen, k2.begin());
        return {k1, k2};
    }
};

// The initiator of Noise_IK_25519_ChaChaPoly_BLAKE2b:
//   <- s
//   ...
//   -> e, es, s, ss
//   <- e, ee, se
struct Initiator {
    SymmetricState ss;
    KeyPair s;
    KeyPair e;
    std::array<std::uint8_t, kDhLen> rs{};

    bool write_message_one(const std::array<std::uint8_t, kDhLen>& server_static, Bytes& out) {
        rs = server_static;
        ss.initialize("Noise_IK_25519_ChaChaPoly_BLAKE2b");
        ss.mix_hash({});  // the empty prologue
        ss.mix_hash(rs);  // the pre-message: the responder's static key
        e = generate_keypair();
        out.insert(out.end(), e.pub.begin(), e.pub.end());
        ss.mix_hash(e.pub);
        const auto es = dh(e, rs);
        if (!es) return false;
        ss.mix_key(*es);
        const Bytes sealed_s = ss.encrypt_and_hash(s.pub);
        out.insert(out.end(), sealed_s.begin(), sealed_s.end());
        const auto s_s = dh(s, rs);
        if (!s_s) return false;
        ss.mix_key(*s_s);
        const Bytes payload = ss.encrypt_and_hash({});  // the payload is empty
        out.insert(out.end(), payload.begin(), payload.end());
        return true;
    }

    // Reads message two on a copy, so that a message that does not complete
    // leaves this state as it was, ready for the real answer.
    std::optional<std::pair<Key, Key>> read_message_two(std::span<const std::uint8_t> msg) const {
        if (msg.size() < kDhLen + kTagLen) return std::nullopt;
        SymmetricState t = ss;
        const auto re = msg.subspan(0, kDhLen);
        t.mix_hash(re);
        const auto ee = dh(e, re);
        if (!ee) return std::nullopt;
        t.mix_key(*ee);
        const auto se = dh(s, re);
        if (!se) return std::nullopt;
        t.mix_key(*se);
        if (!t.decrypt_and_hash(msg.subspan(kDhLen))) return std::nullopt;
        return t.split();
    }
};

// ------------------------------------------------------------- the seal

class Sealer {
public:
    explicit Sealer(const Key& k) : k_(k) {}
    Bytes seal(std::span<const std::uint8_t> plaintext) {
        Writer w;
        w.bytes(envelope(kTypeSealed));
        Writer seq;
        seq.u64(next_);
        w.bytes(seq.data());
        w.bytes(aead_encrypt(k_, next_, seq.data(), plaintext));
        ++next_;
        return w.data();
    }

private:
    Key k_;
    std::uint64_t next_ = 0;
};

class Opener {
public:
    explicit Opener(const Key& k) : k_(k) {}
    // `body` is a SEALED datagram's body: the sequence and the ciphertext.
    std::optional<Bytes> open(std::span<const std::uint8_t> body) {
        if (body.size() < 8 + kTagLen) return std::nullopt;
        Reader r(body.subspan(0, 8));
        const std::uint64_t seq = r.u64();
        if (!fresh(seq)) return std::nullopt;
        auto pt = aead_decrypt(k_, seq, body.subspan(0, 8), body.subspan(8));
        if (!pt) return std::nullopt;
        accept(seq);  // only a body that opens moves the window
        return pt;
    }

private:
    bool fresh(std::uint64_t seq) const {
        if (!any_) return true;
        if (seq > highest_) return true;
        const std::uint64_t behind = highest_ - seq;
        if (behind == 0 || behind >= 64) return false;
        return ((bitmap_ >> (behind - 1)) & 1u) == 0;
    }
    void accept(std::uint64_t seq) {
        if (!any_) {
            any_ = true;
            highest_ = seq;
            bitmap_ = 0;
            return;
        }
        if (seq > highest_) {
            const std::uint64_t shift = seq - highest_;
            // bit i means "highest - 1 - i has been opened"
            if (shift >= 64) {
                bitmap_ = 0;
            } else {
                bitmap_ = (bitmap_ << shift) | (std::uint64_t{1} << (shift - 1));
            }
            highest_ = seq;
        } else {
            bitmap_ |= std::uint64_t{1} << (highest_ - seq - 1);
        }
    }

    Key k_;
    bool any_ = false;
    std::uint64_t highest_ = 0;
    std::uint64_t bitmap_ = 0;
};

// ------------------------------------------------------------ the socket

class Udp {
public:
    Udp() {
#ifdef _WIN32
        WSADATA wsa;
        started_ = WSAStartup(MAKEWORD(2, 2), &wsa) == 0;
#endif
        s_ = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    }
    ~Udp() {
        if (s_ != kNoSocket) {
#ifdef _WIN32
            closesocket(s_);
#else
            close(s_);
#endif
        }
#ifdef _WIN32
        if (started_) WSACleanup();
#endif
    }
    Udp(const Udp&) = delete;
    Udp& operator=(const Udp&) = delete;

    bool ok() const { return s_ != kNoSocket; }

    bool send(const sockaddr_in& to, std::span<const std::uint8_t> d) const {
        if (d.size() > kMaxDatagram) return false;  // never offer more than 1232
#ifdef _WIN32
        const int sent = sendto(s_, reinterpret_cast<const char*>(d.data()), static_cast<int>(d.size()), 0,
                                reinterpret_cast<const sockaddr*>(&to), static_cast<int>(sizeof to));
        return sent == static_cast<int>(d.size());
#else
        const ssize_t sent =
            sendto(s_, d.data(), d.size(), 0, reinterpret_cast<const sockaddr*>(&to), sizeof to);
        return sent == static_cast<ssize_t>(d.size());
#endif
    }

    // Waits up to `ms` for one datagram. Returns false when none came.
    bool receive(int ms, Bytes& out, sockaddr_in& from) const {
#ifdef _WIN32
        WSAPOLLFD p{};
        p.fd = s_;
        p.events = POLLRDNORM;
        if (WSAPoll(&p, 1, ms) <= 0) return false;
        int flen = static_cast<int>(sizeof from);
        const int n = recvfrom(s_, reinterpret_cast<char*>(buf_.data()), static_cast<int>(buf_.size()), 0,
                               reinterpret_cast<sockaddr*>(&from), &flen);
        if (n < 0) return true;  // an error, e.g. ICMP unreachable; nothing to read
        out.assign(buf_.begin(), buf_.begin() + n);
#else
        pollfd p{};
        p.fd = s_;
        p.events = POLLIN;
        if (poll(&p, 1, ms) <= 0) return false;
        socklen_t flen = sizeof from;
        const ssize_t n =
            recvfrom(s_, buf_.data(), buf_.size(), 0, reinterpret_cast<sockaddr*>(&from), &flen);
        if (n < 0) {
            out.clear();
            return true;
        }
        out.assign(buf_.begin(), buf_.begin() + n);
#endif
        return true;
    }

private:
    Socket s_ = kNoSocket;
    // Larger than any datagram, so one longer than 1232 is seen and dropped
    // whole rather than cut.
    mutable std::array<std::uint8_t, 65536> buf_{};
#ifdef _WIN32
    bool started_ = false;
#endif
};

bool same_address(const sockaddr_in& a, const sockaddr_in& b) {
    return a.sin_family == b.sin_family && a.sin_port == b.sin_port &&
           std::memcmp(&a.sin_addr, &b.sin_addr, sizeof a.sin_addr) == 0;
}

// The envelope, read and checked. Returns the body, or nothing to drop it.
std::optional<std::pair<std::uint8_t, std::span<const std::uint8_t>>> read_envelope(const Bytes& d) {
    if (d.size() < 6 || d.size() > kMaxDatagram) return std::nullopt;
    if (!std::equal(kMagic.begin(), kMagic.end(), d.begin())) return std::nullopt;
    if (d[4] != kVersion) return std::nullopt;
    const std::uint8_t type = d[5];
    if (type < kTypeInitiation || type > kTypeRefusal) return std::nullopt;
    return std::pair{type, std::span<const std::uint8_t>(d).subspan(6)};
}

// ------------------------------------------------------- the messages

struct AircraftLine {
    std::uint8_t number = 0;
    float roll = 0.0f;
};

struct StateUpdate {
    double clock = 0.0;
    std::uint32_t applied = 0;
    std::uint8_t your_aircraft = 0xFF;
    std::vector<AircraftLine> aircraft;
};

std::optional<StateUpdate> read_state(std::span<const std::uint8_t> pt) {
    Reader r(pt);
    StateUpdate u;
    if (r.u8() != kKindState) return std::nullopt;
    u.clock = r.f64();
    u.applied = r.u32();
    u.your_aircraft = r.u8();
    const std::uint8_t count = r.u8();
    if (count > 20) return std::nullopt;
    for (std::uint8_t i = 0; i < count && r.ok(); ++i) {
        AircraftLine a;
        a.number = r.u8();
        if (r.u8() > 0x02) r.fail();  // a CONTROLLER this version does not know
        for (int j = 0; j < 3; ++j) static_cast<void>(r.f64());  // position
        for (int j = 0; j < 5; ++j) static_cast<void>(r.f32());  // velocity, heading, pitch
        a.roll = r.f32();
        u.aircraft.push_back(a);
    }
    if (!r.complete()) return std::nullopt;
    return u;
}

// A control as its 16-bit fraction: rounded to the nearest step.
std::int16_t fraction(double v) {
    const double c = std::clamp(v, -1.0, 1.0);
    return static_cast<std::int16_t>(std::lround(c * 32767.0));
}

using Frame = std::array<std::int16_t, 17>;

Frame full_left_aileron() {
    std::array<double, 17> c{};
    c[0] = 0.0;   // elevator
    c[1] = -1.0;  // aileron: +1 rolls right, so -1 is full left
    c[2] = 0.0;   // rudder
    c[3] = 1.0;   // throttle, full
    c[4] = 1.0;   // mixture, full rich
    c[5] = 0.0;   // flaps
    c[6] = 0.0;   // left brake
    c[7] = 0.0;   // right brake
    c[8] = 0.0;   // pitch trim
    c[9] = 1.0;   // propeller, highest rpm
    c[10] = 1.0;  // gear down
    c[11] = 1.0;  // supercharger automatic
    c[12] = 0.0;  // speedbrake
    c[13] = 0.0;  // throttle offset, port
    c[14] = 0.0;  // throttle offset, starboard
    c[15] = 0.0;  // cooling flaps, port
    c[16] = 0.0;  // cooling flaps, starboard
    Frame f{};
    for (std::size_t i = 0; i < f.size(); ++i) f[i] = fraction(c[i]);
    return f;
}

std::optional<int> hex_digit(char ch) {
    if (ch >= '0' && ch <= '9') return ch - '0';
    if (ch >= 'a' && ch <= 'f') return ch - 'a' + 10;
    if (ch >= 'A' && ch <= 'F') return ch - 'A' + 10;
    return std::nullopt;
}

bool parse_endpoint(const std::string& arg, sockaddr_in& out) {
    const auto colon = arg.rfind(':');
    if (colon == std::string::npos) return false;
    const std::string host = arg.substr(0, colon);
    const std::string port_text = arg.substr(colon + 1);
    if (port_text.empty() || port_text.size() > 5) return false;
    unsigned long port = 0;
    for (char ch : port_text) {
        if (ch < '0' || ch > '9') return false;
        port = port * 10 + static_cast<unsigned long>(ch - '0');
    }
    if (port == 0 || port > 65535) return false;
    std::memset(&out, 0, sizeof out);
    out.sin_family = AF_INET;
    const std::array<std::uint8_t, 2> be{static_cast<std::uint8_t>(port >> 8), static_cast<std::uint8_t>(port)};
    std::memcpy(&out.sin_port, be.data(), be.size());  // network byte order
    return inet_pton(AF_INET, host.c_str(), &out.sin_addr) == 1;
}

int milliseconds_until(Clock::time_point t) {
    const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(t - Clock::now()).count();
    return static_cast<int>(std::clamp<long long>(ms, 0, 1000));
}

}  // namespace

int main(int argc, char** argv) {
    if (argc != 4) {
        std::fprintf(stderr, "usage: doc_client HOST:PORT SERVER_PUBLIC_KEY_HEX SECONDS\n");
        return 1;
    }
    sockaddr_in server{};
    if (!parse_endpoint(argv[1], server)) {
        std::fprintf(stderr, "not a numeric IPv4 HOST:PORT: %s\n", argv[1]);
        return 1;
    }
    const std::string key_hex = argv[2];
    std::array<std::uint8_t, kDhLen> server_static{};
    if (key_hex.size() != 64) {
        std::fprintf(stderr, "the server's key must be 64 hexadecimal digits\n");
        return 1;
    }
    for (std::size_t i = 0; i < kDhLen; ++i) {
        const auto hi = hex_digit(key_hex[2 * i]);
        const auto lo = hex_digit(key_hex[2 * i + 1]);
        if (!hi || !lo) {
            std::fprintf(stderr, "the server's key must be 64 hexadecimal digits\n");
            return 1;
        }
        server_static[i] = static_cast<std::uint8_t>(*hi * 16 + *lo);
    }
    const double seconds = std::atof(argv[3]);
    if (!(seconds > 0.0)) {
        std::fprintf(stderr, "SECONDS must be positive\n");
        return 1;
    }

    if (sodium_init() < 0) {
        std::fprintf(stderr, "libsodium did not start\n");
        return 1;
    }
    Udp udp;
    if (!udp.ok()) {
        std::fprintf(stderr, "no UDP socket\n");
        return 1;
    }

    // 1. our own static key pair
    Initiator hs;
    hs.s = generate_keypair();

    // 2. the handshake, the same initiation resent until an answer comes
    Bytes initiation = envelope(kTypeInitiation);
    if (!hs.write_message_one(server_static, initiation)) {
        std::fprintf(stderr, "the server's key is not a usable X25519 key\n");
        return 1;
    }
    std::optional<std::pair<Key, Key>> keys;
    const auto handshake_deadline = Clock::now() + std::chrono::seconds(10);
    const auto resend_every = std::chrono::milliseconds(500);
    auto next_send = Clock::now();
    Bytes d;
    sockaddr_in from{};
    while (!keys && Clock::now() < handshake_deadline) {
        if (Clock::now() >= next_send) {
            udp.send(server, initiation);
            next_send = Clock::now() + resend_every;
        }
        if (!udp.receive(milliseconds_until(next_send), d, from)) continue;
        if (!same_address(from, server)) continue;
        const auto env = read_envelope(d);
        if (!env) continue;
        const auto [type, body] = *env;
        if (type == kTypeRefusal) {
            if (d.size() != 7) continue;  // a refusal is always 7 bytes
            std::printf("refused: %s\n", refusal_name(body[0]));
            std::fprintf(stderr, "the server refused the handshake: %s\n", refusal_name(body[0]));
            return 1;
        }
        if (type == kTypeResponse) keys = hs.read_message_two(body);
        // anything else, or a response that does not complete, is dropped
    }
    if (!keys) {
        std::fprintf(stderr, "no answer to the handshake\n");
        return 1;
    }
    std::printf("handshake complete\n");
    std::fflush(stdout);

    // 3. the first key is ours to send with, the second the server's
    Sealer sealer(keys->first);
    Opener opener(keys->second);

    // 4 to 7. inputs out, state updates and pings in, for SECONDS
    const Frame frame = full_left_aileron();
    std::deque<Frame> recent;
    std::uint32_t sequence = 0;
    std::uint64_t updates = 0;
    std::uint64_t pings = 0;
    std::uint32_t applied = 0;
    bool found = false;
    std::uint8_t mine = 0xFF;
    double my_clock = -1.0;
    float my_roll = 0.0f;

    const auto end = Clock::now() + std::chrono::duration_cast<Clock::duration>(std::chrono::duration<double>(seconds));
    const auto input_every = std::chrono::microseconds(1000000 / 30);
    auto next_input = Clock::now();
    while (Clock::now() < end) {
        if (Clock::now() >= next_input) {
            ++sequence;
            recent.push_back(frame);
            if (recent.size() > 4) recent.pop_front();
            Writer w;
            w.u8(kKindInputs);
            w.u32(sequence);
            w.u8(static_cast<std::uint8_t>(recent.size()));
            for (const Frame& f : recent) {
                for (std::int16_t c : f) w.i16(c);
            }
            udp.send(server, sealer.seal(w.data()));
            next_input += input_every;
        }
        const auto wake = std::min(next_input, end);
        if (!udp.receive(milliseconds_until(wake), d, from)) continue;
        if (!same_address(from, server)) continue;
        const auto env = read_envelope(d);
        if (!env) continue;
        const auto [type, body] = *env;
        if (type != kTypeSealed) continue;  // a refusal now is not believed
        const auto pt = opener.open(body);
        if (!pt || pt->empty()) continue;
        switch ((*pt)[0]) {
        case kKindState: {
            const auto u = read_state(*pt);
            if (!u) break;
            ++updates;
            applied = std::max(applied, u->applied);
            for (const AircraftLine& a : u->aircraft) {
                if (a.number == u->your_aircraft && u->your_aircraft != 0xFF) {
                    found = true;
                    mine = a.number;
                    if (u->clock >= my_clock) {
                        my_clock = u->clock;
                    }
                    // The furthest it banked, either way. Changed after the
                    // client was written, from the roll in the last update:
                    // an aeroplane held at full aileron rolls on round, so the
                    // last roll can be anything - 46 degrees on CI after a
                    // whole turn. It is the test's measure, not the protocol.
                    if (std::fabs(a.roll) > std::fabs(my_roll)) {
                        my_roll = a.roll;
                    }
                }
            }
            break;
        }
        case kKindPing: {
            if (pt->size() != 9) break;  // a PING that is not nine bytes is ignored
            Reader r(*pt);
            static_cast<void>(r.u8());
            const std::uint64_t token = r.u64();
            Writer w;
            w.u8(kKindPong);
            w.u64(token);
            if (udp.send(server, sealer.seal(w.data()))) ++pings;
            break;
        }
        default:
            break;  // a kind this client does not use is ignored, not refused
        }
    }

    std::printf("state updates received: %llu\n", static_cast<unsigned long long>(updates));
    if (found) {
        std::printf("my aircraft is number %u\n", static_cast<unsigned>(mine));
    } else {
        std::printf("my aircraft is number none\n");
    }
    std::printf("pings answered: %llu\n", static_cast<unsigned long long>(pings));
    std::printf("sent %lu input frames, the server applied %lu\n", static_cast<unsigned long>(sequence),
                static_cast<unsigned long>(applied));
    std::printf("my aircraft rolled to %ld degrees\n", std::lround(static_cast<double>(my_roll)));

    if (updates == 0) {
        std::fprintf(stderr, "no state update arrived\n");
        return 1;
    }
    if (!found) {
        std::fprintf(stderr, "no state update named an aircraft of this client's\n");
        return 1;
    }
    return 0;
}
