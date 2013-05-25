/***************************************************************************
 *   Copyright (C) 2007 by Anthony Pajot   
 *   anthony.pajot@etu.enseeiht.fr
 *
 * This file is part of FlexRay
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 ***************************************************************************/

#include "luxrays/accelerators/mqbvhaccel.h"
#include "luxrays/core/utils.h"
#include "luxrays/core/context.h"
#include "luxrays/core/exttrianglemesh.h"
#ifdef LUXRAYS_DISABLE_OPENCL
#include "luxrays/core/intersectiondevice.h"
#else
#include "luxrays/core/oclintersectiondevice.h"
#include "luxrays/kernels/kernels.h"
#endif

namespace luxrays {

#if !defined(LUXRAYS_DISABLE_OPENCL)

class OpenCLMQBVHKernels : public OpenCLKernels {
public:
	OpenCLMQBVHKernels(OpenCLIntersectionDevice *dev, const u_int kernelCount, const MQBVHAccel *accel) :
		OpenCLKernels(dev, kernelCount), mqbvh(accel), mqbvhBuff(NULL), memMapBuff(NULL), leafBuff(NULL),
		leafQuadTrisBuff(NULL), invTransBuff(NULL) {
		const Context *deviceContext = device->GetContext();
		const std::string &deviceName(device->GetName());
		cl::Context &oclContext = device->GetOpenCLContext();
		cl::Device &oclDevice = device->GetOpenCLDevice();

		// Compile sources
		std::stringstream params;
		params << "-D LUXRAYS_OPENCL_KERNEL"
				" -D PARAM_RAY_EPSILON_MIN=" << MachineEpsilon::GetMin() << "f"
				" -D PARAM_RAY_EPSILON_MAX=" << MachineEpsilon::GetMax() << "f";

		std::string code(
			luxrays::ocl::KernelSource_luxrays_types +
			luxrays::ocl::KernelSource_epsilon_types +
			luxrays::ocl::KernelSource_epsilon_funcs +
			luxrays::ocl::KernelSource_point_types +
			luxrays::ocl::KernelSource_vector_types +
			luxrays::ocl::KernelSource_ray_types +
			luxrays::ocl::KernelSource_ray_funcs +
			luxrays::ocl::KernelSource_bbox_types +
			luxrays::ocl::KernelSource_matrix4x4_types);
		code += luxrays::ocl::KernelSource_mqbvh;
		cl::Program::Sources source(1, std::make_pair(code.c_str(), code.length()));
		cl::Program program = cl::Program(oclContext, source);
		try {
			VECTOR_CLASS<cl::Device> buildDevice;
			buildDevice.push_back(oclDevice);
			program.build(buildDevice, params.str().c_str());
		} catch (cl::Error err) {
			cl::STRING_CLASS strError = program.getBuildInfo<CL_PROGRAM_BUILD_LOG>(oclDevice);
			LR_LOG(deviceContext, "[OpenCL device::" << deviceName <<
				"] MQBVH compilation error:\n" << strError.c_str());
			throw err;
		}

		for (u_int i = 0; i < kernelCount; ++i) {
			kernels[i] = new cl::Kernel(program, "Intersect");

			if (device->GetDeviceDesc()->GetForceWorkGroupSize() > 0)
				workGroupSize = device->GetDeviceDesc()->GetForceWorkGroupSize();
			else {
				kernels[i]->getWorkGroupInfo<size_t>(oclDevice,
					CL_KERNEL_WORK_GROUP_SIZE, &workGroupSize);
				//LR_LOG(deviceContext, "[OpenCL device::" << deviceName <<
				//	"] MQBVH kernel work group size: " << workGroupSize);

				if (workGroupSize > 256) {
					// Otherwise I will probably run out of local memory
					workGroupSize = 256;
					//LR_LOG(deviceContext, "[OpenCL device::" << deviceName <<
					//	"] Cap work group size to: " << workGroupSize);
				}
			}
		}
	}
	virtual ~OpenCLMQBVHKernels() {
		device->FreeMemory(mqbvhBuff->getInfo<CL_MEM_SIZE>());
		delete mqbvhBuff;
		mqbvhBuff = NULL;
		device->FreeMemory(memMapBuff->getInfo<CL_MEM_SIZE>());
		delete memMapBuff;
		memMapBuff = NULL;
		device->FreeMemory(leafBuff->getInfo<CL_MEM_SIZE>());
		delete leafBuff;
		leafBuff = NULL;
		device->FreeMemory(leafQuadTrisBuff->getInfo<CL_MEM_SIZE>());
		delete leafQuadTrisBuff;
		leafQuadTrisBuff = NULL;
		device->FreeMemory(invTransBuff->getInfo<CL_MEM_SIZE>());
		delete invTransBuff;
		invTransBuff = NULL;
	}

