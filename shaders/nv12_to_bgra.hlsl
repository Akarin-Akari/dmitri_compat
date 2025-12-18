// NV12 to BGRA Compute Shader
// 替代 DmitriRender 的 CUDA kernel

// 输入纹理 (NV12 格式)
Texture2D<float> texY : register(t0);      // Y 平面 (全分辨率)
Texture2D<float2> texUV : register(t1);    // UV 平面 (半分辨率，交织)

// 输出纹理 (BGRA)
RWTexture2D<float4> outputTex : register(u0);

// 采样器
SamplerState linearSampler : register(s0);

// BT.709 YUV 到 RGB 转换矩阵 (视频标准)
static const float3x3 YUVtoRGB = float3x3(
    1.0,  0.0,      1.5748,     // R
    1.0, -0.1873,  -0.4681,     // G
    1.0,  1.8556,   0.0         // B
);

// BT.601 备用矩阵 (旧标准)
static const float3x3 YUVtoRGB_601 = float3x3(
    1.0,  0.0,      1.402,      // R
    1.0, -0.344,   -0.714,      // G
    1.0,  1.772,    0.0         // B
);

[numthreads(16, 16, 1)]
void main(uint3 DTid : SV_DispatchThreadID)
{
    // 获取输出纹理尺寸
    uint width, height;
    outputTex.GetDimensions(width, height);
    
    // 边界检查
    if (DTid.x >= width || DTid.y >= height)
        return;
    
    // 计算 UV 坐标 (归一化)
    float2 uv = float2(DTid.x + 0.5, DTid.y + 0.5) / float2(width, height);
    
    // 采样 Y (全分辨率)
    float Y = texY.SampleLevel(linearSampler, uv, 0);
    
    // 采样 UV (半分辨率，需要考虑 chroma 位置)
    float2 UV = texUV.SampleLevel(linearSampler, uv, 0);
    float U = UV.x - 0.5;  // 中心化到 [-0.5, 0.5]
    float V = UV.y - 0.5;
    
    // YUV 到 RGB 转换 (BT.709)
    float3 yuv = float3(Y, U, V);
    float3 rgb = mul(YUVtoRGB, yuv);
    
    // 钳制到 [0, 1]
    rgb = saturate(rgb);
    
    // 输出 BGRA (注意顺序：B, G, R, A)
    outputTex[DTid.xy] = float4(rgb.b, rgb.g, rgb.r, 1.0);
}

// 备用版本：直接读取像素（无采样器）
[numthreads(16, 16, 1)]
void mainDirect(uint3 DTid : SV_DispatchThreadID)
{
    uint width, height;
    outputTex.GetDimensions(width, height);
    
    if (DTid.x >= width || DTid.y >= height)
        return;
    
    // 直接读取 Y
    float Y = texY[DTid.xy];
    
    // UV 在半分辨率位置
    uint2 uvPos = DTid.xy / 2;
    float2 UV = texUV[uvPos];
    float U = UV.x - 0.5;
    float V = UV.y - 0.5;
    
    // YUV 到 RGB
    float3 yuv = float3(Y, U, V);
    float3 rgb = mul(YUVtoRGB, yuv);
    rgb = saturate(rgb);
    
    outputTex[DTid.xy] = float4(rgb.b, rgb.g, rgb.r, 1.0);
}
