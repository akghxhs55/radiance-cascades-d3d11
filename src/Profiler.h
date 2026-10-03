#pragma once

#include <d3d11.h>
#include <wrl/client.h>

#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>

enum class ProfileMetric : std::size_t
{
    DistanceField,
    RadianceCascades,
    FinalGather,
    Count,
};

class Profiler final
{
public:
    static constexpr std::size_t MetricCount = static_cast<std::size_t>(ProfileMetric::Count);

    struct MetricTiming
    {
        double cpuMilliseconds = 0.0;
        double gpuMilliseconds = 0.0;
        bool gpuValid = false;
    };

    explicit Profiler(ID3D11Device* device);

    Profiler(Profiler const&) = delete;
    Profiler& operator=(Profiler const&) = delete;

    void BeginFrame(ID3D11DeviceContext* deviceContext);
    void EndGpuFrame(ID3D11DeviceContext* deviceContext);
    void EndFrame();

    void BeginScope(ProfileMetric metric, ID3D11DeviceContext* deviceContext);
    void EndScope(ProfileMetric metric, ID3D11DeviceContext* deviceContext);

    [[nodiscard]] double GetCpuFrameMilliseconds() const noexcept;
    [[nodiscard]] MetricTiming GetMetricTiming(ProfileMetric metric) const noexcept;

private:
    static constexpr std::size_t BufferedFrameCount = 4;

    struct GpuFrame
    {
        Microsoft::WRL::ComPtr<ID3D11Query> disjointQuery;
        std::array<Microsoft::WRL::ComPtr<ID3D11Query>, MetricCount> beginQueries;
        std::array<Microsoft::WRL::ComPtr<ID3D11Query>, MetricCount> endQueries;
        std::array<bool, MetricCount> measured{};
        bool pending = false;
    };

    void CollectCompletedGpuFrames(ID3D11DeviceContext* deviceContext);
    [[nodiscard]] static std::size_t ToIndex(ProfileMetric metric) noexcept;

    std::array<GpuFrame, BufferedFrameCount> gpuFrames{};
    std::array<MetricTiming, MetricCount> timings{};
    std::array<std::chrono::steady_clock::time_point, MetricCount> cpuScopeStarts{};
    std::array<bool, MetricCount> cpuScopeActive{};

    std::chrono::steady_clock::time_point cpuFrameStart{};
    double cpuFrameMilliseconds = 0.0;
    std::size_t nextGpuFrameIndex = 0;
    GpuFrame* activeGpuFrame = nullptr;
    bool frameActive = false;
    bool gpuFrameEnded = false;
};
