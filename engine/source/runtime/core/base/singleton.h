#pragma once

#include <type_traits>

namespace Hybrid
{

    template<typename T>
    class Singleton
    {
    protected:
        Singleton() = default;

    public:
        static T& getInstance() noexcept(std::is_nothrow_constructible<T>::value)
        {
            static T instance;
            return instance;
        }
        virtual ~Singleton() noexcept = default;
        Singleton(const Singleton&) = delete;
        Singleton& operator=(const Singleton&) = delete;
    };
}
