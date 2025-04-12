cbuffer FrustumPlanes : register(b0)
{
    float4 planes[6];
};

struct InstanceData
{
    float4x4 model;
    uint     textureIndex;
    uint     numInstances;
    float2   padding;
};

StructuredBuffer<InstanceData> instanceData : register(t0);
RWByteAddressBuffer          indirectArgs : register(u0);
RWStructuredBuffer<uint>     objectIds    : register(u1);

bool IsAABBInFrustum(in float3 bboxCenter, in float extent)
{
    for (int planeIdx = 0; planeIdx < 6; ++planeIdx)
    {
        float distanceVal = dot(planes[planeIdx].xyz, bboxCenter) + planes[planeIdx].w;
        float radiusVal   = extent * (abs(planes[planeIdx].x) + abs(planes[planeIdx].y) + abs(planes[planeIdx].z));
        if (distanceVal + radiusVal < 0)
            return false;
    }
    return true;
}

[numthreads(64, 1, 1)]
void main(uint3 globalThreadId : SV_DispatchThreadID)
{
    uint totalInstances = instanceData[0].numInstances;
    if (globalThreadId.x >= totalInstances)
        return;

    float3 instancePos = instanceData[globalThreadId.x].model._m03_m13_m23;
    float boundingExtent = 0.5f * 0.95f;

    if (IsAABBInFrustum(instancePos, boundingExtent))
    {
        uint newIndex;
        indirectArgs.InterlockedAdd(4, 1, newIndex);
        objectIds[newIndex] = globalThreadId.x;
    }
}
