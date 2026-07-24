#pragma once

#include <d3dcommon.h>
#include <iostream>
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

inline void OutputBlob(ID3DBlob* const blob)
{
    if (blob == nullptr || blob->GetBufferSize() == 0)
    {
        return;
    }

    auto const* const text = static_cast<char const*>(blob->GetBufferPointer());
    auto const size = static_cast<std::streamsize>(blob->GetBufferSize());

    std::cerr.write(text, size);
    std::cerr.flush();
}
