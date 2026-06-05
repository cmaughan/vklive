#pragma once

// Result<T, E> — a small, hand-rolled value-or-error type for Draxul.
//
// Motivation: the codebase historically used three incompatible error-handling
// patterns (bool + .error() accessor, std::optional<T>, and silent-fail void).
// `Result<T, E>` replaces those with a single type that makes error cases
// impossible to ignore at call sites.
//
// We would prefer `std::expected<T, E>`, but the project baseline is C++20
// (`CMAKE_CXX_STANDARD 20`) and `<expected>` is a C++23 addition that is not
// yet universally available across the Clang/MSVC versions we target. This
// header is intentionally small (value-or-error, no monadic `and_then` /
// `or_else` yet) — we can drop it in favour of a `std::expected` alias once
// we bump the standard.
//
// Usage:
//   Result<Foo, Error> make_foo();
//
//   if (auto r = make_foo()) {
//       use(*r);                 // or r.value()
//   } else {
//       log(r.error().message);
//   }
//
// Conversion to bool is implicit via `explicit operator bool` (contextual
// conversion), so existing call sites that use `if (spawn(...))` or
// `REQUIRE(spawn(...))` keep compiling after a `bool` -> `Result<void, Error>`
// migration.

#include <cassert>
#include <string>
#include <type_traits>
#include <utility>
#include <variant>

namespace vklive_nvim
{

// --- Error category --------------------------------------------------------
//
// We keep a single project-wide `Error` type with a coarse category enum and
// a free-form message. This is deliberately minimal: per-subsystem error
// hierarchies proliferate quickly and the review that motivated WI 24 called
// out the inconsistency, not a lack of richness. If a call site needs more
// structured data it can wrap this in its own struct.

enum class ErrorKind
{
    Unknown = 0,
    // Process / OS
    SpawnFailed,
    IoError,
    // Configuration
    ConfigLoadFailed,
    ConfigParseFailed,
    ConfigApplyFailed,
    // Neovim RPC
    RpcError,
    // Rendering / GPU
    InitFailed,
    AtlasOverflow,
    // Invalid arguments / preconditions
    InvalidArgument,
    NotFound,
};

struct Error
{
    ErrorKind kind = ErrorKind::Unknown;
    std::string message;

    Error() = default;
    Error(ErrorKind k, std::string msg)
        : kind(k)
        , message(std::move(msg))
    {
    }

    // Convenience builder so call sites can write `Error::io("open failed")`.
    static Error io(std::string msg)
    {
        return { ErrorKind::IoError, std::move(msg) };
    }
    static Error spawn(std::string msg)
    {
        return { ErrorKind::SpawnFailed, std::move(msg) };
    }
    static Error config_load(std::string msg)
    {
        return { ErrorKind::ConfigLoadFailed, std::move(msg) };
    }
    static Error config_parse(std::string msg)
    {
        return { ErrorKind::ConfigParseFailed, std::move(msg) };
    }
    static Error config_apply(std::string msg)
    {
        return { ErrorKind::ConfigApplyFailed, std::move(msg) };
    }
    static Error rpc(std::string msg)
    {
        return { ErrorKind::RpcError, std::move(msg) };
    }
    static Error init(std::string msg)
    {
        return { ErrorKind::InitFailed, std::move(msg) };
    }
    static Error invalid_argument(std::string msg)
    {
        return { ErrorKind::InvalidArgument, std::move(msg) };
    }
    static Error not_found(std::string msg)
    {
        return { ErrorKind::NotFound, std::move(msg) };
    }
};

// --- Result<T, E> ----------------------------------------------------------

// Tag type for constructing error results without ambiguity when T == E.
struct ErrorTag
{
};
inline constexpr ErrorTag kErrorTag{};

// Tag type used in the Result<void, E> specialization to construct an "ok".
struct OkTag
{
};
inline constexpr OkTag kOkTag{};

template <typename T, typename E = Error>
class Result
{
public:
    using value_type = T;
    using error_type = E;

    // Default-constructed Result is in the error state with a default-
    // constructed E. Lets call sites declare a Result variable up front and
    // assign into it later (common in worker-thread test fixtures), without
    // needing a placeholder value of T.
    Result()
        : storage_(std::in_place_index<1>, E{})
    {
    }

    // Implicit construction from T — lets `return Foo{...};` just work.
    Result(T value)
        : storage_(std::in_place_index<0>, std::move(value))
    {
    }

    // Explicit error construction via tag to avoid ambiguity.
    Result(ErrorTag, E error)
        : storage_(std::in_place_index<1>, std::move(error))
    {
    }

    // Static factory helpers — the intended "pretty" spelling at call sites.
    static Result ok(T value)
    {
        return Result(std::move(value));
    }
    static Result err(E error)
    {
        return Result(kErrorTag, std::move(error));
    }

    bool has_value() const noexcept
    {
        return storage_.index() == 0;
    }
    explicit operator bool() const noexcept
    {
        return has_value();
    }

    T& value() &
    {
        assert(has_value() && "Result::value() on error result");
        return std::get<0>(storage_);
    }
    const T& value() const&
    {
        assert(has_value() && "Result::value() on error result");
        return std::get<0>(storage_);
    }
    T&& value() &&
    {
        assert(has_value() && "Result::value() on error result");
        return std::move(std::get<0>(storage_));
    }

    T& operator*() &
    {
        return value();
    }
    const T& operator*() const&
    {
        return value();
    }
    T&& operator*() &&
    {
        return std::move(*this).value();
    }

    T* operator->()
    {
        return &value();
    }
    const T* operator->() const
    {
        return &value();
    }

    E& error() &
    {
        assert(!has_value() && "Result::error() on ok result");
        return std::get<1>(storage_);
    }
    const E& error() const&
    {
        assert(!has_value() && "Result::error() on ok result");
        return std::get<1>(storage_);
    }
    E&& error() &&
    {
        assert(!has_value() && "Result::error() on ok result");
        return std::move(std::get<1>(storage_));
    }

    // Return value or a fallback — handy for "don't care why" call sites.
    template <typename U>
    T value_or(U&& fallback) const&
    {
        return has_value() ? value() : static_cast<T>(std::forward<U>(fallback));
    }

private:
    std::variant<T, E> storage_;
};

// --- Result<void, E> specialization ---------------------------------------

template <typename E>
class Result<void, E>
{
public:
    using value_type = void;
    using error_type = E;

    // Default-constructed Result<void, E> is "ok".
    Result()
        : has_value_(true)
    {
    }
    Result(OkTag)
        : has_value_(true)
    {
    }
    Result(ErrorTag, E error)
        : has_value_(false)
        , error_(std::move(error))
    {
    }

    static Result ok()
    {
        return Result(kOkTag);
    }
    static Result err(E error)
    {
        return Result(kErrorTag, std::move(error));
    }

    bool has_value() const noexcept
    {
        return has_value_;
    }
    explicit operator bool() const noexcept
    {
        return has_value_;
    }

    void value() const
    {
        assert(has_value_ && "Result::value() on error result");
    }

    E& error() &
    {
        assert(!has_value_ && "Result::error() on ok result");
        return error_;
    }
    const E& error() const&
    {
        assert(!has_value_ && "Result::error() on ok result");
        return error_;
    }
    E&& error() &&
    {
        assert(!has_value_ && "Result::error() on ok result");
        return std::move(error_);
    }

private:
    bool has_value_ = true;
    E error_{};
};

// Convenience: the overwhelmingly common case uses `vklive_nvim::Error`.
using VoidResult = Result<void, Error>;

} // namespace vklive_nvim