	void SetBuffers(cl::Buffer *m, cl::Buffer *l, cl::Buffer *q,
		cl::Buffer *mm, cl::Buffer *t);
	virtual void Update(const DataSet *newDataSet);
	virtual void EnqueueRayBuffer(cl::CommandQueue &oclQueue, const u_int kernelIndex,
		cl::Buffer &rBuff, cl::Buffer &hBuff, const u_int rayCount,
		const VECTOR_CLASS<cl::Event> *events, cl::Event *event);

protected:
	const MQBVHAccel *mqbvh;

	// MQBVH fields
	cl::Buffer *mqbvhBuff;
	cl::Buffer *memMapBuff;
	cl::Buffer *leafBuff;
	cl::Buffer *leafQuadTrisBuff;
	cl::Buffer *invTransBuff;
};

void OpenCLMQBVHKernels::SetBuffers(cl::Buffer *m, cl::Buffer *l, cl::Buffer *q,
	cl::Buffer *mm, cl::Buffer *t) {
	mqbvhBuff = m;
	leafBuff = l;
	leafQuadTrisBuff = q;
	memMapBuff = mm;
	invTransBuff = t;

	// Set arguments
	BOOST_FOREACH(cl::Kernel *kernel, kernels) {
		kernel->setArg(2, *mqbvhBuff);
		kernel->setArg(4, *memMapBuff);
		kernel->setArg(5, *leafBuff);
		kernel->setArg(6, *leafQuadTrisBuff);
		kernel->setArg(7, *invTransBuff);
	}
}

void OpenCLMQBVHKernels::Update(const DataSet *newDataSet) {
	const Context *deviceContext = device->GetContext();
	const std::string &deviceName(device->GetName());
	OpenCLDeviceDescription *deviceDesc = device->GetDeviceDesc();
	LR_LOG(deviceContext, "[OpenCL device::" << deviceName <<
		"] Updating DataSet");

	// Upload QBVH leafs transformations
	Matrix4x4 *invTrans = new Matrix4x4[mqbvh->GetNLeafs()];
	for (u_int i = 0; i < mqbvh->GetNLeafs(); ++i) {
		if (mqbvh->GetTransforms()[i])
			invTrans[i] = mqbvh->GetTransforms()[i]->mInv;
		else
			invTrans[i] = Matrix4x4();
	}

	device->GetOpenCLQueue().enqueueWriteBuffer(
		*invTransBuff,
		CL_TRUE,
		0,
		mqbvh->GetNLeafs() * sizeof(Matrix4x4),
		invTrans);
	delete[] invTrans;

	// Update MQBVH nodes
	device->FreeMemory(mqbvhBuff->getInfo<CL_MEM_SIZE>());
	delete mqbvhBuff;

	mqbvhBuff = new cl::Buffer(deviceDesc->GetOCLContext(),
		CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR,
		sizeof(QBVHNode) * mqbvh->GetNNodes(), mqbvh->GetTree());
	device->AllocMemory(mqbvhBuff->getInfo<CL_MEM_SIZE>());

	BOOST_FOREACH(cl::Kernel *kernel, kernels)
		kernel->setArg(2, *mqbvhBuff);
}

void OpenCLMQBVHKernels::EnqueueRayBuffer(cl::CommandQueue &oclQueue, const u_int kernelIndex,
		cl::Buffer &rBuff, cl::Buffer &hBuff, const u_int rayCount,
		const VECTOR_CLASS<cl::Event> *events, cl::Event *event) {
	kernels[kernelIndex]->setArg(0, rBuff);
	kernels[kernelIndex]->setArg(1, hBuff);
	kernels[kernelIndex]->setArg(3, rayCount);

	const u_int globalRange = RoundUp<u_int>(rayCount, workGroupSize);
	oclQueue.enqueueNDRangeKernel(*kernels[kernelIndex], cl::NullRange,
		cl::NDRange(globalRange), cl::NDRange(workGroupSize), events,
		event);
}

OpenCLKernels *MQBVHAccel::NewOpenCLKernels(OpenCLIntersectionDevice *device,
		const u_int kernelCount, const u_int stackSize, const bool enableImageStorage) const {
	const Context *deviceContext = device->GetContext();
	cl::Context &oclContext = device->GetOpenCLContext();
	const std::string &deviceName(device->GetName());

	// TODO: remove the following limitation
	// NOTE: this code is somewhat limited to 32bit address space of the OpenCL device

	// Allocate buffers
	LR_LOG(deviceContext, "[OpenCL device::" << deviceName <<
		"] MQBVH buffer size: " <<
		(sizeof(QBVHNode) * nNodes / 1024) << "Kbytes");
	cl::Buffer *mqbvhBuff = new cl::Buffer(oclContext,
		CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR,
		sizeof(QBVHNode) * nNodes, nodes);
	device->AllocMemory(mqbvhBuff->getInfo<CL_MEM_SIZE>());

	// Calculate the size of memory to allocate
	u_int totalNodesCount = 0;
	u_int totalQuadTrisCount = 0;

	std::map<const QBVHAccel *, u_int> indexNodesMap;
	std::map<const QBVHAccel *, u_int> indexQuadTrisMap;

	for (std::map<const Mesh *, QBVHAccel *, bool (*)(const Mesh *, const Mesh *)>::const_iterator it = accels.begin(); it != accels.end(); ++it) {
		const QBVHAccel *qbvh = it->second;

		indexNodesMap[qbvh] = totalNodesCount;
		totalNodesCount += qbvh->nNodes;
		indexQuadTrisMap[qbvh] = totalQuadTrisCount;
		totalQuadTrisCount += qbvh->nQuads;
	}

	// Pack together all QBVH Leafs
	QBVHNode *allLeafs = new QBVHNode[totalNodesCount];
	u_int nodesOffset = 0;
	for (std::map<const Mesh *, QBVHAccel *, bool (*)(const Mesh *, const Mesh *)>::const_iterator it = accels.begin(); it != accels.end(); ++it) {
		const QBVHAccel *qbvh = it->second;

		const size_t nodesMemSize = sizeof(QBVHNode) * qbvh->nNodes;
		memcpy(&allLeafs[nodesOffset], qbvh->nodes, nodesMemSize);
		nodesOffset += qbvh->nNodes;
	}

	// Allocate memory for QBVH Leafs
	LR_LOG(deviceContext, "[OpenCL device::" << deviceName <<
		"] MQBVH leaf Nodes buffer size: " <<
		(totalNodesCount * sizeof(QBVHNode) / 1024) << "Kbytes");
	cl::Buffer *leafBuff = new cl::Buffer(oclContext, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR,
		sizeof(QBVHNode) * totalNodesCount, allLeafs);
	device->AllocMemory(leafBuff->getInfo<CL_MEM_SIZE>());
	delete[] allLeafs;

	// Pack together all QuadTriangle
	QuadTriangle *allQuadTris = new QuadTriangle[totalQuadTrisCount];

	u_int quadTrisOffset = 0;
	for (std::map<const Mesh *, QBVHAccel *, bool (*)(const Mesh *, const Mesh *)>::const_iterator it = accels.begin(); it != accels.end(); ++it) {
		const QBVHAccel *qbvh = it->second;

		const size_t quadTrisMemSize = sizeof(QuadTriangle) * qbvh->nQuads;
		memcpy(&allQuadTris[quadTrisOffset], qbvh->prims, quadTrisMemSize);
		quadTrisOffset += qbvh->nQuads;
	}

	// Allocate memory for QBVH QuadTriangles
	LR_LOG(deviceContext, "[OpenCL device::" << deviceName <<
		"] MQBVH QuadTriangle buffer size: " <<
		(totalQuadTrisCount * sizeof(QuadTriangle) / 1024) << "Kbytes");
	cl::Buffer *leafQuadTrisBuff = new cl::Buffer(oclContext, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR,
		sizeof(QuadTriangle) * totalQuadTrisCount, allQuadTris);
	device->AllocMemory(leafQuadTrisBuff->getInfo<CL_MEM_SIZE>());
	delete[] allQuadTris;

	u_int *memMap = new u_int[nLeafs * 2];
	for (u_int i = 0; i < nLeafs; ++i) {
		memMap[i * 2] = indexNodesMap[leafs[i]];
		memMap[i * 2 + 1] = indexQuadTrisMap[leafs[i]];
	}
	LR_LOG(deviceContext, "[OpenCL device::" << deviceName <<
		"] MQBVH memory map buffer size: " <<
		(nLeafs * sizeof(u_int) * 2 / 1024) <<
		"Kbytes");
	cl::Buffer *memMapBuff = new cl::Buffer(oclContext,
		CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR,
		sizeof(u_int) * nLeafs * 2, memMap);
	device->AllocMemory(memMapBuff->getInfo<CL_MEM_SIZE>());
	delete[] memMap;

	// Upload QBVH leafs transformations
	Matrix4x4 *invTrans = new Matrix4x4[nLeafs];
	for (u_int i = 0; i < nLeafs; ++i) {
		if (leafsTransform[i])
			invTrans[i] = leafsTransform[i]->mInv;
		else
			invTrans[i] = Matrix4x4();
	}
	const size_t invTransMemSize = nLeafs * sizeof(Matrix4x4);
	LR_LOG(deviceContext, "[OpenCL device::" << deviceName <<
		"] MQBVH inverse transformations buffer size: " <<
		(invTransMemSize / 1024) << "Kbytes");
	cl::Buffer *invTransBuff = new cl::Buffer(oclContext,
		CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR,
		invTransMemSize, invTrans);
	device->AllocMemory(invTransBuff->getInfo<CL_MEM_SIZE>());
	delete[] invTrans;

	// Setup kernels
	OpenCLMQBVHKernels *kernels = new OpenCLMQBVHKernels(device, kernelCount, this);
	kernels->SetBuffers(mqbvhBuff, leafBuff, leafQuadTrisBuff, memMapBuff,
				invTransBuff);

	return kernels;
}

bool MQBVHAccel::CanRunOnOpenCLDevice(OpenCLIntersectionDevice *device) const {
	const OpenCLDeviceDescription *deviceDesc = device->GetDeviceDesc();

	u_int totalNodesCount = 0;
	u_int totalQuadTrisCount = 0;
	for (std::map<const Mesh *, QBVHAccel *, bool (*)(const Mesh *, const Mesh *)>::const_iterator it = accels.begin(); it != accels.end(); ++it) {
		const QBVHAccel *qbvh = it->second;
		totalNodesCount += qbvh->nNodes;
		totalQuadTrisCount += qbvh->nQuads;
	}

	// Check if I can allocate enough space for the accelerator data
	if (sizeof(QBVHNode) * nNodes > deviceDesc->GetMaxMemoryAllocSize()) {
		LR_LOG(device->GetContext(), "[OpenCL device::" << device->GetName() <<
			"] Can not run QBVH because root node buffer is too big: " <<
			(sizeof(QBVHNode) * nNodes / 1024) << "Kbytes");
		return false;
	}
	if (sizeof(QBVHNode) * totalNodesCount > deviceDesc->GetMaxMemoryAllocSize()) {
		LR_LOG(device->GetContext(), "[OpenCL device::" << device->GetName() <<
			"] Can not run QBVH because leafs node buffer is too big: " <<
			(sizeof(QBVHNode) * totalNodesCount / 1024) << "Kbytes");
		return false;
	}
	if (sizeof(QuadTriangle) * totalQuadTrisCount > deviceDesc->GetMaxMemoryAllocSize()) {
		LR_LOG(device->GetContext(), "[OpenCL device::" << device->GetName() <<
			"] Can not run QBVH because triangle buffer is too big: " <<
			(sizeof(QuadTriangle) * totalQuadTrisCount / 1024) << "Kbytes");
		return false;
	}
	return true;
}

#else

OpenCLKernels *MQBVHAccel::NewOpenCLKernels(OpenCLIntersectionDevice *device,
		const u_int kernelCount, const u_int stackSize, const bool enableImageStorage) const {
	return NULL;
}

bool MQBVHAccel::CanRunOnOpenCLDevice(OpenCLIntersectionDevice *device) const {
	return false;
}

#endif

MQBVHAccel::MQBVHAccel(const Context *context,
		u_int fst, u_int sf) : fullSweepThreshold(fst),
		skipFactor(sf), accels(MeshPtrCompare), ctx(context) {
	initialized = false;
}

MQBVHAccel::~MQBVHAccel() {
	if (initialized) {
		FreeAligned(nodes);

		delete[] leafsTransform;
		delete[] leafs;

		for (std::map<const Mesh *, QBVHAccel *, bool (*)(const Mesh *, const Mesh *)>::iterator it = accels.begin(); it != accels.end(); it++)
			delete it->second;
	}
}

bool MQBVHAccel::MeshPtrCompare(const Mesh *p0, const Mesh *p1) {
	return p0 < p1;
}

void MQBVHAccel::Init(const std::deque<const Mesh *> &meshes, const u_longlong totalVertexCount,
		const u_longlong totalTriangleCount) {
	assert (!initialized);

	meshList = meshes;

	// Build a QBVH for each mesh
	nLeafs = meshList.size();
	LR_LOG(ctx, "MQBVH leaf count: " << nLeafs);

	leafs = new QBVHAccel*[nLeafs];
	leafsTransform = new const Transform*[nLeafs];
	u_int currentOffset = 0;
	double lastPrint = WallClockTime();
	for (u_int i = 0; i < nLeafs; ++i) {
		const double now = WallClockTime();
		if (now - lastPrint > 2.0) {
			LR_LOG(ctx, "Building QBVH for MQBVH leaf: " << i << "/" << nLeafs);
			lastPrint = now;
		}

		switch (meshList[i]->GetType()) {
			case TYPE_TRIANGLE:
			case TYPE_EXT_TRIANGLE: {
				leafs[i] = new QBVHAccel(ctx, 4, 4 * 4, 1);
				leafs[i]->Init(std::deque<const Mesh *>(1, meshList[i]),
						meshList[i]->GetTotalVertexCount(), meshList[i]->GetTotalTriangleCount());
				accels[meshList[i]] = leafs[i];

				leafsTransform[i] = NULL;
				break;
			}
			case TYPE_TRIANGLE_INSTANCE: {
				InstanceTriangleMesh *itm = (InstanceTriangleMesh *)meshList[i];

				// Check if a QBVH has already been created
				std::map<const Mesh *, QBVHAccel *, bool (*)(const Mesh *, const Mesh *)>::iterator it = accels.find(itm->GetTriangleMesh());

				if (it == accels.end()) {
					// Create a new QBVH
					leafs[i] = new QBVHAccel(ctx, 4, 4 * 4, 1);
					leafs[i]->Init(std::deque<const Mesh *>(1, itm->GetTriangleMesh()),
							itm->GetTotalVertexCount(), itm->GetTotalTriangleCount());
					accels[itm->GetTriangleMesh()] = leafs[i];
				} else {
					//LR_LOG(ctx, "Cached QBVH leaf");
					leafs[i] = it->second;
				}

				leafsTransform[i] = &itm->GetTransformation();
				break;
			}
			case TYPE_EXT_TRIANGLE_INSTANCE: {
				ExtInstanceTriangleMesh *eitm = (ExtInstanceTriangleMesh *)meshList[i];

				// Check if a QBVH has already been created
				std::map<const Mesh *, QBVHAccel *, bool (*)(const Mesh *, const Mesh *)>::iterator it = accels.find(eitm->GetExtTriangleMesh());
				if (it == accels.end()) {
					// Create a new QBVH
					leafs[i] = new QBVHAccel(ctx, 4, 4 * 4, 1);
					leafs[i]->Init(std::deque<const Mesh *>(1, eitm->GetExtTriangleMesh()),
							eitm->GetTotalVertexCount(), eitm->GetTotalTriangleCount());
					accels[eitm->GetExtTriangleMesh()] = leafs[i];
				} else {
					//LR_LOG(ctx, "Cached QBVH leaf");
					leafs[i] = it->second;
				}

				leafsTransform[i] = &eitm->GetTransformation();
				break;
			}
			default:
				assert (false);
				break;
		}

		currentOffset += meshList[i]->GetTotalTriangleCount();
	}

	maxNodes = 2 * nLeafs - 1;
	nodes = AllocAligned<QBVHNode>(maxNodes);

	Update();
	
	initialized = true;
}

void MQBVHAccel::Update() {
	// Temporary data for building
	u_int *primsIndexes = new u_int[nLeafs];

	nNodes = 0;
	for (u_int i = 0; i < maxNodes; ++i)
		nodes[i] = QBVHNode();

	// The arrays that will contain
	// - the bounding boxes for all leafs
	// - the centroids for all leafs
	BBox *primsBboxes = new BBox[nLeafs];
	Point *primsCentroids = new Point[nLeafs];
	// The bouding volume of all the centroids
	BBox centroidsBbox;

	// Fill each base array
	for (u_int i = 0; i < nLeafs; ++i) {
		// This array will be reorganized during construction.
		primsIndexes[i] = i;

		// Compute the bounding box for the triangle
		primsBboxes[i] = meshList[i]->GetBBox();
		primsBboxes[i].Expand(MachineEpsilon::E(primsBboxes[i]));
		primsCentroids[i] = (primsBboxes[i].pMin + primsBboxes[i].pMax) * .5f;

		// Update the global bounding boxes
		worldBound = Union(worldBound, primsBboxes[i]);
		centroidsBbox = Union(centroidsBbox, primsCentroids[i]);
	}

	// Recursively build the tree
	LR_LOG(ctx, "Building MQBVH, leafs: " << nLeafs << ", initial nodes: " << maxNodes);

	BuildTree(0, nLeafs, primsIndexes, primsBboxes, primsCentroids,
			worldBound, centroidsBbox, -1, 0, 0);

	LR_LOG(ctx, "MQBVH completed with " << nNodes << "/" << maxNodes << " nodes");
	LR_LOG(ctx, "Total MQBVH memory usage: " << nNodes * sizeof(QBVHNode) / 1024 << "Kbytes");

	// Release temporary memory
	delete[] primsBboxes;
	delete[] primsCentroids;
	delete[] primsIndexes;
}

void MQBVHAccel::BuildTree(u_int start, u_int end, u_int *primsIndexes,
		BBox *primsBboxes, Point *primsCentroids, const BBox &nodeBbox,
		const BBox &centroidsBbox, int32_t parentIndex, int32_t childIndex, int depth) {
	// Create a leaf ?
	//********
	if (end - start == 1) {
		CreateLeaf(parentIndex, childIndex, primsIndexes[start], nodeBbox);
		return;
	}

	int32_t currentNode = parentIndex;
	int32_t leftChildIndex = childIndex;
	int32_t rightChildIndex = childIndex + 1;

	// Number of primitives in each bin
	int bins[NB_BINS];
	// Bbox of the primitives in the bin
	BBox binsBbox[NB_BINS];

	//--------------
	// Fill in the bins, considering all the primitives when a given
	// threshold is reached, else considering only a portion of the
	// primitives for the binned-SAH process. Also compute the bins bboxes
	// for the primitives. 

	for (u_int i = 0; i < NB_BINS; ++i)
		bins[i] = 0;

	u_int step = (end - start < fullSweepThreshold) ? 1 : skipFactor;

	// Choose the split axis, taking the axis of maximum extent for the
	// centroids (else weird cases can occur, where the maximum extent axis
	// for the nodeBbox is an axis of 0 extent for the centroids one.).
	const int axis = centroidsBbox.MaximumExtent();

	// Precompute values that are constant with respect to the current
	// primitive considered.
	const float k0 = centroidsBbox.pMin[axis];
	const float k1 = NB_BINS / (centroidsBbox.pMax[axis] - k0);

	if (k1 == INFINITY)
		throw std::runtime_error("MQBVH unable to handle geometry, too many primitives with the same centroid");

	// Create an intermediate node if the depth indicates to do so.
	// Register the split axis.
	if (depth % 2 == 0) {
		currentNode = CreateNode(parentIndex, childIndex, nodeBbox);
		leftChildIndex = 0;
		rightChildIndex = 2;
	}

	for (u_int i = start; i < end; i += step) {
		u_int primIndex = primsIndexes[i];

		// Binning is relative to the centroids bbox and to the
		// primitives' centroid.
		const int binId = Min(NB_BINS - 1, Floor2Int(k1 * (primsCentroids[primIndex][axis] - k0)));

		bins[binId]++;
		binsBbox[binId] = Union(binsBbox[binId], primsBboxes[primIndex]);
	}

	//--------------
	// Evaluate where to split.

	// Cumulative number of primitives in the bins from the first to the
	// ith, and from the last to the ith.
	int nbPrimsLeft[NB_BINS];
	int nbPrimsRight[NB_BINS];
	// The corresponding cumulative bounding boxes.
	BBox bboxesLeft[NB_BINS];
	BBox bboxesRight[NB_BINS];

	// The corresponding volumes.
	float vLeft[NB_BINS];
	float vRight[NB_BINS];

	BBox currentBboxLeft, currentBboxRight;
	int currentNbLeft = 0, currentNbRight = 0;

	for (int i = 0; i < NB_BINS; ++i) {
		//-----
		// Left side
		// Number of prims
		currentNbLeft += bins[i];
		nbPrimsLeft[i] = currentNbLeft;
		// Prims bbox
		currentBboxLeft = Union(currentBboxLeft, binsBbox[i]);
		bboxesLeft[i] = currentBboxLeft;
		// Surface area
		vLeft[i] = currentBboxLeft.SurfaceArea();


		//-----
		// Right side
		// Number of prims
		int rightIndex = NB_BINS - 1 - i;
		currentNbRight += bins[rightIndex];
		nbPrimsRight[rightIndex] = currentNbRight;
		// Prims bbox
		currentBboxRight = Union(currentBboxRight, binsBbox[rightIndex]);
		bboxesRight[rightIndex] = currentBboxRight;
		// Surface area
		vRight[rightIndex] = currentBboxRight.SurfaceArea();
	}

	int minBin = -1;
	float minCost = INFINITY;
	// Find the best split axis,
	// there must be at least a bin on the right side
	for (int i = 0; i < NB_BINS - 1; ++i) {
		float cost = vLeft[i] * nbPrimsLeft[i] +
				vRight[i + 1] * nbPrimsRight[i + 1];
		if (cost < minCost) {
			minBin = i;
			minCost = cost;
		}
	}

	//-----------------
	// Make the partition, in a "quicksort partitioning" way,
	// the pivot being the position of the split plane
	// (no more binId computation)
	// track also the bboxes (primitives and centroids)
	// for the left and right halves.

	// The split plane coordinate is the coordinate of the end of
	// the chosen bin along the split axis
	float splitPos = centroidsBbox.pMin[axis] + (minBin + 1) *
			(centroidsBbox.pMax[axis] - centroidsBbox.pMin[axis]) / NB_BINS;

	BBox leftChildBbox, rightChildBbox;
	BBox leftChildCentroidsBbox, rightChildCentroidsBbox;

	u_int storeIndex = start;
	for (u_int i = start; i < end; ++i) {
		u_int primIndex = primsIndexes[i];

		if (primsCentroids[primIndex][axis] <= splitPos) {
			// Swap
			primsIndexes[i] = primsIndexes[storeIndex];
			primsIndexes[storeIndex] = primIndex;
			++storeIndex;

			// Update the bounding boxes,
			// this object is on the left side
			leftChildBbox = Union(leftChildBbox, primsBboxes[primIndex]);
			leftChildCentroidsBbox = Union(leftChildCentroidsBbox, primsCentroids[primIndex]);
		} else {
			// Update the bounding boxes,
			// this object is on the right side.
			rightChildBbox = Union(rightChildBbox, primsBboxes[primIndex]);
			rightChildCentroidsBbox = Union(rightChildCentroidsBbox, primsCentroids[primIndex]);
		}
	}

	// Build recursively
	BuildTree(start, storeIndex, primsIndexes, primsBboxes, primsCentroids,
			leftChildBbox, leftChildCentroidsBbox, currentNode,
			leftChildIndex, depth + 1);
	BuildTree(storeIndex, end, primsIndexes, primsBboxes, primsCentroids,
			rightChildBbox, rightChildCentroidsBbox, currentNode,
			rightChildIndex, depth + 1);
}

int32_t  MQBVHAccel::CreateNode(int32_t parentIndex, int32_t childIndex,
	const BBox &nodeBbox) {
	int32_t index = nNodes++; // increment after assignment
	if (nNodes >= maxNodes) {
		QBVHNode *newNodes = AllocAligned<QBVHNode>(2 * maxNodes);
		memcpy(newNodes, nodes, sizeof(QBVHNode) * maxNodes);
		for (u_int i = 0; i < maxNodes; ++i)
			newNodes[maxNodes + i] = QBVHNode();
		FreeAligned(nodes);
		nodes = newNodes;
		maxNodes *= 2;
	}

	if (parentIndex >= 0) {
		nodes[parentIndex].children[childIndex] = index;
		nodes[parentIndex].SetBBox(childIndex, nodeBbox);
	}

	return index;
}

void MQBVHAccel::CreateLeaf(int32_t parentIndex, int32_t childIndex,
		u_int start, const BBox &nodeBbox) {
	// The leaf is directly encoded in the intermediate node.
	if (parentIndex < 0) {
		// The entire tree is a leaf
		nNodes = 1;
		parentIndex = 0;
	}

	QBVHNode &node = nodes[parentIndex];
	node.SetBBox(childIndex, nodeBbox);
	node.InitializeLeaf(childIndex, 1, start);
}

bool MQBVHAccel::Intersect(const Ray *initialRay, RayHit *rayHit) const {
	Ray ray(*initialRay);
	rayHit->SetMiss();

	//------------------------------
	// Prepare the ray for intersection
	QuadRay ray4(ray);
	__m128 invDir[3];
	invDir[0] = _mm_set1_ps(1.f / ray.d.x);
	invDir[1] = _mm_set1_ps(1.f / ray.d.y);
	invDir[2] = _mm_set1_ps(1.f / ray.d.z);

	int signs[3];
	ray.GetDirectionSigns(signs);

	//------------------------------
	// Main loop
	int todoNode = 0; // the index in the stack
	int32_t nodeStack[64];
	nodeStack[0] = 0; // first node to handle: root node

	while (todoNode >= 0) {
		// Leaves are identified by a negative index
		if (!QBVHNode::IsLeaf(nodeStack[todoNode])) {
			QBVHNode &node = nodes[nodeStack[todoNode]];
			--todoNode;

			// It is quite strange but checking here for empty nodes slows down the rendering
			const int32_t visit = node.BBoxIntersect(ray4, invDir, signs);

			switch (visit) {
				case (0x1 | 0x0 | 0x0 | 0x0):
					nodeStack[++todoNode] = node.children[0];
					break;
				case (0x0 | 0x2 | 0x0 | 0x0):
					nodeStack[++todoNode] = node.children[1];
					break;
				case (0x1 | 0x2 | 0x0 | 0x0):
					nodeStack[++todoNode] = node.children[0];
					nodeStack[++todoNode] = node.children[1];
					break;
				case (0x0 | 0x0 | 0x4 | 0x0):
					nodeStack[++todoNode] = node.children[2];
					break;
				case (0x1 | 0x0 | 0x4 | 0x0):
					nodeStack[++todoNode] = node.children[0];
					nodeStack[++todoNode] = node.children[2];
					break;
				case (0x0 | 0x2 | 0x4 | 0x0):
					nodeStack[++todoNode] = node.children[1];
					nodeStack[++todoNode] = node.children[2];
					break;
				case (0x1 | 0x2 | 0x4 | 0x0):
					nodeStack[++todoNode] = node.children[0];
					nodeStack[++todoNode] = node.children[1];
					nodeStack[++todoNode] = node.children[2];
					break;

				case (0x0 | 0x0 | 0x0 | 0x8):
					nodeStack[++todoNode] = node.children[3];
					break;
				case (0x1 | 0x0 | 0x0 | 0x8):
					nodeStack[++todoNode] = node.children[0];
					nodeStack[++todoNode] = node.children[3];
					break;
				case (0x0 | 0x2 | 0x0 | 0x8):
					nodeStack[++todoNode] = node.children[1];
					nodeStack[++todoNode] = node.children[3];
					break;
				case (0x1 | 0x2 | 0x0 | 0x8):
					nodeStack[++todoNode] = node.children[0];
					nodeStack[++todoNode] = node.children[1];
					nodeStack[++todoNode] = node.children[3];
					break;
				case (0x0 | 0x0 | 0x4 | 0x8):
					nodeStack[++todoNode] = node.children[2];
					nodeStack[++todoNode] = node.children[3];
					break;
				case (0x1 | 0x0 | 0x4 | 0x8):
					nodeStack[++todoNode] = node.children[0];
					nodeStack[++todoNode] = node.children[2];
					nodeStack[++todoNode] = node.children[3];
					break;
				case (0x0 | 0x2 | 0x4 | 0x8):
					nodeStack[++todoNode] = node.children[1];
					nodeStack[++todoNode] = node.children[2];
					nodeStack[++todoNode] = node.children[3];
					break;
				case (0x1 | 0x2 | 0x4 | 0x8):
					nodeStack[++todoNode] = node.children[0];
					nodeStack[++todoNode] = node.children[1];
					nodeStack[++todoNode] = node.children[2];
					nodeStack[++todoNode] = node.children[3];
					break;
			}
		} else {
			//----------------------
			// It is a leaf,
			// all the informations are encoded in the index
			const int32_t leafData = nodeStack[todoNode];
			--todoNode;

			if (QBVHNode::IsEmpty(leafData))
				continue;

			const u_int leafIndex = QBVHNode::FirstQuadIndex(leafData);
			QBVHAccel *qbvh = leafs[leafIndex];

			if (leafsTransform[leafIndex]) {
				Ray r(Inverse(*leafsTransform[leafIndex]) * ray);
				RayHit rh;
				if (qbvh->Intersect(&r, &rh)) {
					rayHit->t = rh.t;
					rayHit->b1 = rh.b1;
					rayHit->b2 = rh.b2;
					rayHit->meshIndex = leafIndex;
					rayHit->triangleIndex = rh.triangleIndex;

					ray.maxt = rh.t;
				}
			} else {
				RayHit rh;
				if (qbvh->Intersect(&ray, &rh)) {
					rayHit->t = rh.t;
					rayHit->b1 = rh.b1;
					rayHit->b2 = rh.b2;
					rayHit->meshIndex = leafIndex;
					rayHit->triangleIndex = rh.triangleIndex;

					ray.maxt = rh.t;
				}
			}
		}//end of the else
	}

	return !rayHit->Miss();
}

}