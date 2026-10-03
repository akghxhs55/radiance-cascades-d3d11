#include "Profiler.h"

#include "D3DUtils.h"

#include <cassert>

namespace
{
    [[nodiscard]] Microsoft::WRL::ComPtr<ID3D11Query> CreateQuery(
        ID3D11Device* const device,
        D3D11_QUERY const query)
    {
        D3D11_QUERY_DESC const description{ .Query = query };
        Microsoft::WRL::ComPtr<ID3D11Query> result;
        ThrowIfFailed(
            device->CreateQuery(&description, result.GetAddressOf()),
            "ID3D11Device::CreateQuery failed"
        );
        return result;
    }
}

Profiler::Profiler(ID3D11Device* const device)
{
    assert(device != nullptr);

    for (GpuFrame& frame : gpuFrames)
    {
        frame.disjointQuery = CreateQuery(device, D3D11_QUERY_TIMESTAMP_DISJOINT);
        for (std::size_t metricIndex = 0; metricIndex < MetricCount; ++metricIndex)
        {
            frame.beginQueries[metricIndex] = CreateQuery(device, D3D11_QUERY_TIMESTAMP);
            frame.endQueries[metricIndex] = CreateQuery(device, D3D11_QUERY_TIMESTAMP);
        }
    }
}

void Profiler::BeginFrame(ID3D11DeviceContext* const deviceContext)
{
    assert(deviceContext != nullptr);
    assert(!frameActive);

    CollectCompletedGpuFrames(deviceContext);

    frameActive = true;
    gpuFrameEnded = false;
    cpuFrameStart = std::chrono::steady_clock::now();
    cpuScopeActive.fill(false);

    GpuFrame& candidate = gpuFrames[nextGpuFrameIndex];
    nextGpuFrameIndex = (nextGpuFrameIndex + 1) % BufferedFrameCount;

    activeGpuFrame = candidate.pending ? nullptr : &candidate;
    if (activeGpuFrame != nullptr)
    {
        activeGpuFrame->measured.fill(false);
        deviceContext->Begin(activeGpuFrame->disjointQuery.Get());
    }
}

void Profiler::EndGpuFrame(ID3D11DeviceContext* const deviceContext)
{
    assert(deviceContext != nullptr);
    assert(frameActive);
    assert(!gpuFrameEnded);

    for (bool const scopeActive : cpuScopeActive)
    {
        assert(!scopeActive && "Every profiler scope must end before the GPU frame ends");
    }

    if (activeGpuFrame != nullptr)
    {
        deviceContext->End(activeGpuFrame->disjointQuery.Get());
        activeGpuFrame->pending = true;
        activeGpuFrame = nullptr;
    }

    gpuFrameEnded = true;
}

void Profiler::EndFrame()
{
    assert(frameActive);
    assert(gpuFrameEnded);

    cpuFrameMilliseconds = std::chrono::duration<double, std::milli>(
        std::chrono::steady_clock::now() - cpuFrameStart
    ).count();
    frameActive = false;
}

void Profiler::BeginScope(ProfileMetric const metric, ID3D11DeviceContext* const deviceContext)
{
    assert(deviceContext != nullptr);
    if (!frameActive)
    {
        return;
    }

    assert(frameActive && !gpuFrameEnded);

    std::size_t const metricIndex = ToIndex(metric);
    assert(!cpuScopeActive[metricIndex]);

    cpuScopeStarts[metricIndex] = std::chrono::steady_clock::now();
    cpuScopeActive[metricIndex] = true;

    if (activeGpuFrame != nullptr)
    {
        deviceContext->End(activeGpuFrame->beginQueries[metricIndex].Get());
    }
}

void Profiler::EndScope(ProfileMetric const metric, ID3D11DeviceContext* const deviceContext)
{
    assert(deviceContext != nullptr);
    if (!frameActive)
    {
        return;
    }

    assert(frameActive && !gpuFrameEnded);

    std::size_t const metricIndex = ToIndex(metric);
    assert(cpuScopeActive[metricIndex]);

    timings[metricIndex].cpuMilliseconds = std::chrono::duration<double, std::milli>(
        std::chrono::steady_clock::now() - cpuScopeStarts[metricIndex]
    ).count();
    cpuScopeActive[metricIndex] = false;

    if (activeGpuFrame != nullptr)
    {
        deviceContext->End(activeGpuFrame->endQueries[metricIndex].Get());
        activeGpuFrame->measured[metricIndex] = true;
    }
}

double Profiler::GetCpuFrameMilliseconds() const noexcept
{
    return cpuFrameMilliseconds;
}

Profiler::MetricTiming Profiler::GetMetricTiming(ProfileMetric const metric) const noexcept
{
    return timings[ToIndex(metric)];
}

void Profiler::CollectCompletedGpuFrames(ID3D11DeviceContext* const deviceContext)
{
    for (GpuFrame& frame : gpuFrames)
    {
        if (!frame.pending)
        {
            continue;
        }

        D3D11_QUERY_DATA_TIMESTAMP_DISJOINT disjoint{};
        if (deviceContext->GetData(
            frame.disjointQuery.Get(),
            &disjoint,
            sizeof(disjoint),
            D3D11_ASYNC_GETDATA_DONOTFLUSH
        ) != S_OK)
        {
            continue;
        }

        if (!disjoint.Disjoint)
        {
            for (std::size_t metricIndex = 0; metricIndex < MetricCount; ++metricIndex)
            {
                if (!frame.measured[metricIndex])
                {
                    continue;
                }

                UINT64 beginTimestamp = 0;
                UINT64 endTimestamp = 0;
                HRESULT const beginResult = deviceContext->GetData(
                    frame.beginQueries[metricIndex].Get(),
                    &beginTimestamp,
                    sizeof(beginTimestamp),
                    D3D11_ASYNC_GETDATA_DONOTFLUSH
                );
                HRESULT const endResult = deviceContext->GetData(
                    frame.endQueries[metricIndex].Get(),
                    &endTimestamp,
                    sizeof(endTimestamp),
                    D3D11_ASYNC_GETDATA_DONOTFLUSH
                );

                if (beginResult == S_OK && endResult == S_OK)
                {
                    timings[metricIndex].gpuMilliseconds = static_cast<double>(endTimestamp - beginTimestamp)
                        * 1000.0 / static_cast<double>(disjoint.Frequency);
                    timings[metricIndex].gpuValid = true;
                }
            }
        }

        frame.pending = false;
    }
}

std::size_t Profiler::ToIndex(ProfileMetric const metric) noexcept
{
    return static_cast<std::size_t>(metric);
}
