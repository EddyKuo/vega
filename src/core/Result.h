#pragma once
#include <variant>
#include <utility>

namespace vega
{

/// Lightweight Result<T, E> type for error handling without exceptions.
template <typename T, typename E>
class Result
{
public:
    static Result ok(T value) { return Result{std::move(value)}; }
    static Result err(E error) { return Result{std::move(error)}; }

    bool is_ok() const { return std::holds_alternative<T>(data_); }
    bool is_err() const { return std::holds_alternative<E>(data_); }

    T& value() { return std::get<T>(data_); }
    const T& value() const { return std::get<T>(data_); }

    E& error() { return std::get<E>(data_); }
    const E& error() const { return std::get<E>(data_); }

    explicit operator bool() const { return is_ok(); }

private:
    explicit Result(T value) : data_{std::move(value)} {}
    explicit Result(E error) : data_{std::move(error)} {}

    std::variant<T, E> data_;
};

} // namespace vega
