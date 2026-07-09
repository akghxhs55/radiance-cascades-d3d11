#pragma once

#include <stdexcept>
#include <string>
#include <string_view>
#include <Windows.h>

class D3DException : public std::runtime_error
{
public:
    D3DException(std::string_view operation, HRESULT result)
        : std::runtime_error(std::string(operation))
        , result(result)
    {
    }

    HRESULT GetResult() const noexcept
    {
        return result;
    }

private:
    HRESULT result;
};

inline void ThrowIfFailed(HRESULT const result, std::string_view const operation)
{
    if (FAILED(result))
    {
        throw D3DException(operation, result);
    }
}
