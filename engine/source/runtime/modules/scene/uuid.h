#pragma once
#include <cstdint>
#include <random>

namespace Hybrid
{
    struct UUID
    {
        uint64_t value = 0;

        UUID() = default;
        explicit UUID(uint64_t v) : value(v) {}

        explicit operator uint64_t() const { return value; }

        bool operator==(const UUID& rhs) const { return value == rhs.value; }
        bool operator!=(const UUID& rhs) const { return value != rhs.value; }
    };

    struct UUIDHash
    {
        size_t operator()(const UUID& id) const noexcept
        {
            return std::hash<uint64_t>{}(id.value);
        }
    };

    class UUIDGenerator
    {
    public:
        static UUID New()
        {
            static std::mt19937_64 rng{ std::random_device{}() };
            static std::uniform_int_distribution<uint64_t> dist(1, UINT64_MAX);
            return UUID{ dist(rng) };
        }
    };
} // namespace Hybrid
