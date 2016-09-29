#include <string>
namespace luxrays { namespace ocl {
std::string KernelSource_ray_funcs = 
"#line 2 \"ray_funcs.cl\"\n"
"\n"
"/***************************************************************************\n"
" * Copyright 1998-2015 by authors (see AUTHORS.txt)                        *\n"
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
"void Ray_Init4_Private(Ray *ray, const float3 orig, const float3 dir,\n"
"		const float mint, const float maxt, const float time) {\n"
"	ray->o.x = orig.x;\n"
"	ray->o.y = orig.y;\n"
"	ray->o.z = orig.z;\n"
"\n"
"	ray->d.x = dir.x;\n"
"	ray->d.y = dir.y;\n"
"	ray->d.z = dir.z;\n"
"\n"
"	ray->mint = mint + MachineEpsilon_E_Float3(orig);\n"
"	ray->maxt = maxt - MachineEpsilon_E_Float3(orig + dir * maxt);\n"
"\n"
"	ray->time = time;\n"
"\n"
"	ray->flags = RAY_FLAGS_NONE;\n"
"}\n"
"\n"
"void Ray_Init3_Private(Ray *ray, const float3 orig, const float3 dir,\n"
"		const float maxt, const float time) {\n"
"	ray->o.x = orig.x;\n"
"	ray->o.y = orig.y;\n"
"	ray->o.z = orig.z;\n"
"\n"
"	ray->d.x = dir.x;\n"
"	ray->d.y = dir.y;\n"
"	ray->d.z = dir.z;\n"
"\n"
"	ray->mint = MachineEpsilon_E_Float3(orig);\n"
"	ray->maxt = maxt;\n"
"\n"
"	ray->time = time;\n"
"\n"
"	ray->flags = RAY_FLAGS_NONE;\n"
"}\n"
"\n"
"void Ray_Init2_Private(Ray *ray, const float3 orig, const float3 dir, const float time) {\n"
"	ray->o.x = orig.x;\n"
"	ray->o.y = orig.y;\n"
"	ray->o.z = orig.z;\n"
"\n"
"	ray->d.x = dir.x;\n"
"	ray->d.y = dir.y;\n"
"	ray->d.z = dir.z;\n"
"\n"
"	ray->mint = MachineEpsilon_E_Float3(orig);\n"
"	ray->maxt = INFINITY;\n"
"\n"
"	ray->time = time;\n"
"\n"
"	ray->flags = RAY_FLAGS_NONE;\n"
"}\n"
"\n"
"// Note: Ray_Init4() work like CPU with a call to Ray::UpdateMinMaxWithEpsilon())\n"
"void Ray_Init4(__global Ray *ray, const float3 orig, const float3 dir,\n"
"		const float mint, const float maxt, const float time) {\n"
"	VSTORE3F(orig, &ray->o.x);\n"
"	VSTORE3F(dir, &ray->d.x);\n"
"\n"
"	ray->mint = mint + MachineEpsilon_E_Float3(orig);\n"
"	ray->maxt = maxt - MachineEpsilon_E_Float3(orig + dir * maxt);\n"
"\n"
"	ray->time = time;\n"
"\n"
"	ray->flags = RAY_FLAGS_NONE;\n"
"}\n"
"\n"
"void Ray_Init3(__global Ray *ray, const float3 orig, const float3 dir,\n"
"		const float maxt, const float time) {\n"
"	VSTORE3F(orig, &ray->o.x);\n"
"	VSTORE3F(dir, &ray->d.x);\n"
"\n"
"	ray->mint = MachineEpsilon_E_Float3(orig);\n"
"	ray->maxt = maxt;\n"
"\n"
"	ray->time = time;\n"
"\n"
"	ray->flags = RAY_FLAGS_NONE;\n"
"}\n"
"\n"
"void Ray_Init2(__global Ray *ray, const float3 orig, const float3 dir, const float time) {\n"
"	VSTORE3F(orig, &ray->o.x);\n"
"	VSTORE3F(dir, &ray->d.x);\n"
"\n"
"	ray->mint = MachineEpsilon_E_Float3(orig);\n"
"	ray->maxt = INFINITY;\n"
"\n"
"	ray->time = time;\n"
"\n"
"	ray->flags = RAY_FLAGS_NONE;\n"
"}\n"
"\n"
"void Ray_ReadAligned4(__global const Ray* restrict ray, float3 *rayOrig, float3 *rayDir,\n"
"		float *mint, float *maxt, float *time, uint *flags) {\n"
"	__global float4 *basePtr =(__global float4 *)ray;\n"
"	const float4 data0 = (*basePtr++);\n"
"	const float4 data1 = (*basePtr);\n"
"\n"
"	*rayOrig = (float3)(data0.x, data0.y, data0.z);\n"
"	*rayDir = (float3)(data0.w, data1.x, data1.y);\n"
"\n"
"	*mint = data1.z;\n"
"	*maxt = data1.w;\n"
"\n"
"	*time = ray->time;\n"
"	*flags = ray->flags;\n"
"}\n"
"\n"
"void Ray_ReadAligned4_Private(__global const Ray* restrict ray, Ray *dstRay) {\n"
"	__global float4 *basePtr =(__global float4 *)ray;\n"
"	const float4 data0 = (*basePtr++);\n"
"	const float4 data1 = (*basePtr);\n"
"\n"
"	dstRay->o.x = data0.x;\n"
"	dstRay->o.y = data0.y;\n"
"	dstRay->o.z = data0.z;\n"
"	dstRay->d.x = data0.w;\n"
"	dstRay->d.y = data1.x;\n"
"	dstRay->d.z = data1.y;\n"
"\n"
"	dstRay->mint = data1.z;\n"
"	dstRay->maxt = data1.w;\n"
"\n"
"	dstRay->time = ray->time;\n"
"	dstRay->flags = ray->flags;\n"
"}\n"
; } }
