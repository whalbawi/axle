#pragma once

#include <cstdint>

#include <concepts>
#include <string>
#include <utility>
#include <variant>

namespace axle {

enum class None : uint8_t {};

template <typename T>
struct Ok {
    T v;
};

template <typename T>
Ok(T) -> Ok<T>;

Ok() -> Ok<None>;

template <typename E>
struct Err {
    E v;
};

template <typename E>
Err(E) -> Err<E>;

Err() -> Err<None>;

template <typename T = None, typename E = None>
class Status {
    using variant_type = std::variant<T, E>;

  public:
    template <typename U>
        requires std::convertible_to<U, T>
    Status(Ok<U>&& ok); // NOLINT(google-explicit-constructor,)

    template <typename U>
        requires std::convertible_to<U, E>
    Status(Err<U>&& err); // NOLINT(google-explicit-constructor)

    bool is_ok() const;

    bool is_err() const;

    T ok()
        requires(!std::same_as<T, None>);

    E err()
        requires(!std::same_as<E, None>);

    std::string err()
        requires(std::same_as<E, None>);

    std::string ok()
        requires(std::same_as<T, None>);

  private:
    Status() = delete;

    explicit Status(variant_type state);

    variant_type state_;
};

template <typename T>
Status(Ok<T>) -> Status<T, None>;

template <typename E>
Status(Err<E>) -> Status<None, E>;

template <typename T, typename E>
template <typename U>
    requires std::convertible_to<U, T>
Status<T, E>::Status(Ok<U>&& ok) : Status(variant_type(std::in_place_index<0>, std::move(ok).v)) {}

template <typename T, typename E>
template <typename U>
    requires std::convertible_to<U, E>
Status<T, E>::Status(Err<U>&& err)
    : Status(variant_type(std::in_place_index<1>, std::move(err).v)) {}

template <typename T, typename E>
Status<T, E>::Status(variant_type state) : state_(std::move(state)) {}

template <typename T, typename E>
bool Status<T, E>::is_ok() const {
    return state_.index() == 0;
}

template <typename T, typename E>
bool Status<T, E>::is_err() const {
    return state_.index() == 1;
}

template <typename T, typename E>
T Status<T, E>::ok()
    requires(!std::same_as<T, None>)
{
    return std::move(std::get<0>(state_));
}

template <typename T, typename E>
E Status<T, E>::err()
    requires(!std::same_as<E, None>)
{
    return std::move(std::get<1>(state_));
}

template <typename T, typename E>
std::string Status<T, E>::ok()
    requires(std::same_as<T, None>)
{

    return "(nil)";
}

template <typename T, typename E>
std::string Status<T, E>::err()
    requires(std::same_as<E, None>)
{
    return "(nil)";
}

} // namespace axle
