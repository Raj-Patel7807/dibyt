#pragma once

#include <string>
#include <compare>
#include <functional>

namespace dibyt::core {

// Strong typed ID template to prevent primitive obsession and accidental argument swapping
template <typename Tag>
class TypedId {
public:
    TypedId() = default;
    explicit TypedId(std::string value) : value_(std::move(value)) {}

    [[nodiscard]] const std::string& value() const noexcept { return value_; }
    [[nodiscard]] bool empty() const noexcept { return value_.empty(); }

    auto operator<=>(const TypedId&) const = default;
    bool operator==(const TypedId&) const = default;

private:
    std::string value_;
};

struct CollectionTag {};
struct FileTag {};
struct SessionTag {};
struct TaskTag {};

using CollectionId = TypedId<CollectionTag>;
using FileId = TypedId<FileTag>;
using SessionId = TypedId<SessionTag>;
using TaskId = TypedId<TaskTag>;

} // namespace dibyt::core

namespace std {
template <typename Tag>
struct hash<dibyt::core::TypedId<Tag>> {
    size_t operator()(const dibyt::core::TypedId<Tag>& id) const noexcept {
        return hash<string>{}(id.value());
    }
};
} // namespace std
