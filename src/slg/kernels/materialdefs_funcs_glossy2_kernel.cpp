#include <string>
namespace slg { namespace ocl {
std::string KernelSource_materialdefs_funcs_glossy2 = 
"#line 2 \"materialdefs_funcs_glossy2.cl\"\n"
"\n"
"/***************************************************************************\n"
" * Copyright 1998-2013 by authors (see AUTHORS.txt)                        *\n"
" *                                                                         *\n"
" *   This file is part of LuxRender.                                       *\n"
" *                                                                         *\n"
" * Licensed under the Apache License, Version 2.0 (the \"License\");         *\n"
" * you may not use this file except in compliance with the License.        *\n"
" * You may obtain a copy of the License at                                 *\n"
" *                                                                         *\n"
" *     http://www.apache.org/licenses/LICENSE-2.0                          *\n"
" *                                                                         *\n"
" * Unless required by applicable law or agreed to in writing, software     *\n"
" * distributed under the License is distributed on an \"AS IS\" BASIS,       *\n"
" * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.*\n"
" * See the License for the specific language governing permissions and     *\n"
" * limitations under the License.                                          *\n"
" ***************************************************************************/\n"
"\n"
"//------------------------------------------------------------------------------\n"
"// Glossy2 material\n"
"//\n"
"// LuxRender Glossy2 material porting.\n"
"//------------------------------------------------------------------------------\n"
"\n"
"#if defined (PARAM_ENABLE_MAT_GLOSSY2)\n"
"\n"
"BSDFEvent Glossy2Material_GetEventTypes() {\n"
"	return GLOSSY | REFLECT;\n"
"}\n"
"\n"
"bool Glossy2Material_IsDelta() {\n"
"	return false;\n"
"}\n"
"\n"
"#if defined(PARAM_HAS_PASSTHROUGH)\n"
"float3 Glossy2Material_GetPassThroughTransparency(__global Material *material,\n"
"		__global HitPoint *hitPoint, const float3 localFixedDir, const float passThroughEvent\n"
"		TEXTURES_PARAM_DECL) {\n"
"	return BLACK;\n"
"}\n"
"#endif\n"
"\n"
"float SchlickBSDF_CoatingWeight(const float3 ks, const float3 fixedDir) {\n"
"	// No sampling on the back face\n"
"	if (fixedDir.z <= 0.f)\n"
"		return 0.f;\n"
"\n"
"	// Approximate H by using reflection direction for wi\n"
"	const float u = fabs(fixedDir.z);\n"
"	const float3 S = FresnelSchlick_Evaluate(ks, u);\n"
"\n"
"	// Ensures coating is never sampled less than half the time\n"
"	// unless we are on the back face\n"
"	return .5f * (1.f + Spectrum_Filter(S));\n"
"}\n"
"\n"
"float3 SchlickBSDF_CoatingF(const float3 ks, const float roughness,\n"
"		const float anisotropy, const int multibounce, const float3 fixedDir,\n"
"		const float3 sampledDir) {\n"
"	// No sampling on the back face\n"
"	if (fixedDir.z <= 0.f)\n"
"		return BLACK;\n"
"\n"
"	const float coso = fabs(fixedDir.z);\n"
"	const float cosi = fabs(sampledDir.z);\n"
"\n"
"	const float3 wh = normalize(fixedDir + sampledDir);\n"
"	const float3 S = FresnelSchlick_Evaluate(ks, fabs(dot(sampledDir, wh)));\n"
"\n"
"	const float G = SchlickDistribution_G(roughness, fixedDir, sampledDir);\n"
"\n"
"	// Multibounce - alternative with interreflection in the coating creases\n"
"	float factor = SchlickDistribution_D(roughness, wh, anisotropy) * G;\n"
"	//if (!fromLight)\n"
"		factor = factor / 4.f * coso +\n"
"				(multibounce ? cosi * clamp((1.f - G) / (4.f * coso * cosi), 0.f, 1.f) : 0.f);\n"
"	//else\n"
"	//	factor = factor / (4.f * cosi) + \n"
"	//			(multibounce ? coso * Clamp((1.f - G) / (4.f * cosi * coso), 0.f, 1.f) : 0.f);\n"
"\n"
"	return factor * S;\n"
"}\n"
"\n"
"float3 SchlickBSDF_CoatingSampleF(const float3 ks,\n"
"		const float roughness, const float anisotropy, const int multibounce,\n"
"		const float3 fixedDir, float3 *sampledDir,\n"
"		float u0, float u1, float *pdf) {\n"
"	// No sampling on the back face\n"
"	if (fixedDir.z <= 0.f)\n"
"		return BLACK;\n"
"\n"
"	float3 wh;\n"
"	float d, specPdf;\n"
"	SchlickDistribution_SampleH(roughness, anisotropy, u0, u1, &wh, &d, &specPdf);\n"
"	const float cosWH = dot(fixedDir, wh);\n"
"	*sampledDir = 2.f * cosWH * wh - fixedDir;\n"
"\n"
"	if (((*sampledDir).z < DEFAULT_COS_EPSILON_STATIC) || (fixedDir.z * (*sampledDir).z < 0.f))\n"
"		return BLACK;\n"
"\n"
"	const float coso = fabs(fixedDir.z);\n"
"	const float cosi = fabs((*sampledDir).z);\n"
"\n"
"	*pdf = specPdf / (4.f * cosWH);\n"
"	if (*pdf <= 0.f)\n"
"		return BLACK;\n"
"\n"
"	float3 S = FresnelSchlick_Evaluate(ks, cosWH);\n"
"\n"
"	const float G = SchlickDistribution_G(roughness, fixedDir, *sampledDir);\n"
"\n"
"	//CoatingF(sw, *wi, wo, f_);\n"
"	S *= (d / *pdf) * G / (4.f * coso) + \n"
"			(multibounce ? cosi * clamp((1.f - G) / (4.f * coso * cosi), 0.f, 1.f) : 0.f);\n"
"\n"
"	return S;\n"
"}\n"
"\n"
"float SchlickBSDF_CoatingPdf(const float roughness, const float anisotropy,\n"
"		const float3 fixedDir, const float3 sampledDir) {\n"
"	// No sampling on the back face\n"
"	if (fixedDir.z <= 0.f)\n"
"		return 0.f;\n"
"\n"
"	const float3 wh = normalize(fixedDir + sampledDir);\n"
"	return SchlickDistribution_Pdf(roughness, wh, anisotropy) / (4.f * fabs(dot(fixedDir, wh)));\n"
"}\n"
"\n"
"float3 Glossy2Material_ConstEvaluate(\n"
"		__global HitPoint *hitPoint, const float3 lightDir, const float3 eyeDir,\n"
"		BSDFEvent *event, float *directPdfW,\n"
"#if defined(PARAM_ENABLE_MAT_GLOSSY2_INDEX)\n"
"		const float i,\n"
"#endif\n"
"		const float nuVal,\n"
"#if defined(PARAM_ENABLE_MAT_GLOSSY2_ANISOTROPIC)\n"
"		const float nvVal,\n"
"#endif\n"
"#if defined(PARAM_ENABLE_MAT_GLOSSY2_ABSORPTION)\n"
"		const float3 kaVal,\n"
"		const float d,\n"
"#endif\n"
"#if defined(PARAM_ENABLE_MAT_GLOSSY2_MULTIBOUNCE)\n"
"		const int multibounceVal,\n"
"#endif\n"
"		const float3 kdVal, const float3 ksVal) {\n"
"	const float3 fixedDir = eyeDir;\n"
"	const float3 sampledDir = lightDir;\n"
"\n"
"	const float3 baseF = Spectrum_Clamp(kdVal) * M_1_PI_F * fabs(lightDir.z);\n"
"	if (eyeDir.z <= 0.f) {\n"
"		// Back face: no coating\n"
"\n"
"		if (directPdfW)\n"
"			*directPdfW = fabs(sampledDir.z * M_1_PI_F);\n"
"\n"
"		*event = DIFFUSE | REFLECT;\n"
"		return baseF;\n"
"	}\n"
"\n"
"	// Front face: coating+base\n"
"	*event = GLOSSY | REFLECT;\n"
"\n"
"	float3 ks = ksVal;\n"
"#if defined(PARAM_ENABLE_MAT_GLOSSY2_INDEX)\n"
"	if (i > 0.f) {\n"
"		const float ti = (i - 1.f) / (i + 1.f);\n"
"		ks *= ti * ti;\n"
"	}\n"
"#endif\n"
"	ks = Spectrum_Clamp(ks);\n"
"\n"
"	const float u = clamp(nuVal, 6e-3f, 1.f);\n"
"#if defined(PARAM_ENABLE_MAT_GLOSSY2_ANISOTROPIC)\n"
"	const float v = clamp(nvVal, 6e-3f, 1.f);\n"
"	const float u2 = u * u;\n"
"	const float v2 = v * v;\n"
"	const float anisotropy = (u2 < v2) ? (1.f - u2 / v2) : (v2 / u2 - 1.f);\n"
"	const float roughness = u * v;\n"
"#else\n"
"	const float anisotropy = 0.f;\n"
"	const float roughness = u * u;\n"
"#endif\n"
"\n"
"	if (directPdfW) {\n"
"		const float wCoating = SchlickBSDF_CoatingWeight(ks, fixedDir);\n"
"		const float wBase = 1.f - wCoating;\n"
"\n"
"		*directPdfW = wBase * fabs(sampledDir.z * M_1_PI_F) +\n"
"				wCoating * SchlickBSDF_CoatingPdf(roughness, anisotropy, fixedDir, sampledDir);\n"
"	}\n"
"\n"
"	// Absorption\n"
"	const float cosi = fabs(sampledDir.z);\n"
"	const float coso = fabs(fixedDir.z);\n"
"\n"
"#if defined(PARAM_ENABLE_MAT_GLOSSY2_ABSORPTION)\n"
"	const float3 alpha = Spectrum_Clamp(kaVal);\n"
"	const float3 absorption = CoatingAbsorption(cosi, coso, alpha, d);\n"
"#else\n"
"	const float3 absorption = WHITE;\n"
"#endif\n"
"\n"
"	// Coating fresnel factor\n"
"	const float3 H = normalize(fixedDir + sampledDir);\n"
"	const float3 S = FresnelSchlick_Evaluate(ks, fabs(dot(sampledDir, H)));\n"
"\n"
"#if defined(PARAM_ENABLE_MAT_GLOSSY2_MULTIBOUNCE)\n"
"	const int multibounce = multibounceVal;\n"
"#else\n"
"	const int multibounce = 0;\n"
"#endif\n"
"	const float3 coatingF = SchlickBSDF_CoatingF(ks, roughness, anisotropy, multibounce,\n"
"			fixedDir, sampledDir);\n"
"\n"
"	// Blend in base layer Schlick style\n"
"	// assumes coating bxdf takes fresnel factor S into account\n"
"\n"
"	return coatingF + absorption * (WHITE - S) * baseF;\n"
"}\n"
"\n"
"float3 Glossy2Material_ConstSample(\n"
"		__global HitPoint *hitPoint, const float3 fixedDir, float3 *sampledDir,\n"
"		const float u0, const float u1,\n"
"#if defined(PARAM_HAS_PASSTHROUGH)\n"
"		const float passThroughEvent,\n"
"#endif\n"
"		float *pdfW, float *cosSampledDir, BSDFEvent *event,\n"
"		const BSDFEvent requestedEvent,\n"
"#if defined(PARAM_ENABLE_MAT_GLOSSY2_INDEX)\n"
"		const float i,\n"
"#endif\n"
"		const float nuVal,\n"
"#if defined(PARAM_ENABLE_MAT_GLOSSY2_ANISOTROPIC)\n"
"		const float nvVal,\n"
"#endif\n"
"#if defined(PARAM_ENABLE_MAT_GLOSSY2_ABSORPTION)\n"
"		const float3 kaVal,\n"
"		const float d,\n"
"#endif\n"
"#if defined(PARAM_ENABLE_MAT_GLOSSY2_MULTIBOUNCE)\n"
"		const int multibounceVal,\n"
"#endif\n"
"		const float3 kdVal, const float3 ksVal) {\n"
"	if ((!(requestedEvent & (GLOSSY | REFLECT)) && fixedDir.z > 0.f) ||\n"
"		(!(requestedEvent & (DIFFUSE | REFLECT)) && fixedDir.z <= 0.f) ||\n"
"		(fabs(fixedDir.z) < DEFAULT_COS_EPSILON_STATIC))\n"
"		return BLACK;\n"
"\n"
"	if (fixedDir.z <= 0.f) {\n"
"		// Back Face\n"
"		*sampledDir = (signbit(fixedDir.z) ? -1.f : 1.f) * CosineSampleHemisphereWithPdf(u0, u1, pdfW);\n"
"		*cosSampledDir = fabs((*sampledDir).z);\n"
"		if (*cosSampledDir < DEFAULT_COS_EPSILON_STATIC)\n"
"			return BLACK;\n"
"		*event = DIFFUSE | REFLECT;\n"
"		return Spectrum_Clamp(kdVal);\n"
"	}\n"
"\n"
"	float3 ks = ksVal;\n"
"#if defined(PARAM_ENABLE_MAT_GLOSSY2_INDEX)\n"
"	if (i > 0.f) {\n"
"		const float ti = (i - 1.f) / (i + 1.f);\n"
"		ks *= ti * ti;\n"
"	}\n"
"#endif\n"
"	ks = Spectrum_Clamp(ks);\n"
"\n"
"	const float u = clamp(nuVal, 6e-3f, 1.f);\n"
"#if defined(PARAM_ENABLE_MAT_GLOSSY2_ANISOTROPIC)\n"
"	const float v = clamp(nvVal, 6e-3f, 1.f);\n"
"	const float u2 = u * u;\n"
"	const float v2 = v * v;\n"
"	const float anisotropy = (u2 < v2) ? (1.f - u2 / v2) : (v2 / u2 - 1.f);\n"
"	const float roughness = u * v;\n"
"#else\n"
"	const float anisotropy = 0.f;\n"
"	const float roughness = u * u;\n"
"#endif\n"
"\n"
"	// Coating is used only on the front face\n"
"	const float wCoating = SchlickBSDF_CoatingWeight(ks, fixedDir);\n"
"	const float wBase = 1.f - wCoating;\n"
"\n"
"	const float3 baseF = Spectrum_Clamp(kdVal) * M_1_PI_F;\n"
"\n"
"#if defined(PARAM_ENABLE_MAT_GLOSSY2_MULTIBOUNCE)\n"
"	const int multibounce = multibounceVal;\n"
"#else\n"
"	const int multibounce = 0;\n"
"#endif\n"
"\n"
"	float basePdf, coatingPdf;\n"
"	float3 coatingF;\n"
"	if (passThroughEvent < wBase) {\n"
"		// Sample base BSDF (Matte BSDF)\n"
"		*sampledDir = (signbit(fixedDir.z) ? -1.f : 1.f) * CosineSampleHemisphereWithPdf(u0, u1, &basePdf);\n"
"\n"
"		*cosSampledDir = fabs((*sampledDir).z);\n"
"		if (*cosSampledDir < DEFAULT_COS_EPSILON_STATIC)\n"
"			return BLACK;\n"
"\n"
"		// Evaluate coating BSDF (Schlick BSDF)\n"
"		coatingF = SchlickBSDF_CoatingF(ks, roughness, anisotropy, multibounce,\n"
"				fixedDir, *sampledDir);\n"
"		coatingPdf = SchlickBSDF_CoatingPdf(roughness, anisotropy, fixedDir, *sampledDir);\n"
"\n"
"		*event = GLOSSY | REFLECT;\n"
"	} else {\n"
"		// Sample coating BSDF (Schlick BSDF)\n"
"		coatingF = SchlickBSDF_CoatingSampleF(ks, roughness, anisotropy,\n"
"				multibounce, fixedDir, sampledDir, u0, u1, &coatingPdf);\n"
"		if (Spectrum_IsBlack(coatingF))\n"
"			return BLACK;\n"
"\n"
"		*cosSampledDir = fabs((*sampledDir).z);\n"
"		if (*cosSampledDir < DEFAULT_COS_EPSILON_STATIC)\n"
"			return BLACK;\n"
"\n"
"		coatingF *= coatingPdf;\n"
"\n"
"		// Evaluate base BSDF (Matte BSDF)\n"
"		basePdf = *cosSampledDir * M_1_PI_F;\n"
"\n"
"		*event = GLOSSY | REFLECT;\n"
"	}\n"
"\n"
"	*pdfW = coatingPdf * wCoating + basePdf * wBase;\n"
"\n"
"	// Absorption\n"
"	const float cosi = fabs((*sampledDir).z);\n"
"	const float coso = fabs(fixedDir.z);\n"
"\n"
"#if defined(PARAM_ENABLE_MAT_GLOSSY2_ABSORPTION)\n"
"	const float3 alpha = Spectrum_Clamp(kaVal);\n"
"	const float3 absorption = CoatingAbsorption(cosi, coso, alpha, d);\n"
"#else\n"
"	const float3 absorption = WHITE;\n"
"#endif\n"
"\n"
"	// Coating fresnel factor\n"
"	const float3 H = normalize(fixedDir + *sampledDir);\n"
"	const float3 S = FresnelSchlick_Evaluate(ks, fabs(dot(*sampledDir, H)));\n"
"\n"
"	// Blend in base layer Schlick style\n"
"	// coatingF already takes fresnel factor S into account\n"
"\n"
"	return (coatingF + absorption * (WHITE - S) * baseF * *cosSampledDir) / *pdfW;\n"
"}\n"
"\n"
"#if defined(PARAM_DISABLE_MAT_DYNAMIC_EVALUATION)\n"
"float3 Glossy2Material_Evaluate(__global Material *material,\n"
"		__global HitPoint *hitPoint, const float3 lightDir, const float3 eyeDir,\n"
"		BSDFEvent *event, float *directPdfW\n"
"		TEXTURES_PARAM_DECL) {\n"
"#if defined(PARAM_ENABLE_MAT_GLOSSY2_INDEX)\n"
"	const float i = Texture_GetFloatValue(material->glossy2.indexTexIndex, hitPoint\n"
"			TEXTURES_PARAM);\n"
"#endif\n"
"\n"
"	const float nuVal = Texture_GetFloatValue(material->glossy2.nuTexIndex, hitPoint\n"
"		TEXTURES_PARAM);\n"
"#if defined(PARAM_ENABLE_MAT_GLOSSY2_ANISOTROPIC)\n"
"	const float nvVal = Texture_GetFloatValue(material->glossy2.nvTexIndex, hitPoint\n"
"		TEXTURES_PARAM);\n"
"#endif\n"
"\n"
"#if defined(PARAM_ENABLE_MAT_GLOSSY2_ABSORPTION)\n"
"	const float3 kaVal = Texture_GetSpectrumValue(material->glossy2.kaTexIndex, hitPoint\n"
"			TEXTURES_PARAM);\n"
"	const float d = Texture_GetFloatValue(material->glossy2.depthTexIndex, hitPoint\n"
"			TEXTURES_PARAM);\n"
"#endif\n"
"\n"
"#if defined(PARAM_ENABLE_MAT_GLOSSY2_MULTIBOUNCE)\n"
"	const int multibounce = material->glossy2.multibounce;\n"
"#endif\n"
"\n"
"	const float3 kdVal = Texture_GetSpectrumValue(material->glossy2.kdTexIndex, hitPoint\n"
"			TEXTURES_PARAM);\n"
"	const float3 ksVal = Texture_GetSpectrumValue(material->glossy2.ksTexIndex, hitPoint\n"
"			TEXTURES_PARAM);\n"
"\n"
"	return Glossy2Material_ConstEvaluate(\n"
"			hitPoint, lightDir, eyeDir,\n"
"			event, directPdfW,\n"
"#if defined(PARAM_ENABLE_MAT_GLOSSY2_INDEX)\n"
"			i,\n"
"#endif\n"
"			nuVal,\n"
"#if defined(PARAM_ENABLE_MAT_GLOSSY2_ANISOTROPIC)\n"
"			nvVal,\n"
"#endif\n"
"#if defined(PARAM_ENABLE_MAT_GLOSSY2_ABSORPTION)\n"
"			kaVal,\n"
"			d,\n"
"#endif\n"
"#if defined(PARAM_ENABLE_MAT_GLOSSY2_MULTIBOUNCE)\n"
"			multibounce,\n"
"#endif\n"
"			kdVal, ksVal);\n"
"}\n"
"\n"
"float3 Glossy2Material_Sample(__global Material *material,\n"
"		__global HitPoint *hitPoint, const float3 fixedDir, float3 *sampledDir,\n"
"		const float u0, const float u1,\n"
"#if defined(PARAM_HAS_PASSTHROUGH)\n"
"		const float passThroughEvent,\n"
"#endif\n"
"		float *pdfW, float *cosSampledDir, BSDFEvent *event,\n"
"		const BSDFEvent requestedEvent\n"
"		TEXTURES_PARAM_DECL) {\n"
"#if defined(PARAM_ENABLE_MAT_GLOSSY2_INDEX)\n"
"	const float i = Texture_GetFloatValue(material->glossy2.indexTexIndex, hitPoint\n"
"			TEXTURES_PARAM);\n"
"#endif\n"
"\n"
"	const float nuVal = Texture_GetFloatValue(material->glossy2.nuTexIndex, hitPoint\n"
"		TEXTURES_PARAM);\n"
"#if defined(PARAM_ENABLE_MAT_GLOSSY2_ANISOTROPIC)\n"
"	const float nvVal = Texture_GetFloatValue(material->glossy2.nvTexIndex, hitPoint\n"
"		TEXTURES_PARAM);\n"
"#endif\n"
"\n"
"#if defined(PARAM_ENABLE_MAT_GLOSSY2_ABSORPTION)\n"
"	const float3 kaVal = Texture_GetSpectrumValue(material->glossy2.kaTexIndex, hitPoint\n"
"			TEXTURES_PARAM);\n"
"	const float d = Texture_GetFloatValue(material->glossy2.depthTexIndex, hitPoint\n"
"			TEXTURES_PARAM);\n"
"#endif\n"
"\n"
"	const float3 kdVal = Texture_GetSpectrumValue(material->glossy2.kdTexIndex, hitPoint\n"
"			TEXTURES_PARAM);\n"
"	const float3 ksVal = Texture_GetSpectrumValue(material->glossy2.ksTexIndex, hitPoint\n"
"			TEXTURES_PARAM);\n"
"\n"
"#if defined(PARAM_ENABLE_MAT_GLOSSY2_MULTIBOUNCE)\n"
"	const int multibounce = material->glossy2.multibounce;\n"
"#endif\n"
"\n"
"	return Glossy2Material_ConstSample(\n"
"			hitPoint, fixedDir, sampledDir,\n"
"			u0, u1,\n"
"#if defined(PARAM_HAS_PASSTHROUGH)\n"
"			passThroughEvent,\n"
"#endif\n"
"			pdfW, cosSampledDir, event,\n"
"			requestedEvent,\n"
"#if defined(PARAM_ENABLE_MAT_GLOSSY2_INDEX)\n"
"			i,\n"
"#endif\n"
"			nuVal,\n"
"#if defined(PARAM_ENABLE_MAT_GLOSSY2_ANISOTROPIC)\n"
"			nvVal,\n"
"#endif\n"
"#if defined(PARAM_ENABLE_MAT_GLOSSY2_ABSORPTION)\n"
"			kaVal,\n"
"			d,\n"
"#endif\n"
"#if defined(PARAM_ENABLE_MAT_GLOSSY2_MULTIBOUNCE)\n"
"			multibounce,\n"
"#endif\n"
"			kdVal, ksVal);\n"
"}\n"
"#endif\n"
"\n"
"#endif\n"
; } }