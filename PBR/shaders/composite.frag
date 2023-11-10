#version 460 core

layout(location = 0) out vec4 OutputColor;

layout(location = 0) in vec2 m_Uv;

layout (set = 0, binding = 0) uniform sampler2D u_Color;

// FXAA is an anti-aliasing algorithm made by preprocessing
// Mostly implemented based on this piece of code: https://github.com/McNopper/OpenGL/blob/master/Example42/shader/fxaa.frag.glsl
// Also this article was pretty helpfull https://developer.download.nvidia.com/assets/gamedev/files/sdk/11/FXAA_WhitePaper.pdf

float FXAA_To_LUMA(vec3 rgb)
{
    return dot(rgb, vec3(0.299, 0.587, 0.114));
}
vec2 FXAA_CalculateSamplingDirection(float lumaNW, float lumaNE, float lumaSW, float lumaSE, float mulReduce, float minReduce, float maxSpan, vec2 texelStep)
{
    vec2 samplingDirection;	
	samplingDirection.x = -((lumaNW + lumaNE) - (lumaSW + lumaSE));
    samplingDirection.y =  ((lumaNW + lumaSW) - (lumaNE + lumaSE));
    float samplingDirectionReduce = max((lumaNW + lumaNE + lumaSW + lumaSE) * 0.25 * mulReduce, minReduce);
    float minSamplingDirectionFactor = 1.0 / (min(abs(samplingDirection.x), abs(samplingDirection.y)) + samplingDirectionReduce);
    return samplingDirection = clamp(samplingDirection * minSamplingDirectionFactor, vec2(-maxSpan), vec2(maxSpan)) * texelStep;
}

vec3 FXAA_EstimateHowManySamplesToUse(float lumaMin, float lumaMax, vec3 rgbTwoTab, vec3 rgbFourTab)
{
    float lumaFourTab = FXAA_To_LUMA(rgbFourTab);

    if (lumaFourTab < lumaMin || lumaFourTab > lumaMax)
	{
		return vec3(rgbTwoTab); 
	}
	else
	{
		return vec3(rgbFourTab);
	}
}

vec2 FXAA_CalculateLumaMinMax(float lumaThreshold, float lumaM, float lumaNW, float lumaNE, float lumaSW, float lumaSE)
{
    vec2 result = vec2(0.0, 0.0);
    result.x = min(lumaM, min(min(lumaNW, lumaNE), min(lumaSW, lumaSE)));
	result.y = max(lumaM, max(max(lumaNW, lumaNE), max(lumaSW, lumaSE)));
    return result;
}

vec3 FXAA(vec3 rgbM, sampler2D tex, vec2 uv, float lumaThreshold, float mulReduce, float minReduce, float maxSpan, vec2 texelStep)
{
    vec3 rgbNW = textureOffset(tex, uv, ivec2(-1, 1)).rgb;
    vec3 rgbNE = textureOffset(tex, uv, ivec2(1, 1)).rgb;
    vec3 rgbSW = textureOffset(tex, uv, ivec2(-1, -1)).rgb;
    vec3 rgbSE = textureOffset(tex, uv, ivec2(1, -1)).rgb;

	float lumaNW = FXAA_To_LUMA(rgbNW);
	float lumaNE = FXAA_To_LUMA(rgbNE);
	float lumaSW = FXAA_To_LUMA(rgbSW);
	float lumaSE = FXAA_To_LUMA(rgbSE);
	float lumaM = FXAA_To_LUMA(rgbM);

    vec2 lumaMinMax = FXAA_CalculateLumaMinMax(lumaThreshold, lumaM, lumaNW, lumaNE, lumaSW, lumaSE);

    if (lumaMinMax.y - lumaMinMax.x <= lumaMinMax.y * lumaThreshold)
	{
		return rgbM;
	}

    vec2 samplingDirection = FXAA_CalculateSamplingDirection(lumaNW, lumaNE, lumaSW, lumaSE, mulReduce, minReduce, maxSpan, texelStep);

	vec3 rgbSampleNeg = texture(tex, uv + samplingDirection * (1.0/3.0 - 0.5)).rgb;
	vec3 rgbSamplePos = texture(tex, uv + samplingDirection * (2.0/3.0 - 0.5)).rgb;

    vec3 rgbTwoTab = (rgbSamplePos + rgbSampleNeg) * 0.5;

    vec3 rgbSampleNegOuter = texture(tex, uv + samplingDirection * (0.0/3.0 - 0.5)).rgb;
	vec3 rgbSamplePosOuter = texture(tex, uv + samplingDirection * (3.0/3.0 - 0.5)).rgb;

    vec3 rgbFourTab = (rgbSamplePosOuter + rgbSampleNegOuter) * 0.25 + rgbTwoTab * 0.5;

    vec3 result = FXAA_EstimateHowManySamplesToUse(lumaMinMax.x, lumaMinMax.y, rgbTwoTab, rgbFourTab);
    return result;
}

// Hill ACES computation cost
// Seen it in one of Acerola videos: https://www.youtube.com/watch?v=wbn5ULLtkHs it should be shown at 6:16
vec3 ACES_Hill_Tonemapping(vec3 color)
{
	mat3 acesInput = mat3(
		0.59719, 0.07600, 0.02840,
		0.35458, 0.90834, 0.13383,
		0.04823, 0.01566, 0.83777
	);
	mat3 acesOutput = mat3(
		1.60475, -0.10208, -0.00327,
		-0.53108, 1.10813, -0.07276,
		-0.07367, -0.00605, 1.07602
	);
	vec3 v = acesInput * color;
	vec3 a = v * (v + 0.0245786) - 0.000090537;
	vec3 b = v * (0.983729 * v + 0.4329510) + 0.238081;
	return clamp(acesOutput * (a / b), 0.0, 1.0);
}

vec3 GammaCorrect(vec3 color, float gamma)
{
	return pow(color, vec3(1.0f / gamma));
}

layout(push_constant) uniform Constant
{
    vec2 Resolution;
} u_Constant;

const float gamma = 2.2;
const int step_size = 1;

void main()
{
	vec3 color = texture(u_Color, m_Uv).rgb;

	vec2 texelStep = vec2(1.0 / u_Constant.Resolution.x, 1.0 / u_Constant.Resolution.y);
	float lumaThreshold = 0.5;
	float mulReduce = 1.0 / 8.0;
	float minReduce = 1.0 / 128.0;
	float maxSpan = 8.0;
	color = FXAA(color, u_Color, m_Uv, lumaThreshold, mulReduce, minReduce, maxSpan, texelStep);

	color = ACES_Hill_Tonemapping(color);
	color = GammaCorrect(color, gamma);
    OutputColor = vec4(color, 1.0);
}