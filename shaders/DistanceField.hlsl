Texture2D<float> Obstacle : register(t0);
Texture2D<uint> InputSeed : register(t1);
RWTexture2D<uint> OutputSeed : register(u0);
RWTexture2D<float> DistanceField : register(u1);

cbuffer DistanceFieldConstants : register(b0)
{
	uint2 SceneSize;
	uint JumpSize;
	uint Padding;
};

float DistanceSquared(uint2 a, uint2 b)
{
	float2 diff = float2(a) - float2(b);
	return dot(diff, diff);
}

[numthreads(8, 8, 1)]
void CSInit(uint3 threadId : SV_DispatchThreadID)
{
	uint2 texel = threadId.xy;
	if (any(texel >= SceneSize))
	{
		return;
	}

	if (Obstacle.Load(int3(texel, 0)) > 0.5)
	{
		OutputSeed[texel] = (texel.y << 16) | texel.x;
	}
	else
	{
		OutputSeed[texel] = 0xFFFFFFFF;
	}
}

[numthreads(8, 8, 1)]
void CSJumpFlood(uint3 threadId : SV_DispatchThreadID)
{
	uint2 texel = threadId.xy;
	if (any(texel >= SceneSize))
	{
		return;
	}

	uint bestSeed = InputSeed.Load(int3(texel, 0));
	float bestDistanceSquared = 3.402823466e+38f;

    if (bestSeed != 0xFFFFFFFF)
    {
        uint2 bestPosition = uint2(bestSeed & 0xFFFF, bestSeed >> 16);
        bestDistanceSquared = DistanceSquared(texel, bestPosition);
    }
	for (int dx = -1; dx <= 1; ++dx)
	{
		for (int dy = -1; dy <= 1; ++dy)
		{
			if (dx == 0 && dy == 0)
			{
				continue;
			}

			int2 neighborTexel = int2(texel) + int2(dx, dy) * int(JumpSize);
			if (any(neighborTexel < 0) || any(neighborTexel >= int2(SceneSize)))
			{
				continue;
			}

			uint neighborSeed = InputSeed.Load(int3(uint2(neighborTexel), 0));
			if (neighborSeed == 0xFFFFFFFF)
			{
				continue;
			}

			uint2 neighborPos = uint2(neighborSeed & 0xFFFF, neighborSeed >> 16);
			float neighborDistanceSquared = DistanceSquared(texel, neighborPos);

			if (neighborDistanceSquared < bestDistanceSquared)
			{
				bestDistanceSquared = neighborDistanceSquared;
				bestSeed = neighborSeed;
			}
		}
	}

	OutputSeed[texel] = bestSeed;
}

[numthreads(8, 8, 1)]
void CSFinalize(uint3 threadId : SV_DispatchThreadID)
{
	uint2 texel = threadId.xy;
	if (any(texel >= SceneSize))
	{
		return;
	}

	uint seed = InputSeed.Load(int3(texel, 0));
	if (seed == 0xFFFFFFFF)
	{
		DistanceField[texel] = length(float2(SceneSize));
	}
	else
	{
		uint2 seedPos = uint2(seed & 0xFFFF, seed >> 16);
		float distance = length(float2(texel) - float2(seedPos));
		DistanceField[texel] = distance;
	}
}
