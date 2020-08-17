#include <DustEngine/Render/DX12/RsrcMngrDX12.h>

#include <DustEngine/Core/Texture2D.h>
#include <DustEngine/Core/Image.h>

#include <unordered_map>
#include <iostream>

using namespace Ubpa::DustEngine;
using namespace Ubpa;
using namespace std;

struct RsrcMngrDX12::Impl {
	struct Texture2DGPUData {
		ID3D12Resource* resource;
		UDX12::DescriptorHeapAllocation allocationSRV;
	};
	struct RenderTargetGPUData {
		vector<ID3D12Resource*> resources;
		UDX12::DescriptorHeapAllocation allocationSRV;
		UDX12::DescriptorHeapAllocation allocationRTV;
	};

	bool isInit{ false };
	ID3D12Device* device{ nullptr };
	DirectX::ResourceUploadBatch* upload{ nullptr };

	unordered_map<size_t, Texture2DGPUData> texture2DMap;
	unordered_map<size_t, RenderTargetGPUData> renderTargetMap;

	unordered_map<size_t, UDX12::MeshGPUBuffer> meshGeoMap;
	unordered_map<size_t, ID3DBlob*> shaderByteCodeMap;
	unordered_map<size_t, ID3D12RootSignature*> rootSignatureMap;
	unordered_map<size_t, ID3D12PipelineState*> PSOMap;

	const CD3DX12_STATIC_SAMPLER_DESC pointWrap{
		0,                               // shaderRegister
		D3D12_FILTER_MIN_MAG_MIP_POINT,  // filter
		D3D12_TEXTURE_ADDRESS_MODE_WRAP, // addressU
		D3D12_TEXTURE_ADDRESS_MODE_WRAP, // addressV
		D3D12_TEXTURE_ADDRESS_MODE_WRAP  // addressW
	};

	const CD3DX12_STATIC_SAMPLER_DESC pointClamp{
		1,                                // shaderRegister
		D3D12_FILTER_MIN_MAG_MIP_POINT,   // filter
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP, // addressU
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP, // addressV
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP  // addressW
	};

	const CD3DX12_STATIC_SAMPLER_DESC linearWrap{
		2,                               // shaderRegister
		D3D12_FILTER_MIN_MAG_MIP_LINEAR, // filter
		D3D12_TEXTURE_ADDRESS_MODE_WRAP, // addressU
		D3D12_TEXTURE_ADDRESS_MODE_WRAP, // addressV
		D3D12_TEXTURE_ADDRESS_MODE_WRAP  // addressW
	};

	const CD3DX12_STATIC_SAMPLER_DESC linearClamp{
		3,                                // shaderRegister
		D3D12_FILTER_MIN_MAG_MIP_LINEAR,  // filter
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP, // addressU
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP, // addressV
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP  // addressW
	};

	const CD3DX12_STATIC_SAMPLER_DESC anisotropicWrap{
		4,                               // shaderRegister
		D3D12_FILTER_ANISOTROPIC,        // filter
		D3D12_TEXTURE_ADDRESS_MODE_WRAP, // addressU
		D3D12_TEXTURE_ADDRESS_MODE_WRAP, // addressV
		D3D12_TEXTURE_ADDRESS_MODE_WRAP, // addressW
		0.0f,                            // mipLODBias
		8                                // maxAnisotropy
	};

	const CD3DX12_STATIC_SAMPLER_DESC anisotropicClamp{
		5,                                // shaderRegister
		D3D12_FILTER_ANISOTROPIC,         // filter
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP, // addressU
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP, // addressV
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP, // addressW
		0.0f,                             // mipLODBias
		8                                 // maxAnisotropy
	};
};

RsrcMngrDX12::RsrcMngrDX12()
	: pImpl(new Impl)
{
}

RsrcMngrDX12::~RsrcMngrDX12() {
	assert(!pImpl->isInit);
	delete(pImpl);
}

RsrcMngrDX12& RsrcMngrDX12::Init(ID3D12Device* device) {
	assert(!pImpl->isInit);

	pImpl->device = device;
	pImpl->upload = new DirectX::ResourceUploadBatch{ device };

	pImpl->isInit = true;
	return *this;
}

void RsrcMngrDX12::Clear() {
	assert(pImpl->isInit);

	for (auto& [name, tex] : pImpl->texture2DMap) {
		UDX12::DescriptorHeapMngr::Instance().GetCSUGpuDH()->Free(move(tex.allocationSRV));
		tex.resource->Release();
	}

	for (auto& [name, tex] : pImpl->renderTargetMap) {
		UDX12::DescriptorHeapMngr::Instance().GetCSUGpuDH()->Free(move(tex.allocationSRV));
		UDX12::DescriptorHeapMngr::Instance().GetRTVCpuDH()->Free(move(tex.allocationRTV));
		for (auto rsrc : tex.resources)
			rsrc->Release();
	}

	for (auto& [name, rootSig] : pImpl->rootSignatureMap)
		rootSig->Release();

	for (auto& [name, PSO] : pImpl->PSOMap)
		PSO->Release();

	pImpl->device = nullptr;
	delete pImpl->upload;

	pImpl->texture2DMap.clear();
	pImpl->renderTargetMap.clear();
	pImpl->meshGeoMap.clear();
	pImpl->rootSignatureMap.clear();
	pImpl->PSOMap.clear();

	pImpl->isInit = false;
}

DirectX::ResourceUploadBatch& RsrcMngrDX12::GetUpload() const {
	return *pImpl->upload;
}

//RsrcMngrDX12& RsrcMngrDX12::RegisterTexture2D(
//	DirectX::ResourceUploadBatch& upload,
//	size_t id,
//	wstring_view filename)
//{
//	return RegisterDDSTextureArrayFromFile(upload, id, &filename, 1);
//}
//
//RsrcMngrDX12& RsrcMngrDX12::RegisterDDSTextureArrayFromFile(DirectX::ResourceUploadBatch& upload,
//	size_t id, const wstring_view* filenameArr, UINT num)
//{
//	Impl::Texture tex;
//	tex.resources.resize(num);
//
//	tex.allocationSRV = UDX12::DescriptorHeapMngr::Instance().GetCSUGpuDH()->Allocate(num);
//
//	for (UINT i = 0; i < num; i++) {
//		bool isCubeMap;
//		DirectX::CreateDDSTextureFromFile(
//			pImpl->device,
//			upload,
//			filenameArr[i].data(),
//			&tex.resources[i],
//			false,
//			0,
//			nullptr,
//			&isCubeMap);
//
//		D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc =
//			isCubeMap ?
//			UDX12::Desc::SRV::TexCube(tex.resources[i]->GetDesc().Format)
//			: UDX12::Desc::SRV::Tex2D(tex.resources[i]->GetDesc().Format);
//
//		pImpl->device->CreateShaderResourceView(tex.resources[i], &srvDesc, tex.allocationSRV.GetCpuHandle(i));
//	}
//
//	pImpl->textureMap.emplace(id, move(tex));
//
//	return *this;
//}

RsrcMngrDX12& RsrcMngrDX12::RegisterTexture2D(
	DirectX::ResourceUploadBatch& upload,
	const Texture2D* tex2D)
{
	Impl::Texture2DGPUData tex;

	tex.allocationSRV = UDX12::DescriptorHeapMngr::Instance().GetCSUGpuDH()->Allocate(static_cast<uint32_t>(1));

	constexpr DXGI_FORMAT channelMap[] = {
		DXGI_FORMAT::DXGI_FORMAT_R32_FLOAT,
		DXGI_FORMAT::DXGI_FORMAT_R32G32_FLOAT,
		DXGI_FORMAT::DXGI_FORMAT_R32G32B32_FLOAT,
		DXGI_FORMAT::DXGI_FORMAT_R32G32B32A32_FLOAT,
	};

	D3D12_SUBRESOURCE_DATA data;
	data.pData = tex2D->image->data;
	data.RowPitch = tex2D->image->width * tex2D->image->channel * sizeof(float);
	data.SlicePitch = tex2D->image->height * data.RowPitch;

	DirectX::CreateTextureFromMemory(
		pImpl->device,
		upload,
		tex2D->image->width.get(),
		tex2D->image->height.get(),
		channelMap[tex2D->image->channel - 1],
		data,
		&tex.resource
	);

	pImpl->device->CreateShaderResourceView(
		tex.resource,
		&UDX12::Desc::SRV::Tex2D(tex.resource->GetDesc().Format),
		tex.allocationSRV.GetCpuHandle(static_cast<uint32_t>(0))
	);

	pImpl->texture2DMap.emplace(tex2D->GetInstanceID(), move(tex));

	return *this;
}

//RsrcMngrDX12& RsrcMngrDX12::RegisterTexture2DArray(
//	DirectX::ResourceUploadBatch& upload,
//	size_t id, const Texture2D** tex2Ds, size_t num)
//{
//	Impl::Texture tex;
//	tex.resources.resize(num);
//
//	tex.allocationSRV = UDX12::DescriptorHeapMngr::Instance().GetCSUGpuDH()->Allocate(static_cast<uint32_t>(num));
//
//	constexpr DXGI_FORMAT channelMap[] = {
//		DXGI_FORMAT::DXGI_FORMAT_R32_FLOAT,
//		DXGI_FORMAT::DXGI_FORMAT_R32G32_FLOAT,
//		DXGI_FORMAT::DXGI_FORMAT_R32G32B32_FLOAT,
//		DXGI_FORMAT::DXGI_FORMAT_R32G32B32A32_FLOAT,
//	};
//
//	for (size_t i = 0; i < num; i++) {
//		D3D12_SUBRESOURCE_DATA data;
//		data.pData = tex2Ds[i]->image->data;
//		data.RowPitch = tex2Ds[i]->image->width * tex2Ds[i]->image->channel * sizeof(float);
//		data.SlicePitch = tex2Ds[i]->image->height * data.RowPitch;
//		
//		DirectX::CreateTextureFromMemory(
//			pImpl->device,
//			upload,
//			tex2Ds[i]->image->width.get(),
//			tex2Ds[i]->image->height.get(),
//			channelMap[tex2Ds[i]->image->channel - 1],
//			data,
//			&tex.resources[i]
//		);
//
//		pImpl->device->CreateShaderResourceView(
//			tex.resources[i],
//			&UDX12::Desc::SRV::Tex2D(tex.resources[i]->GetDesc().Format),
//			tex.allocationSRV.GetCpuHandle(static_cast<uint32_t>(i))
//		);
//	}
//
//	pImpl->textureMap.emplace(id, move(tex));
//
//	return *this;
//}

D3D12_CPU_DESCRIPTOR_HANDLE RsrcMngrDX12::GetTexture2DSrvCpuHandle(const Texture2D* tex2D) const {
	return pImpl->texture2DMap.find(tex2D->GetInstanceID())->second.allocationSRV.GetCpuHandle(0);
}
D3D12_GPU_DESCRIPTOR_HANDLE RsrcMngrDX12::GetTexture2DSrvGpuHandle(const Texture2D* tex2D) const {
	return pImpl->texture2DMap.find(tex2D->GetInstanceID())->second.allocationSRV.GetGpuHandle(0);
}

//UDX12::DescriptorHeapAllocation& RsrcMngrDX12::GetTextureRtvs(const Texture2D* tex2D) const {
//	return pImpl->textureMap.find(tex2D->GetInstanceID())->second.allocationRTV;
//}

UDX12::MeshGPUBuffer& RsrcMngrDX12::RegisterStaticMeshGPUBuffer(
	DirectX::ResourceUploadBatch& upload, size_t id,
	const void* vb_data, UINT vb_count, UINT vb_stride,
	const void* ib_data, UINT ib_count, DXGI_FORMAT ib_format)
{
	auto& meshGeo = pImpl->meshGeoMap[id];
	meshGeo.InitBuffer(pImpl->device, upload,
		vb_data, vb_count, vb_stride,
		ib_data, ib_count, ib_format
	);
	return meshGeo;
}

UDX12::MeshGPUBuffer& RsrcMngrDX12::RegisterDynamicMeshGPUBuffer(
	size_t id,
	const void* vb_data, UINT vb_count, UINT vb_stride,
	const void* ib_data, UINT ib_count, DXGI_FORMAT ib_format)
{
	auto& meshGeo = pImpl->meshGeoMap[id];
	meshGeo.InitBuffer(pImpl->device,
		vb_data, vb_count, vb_stride,
		ib_data, ib_count, ib_format);
	return meshGeo;
}

UDX12::MeshGPUBuffer& RsrcMngrDX12::GetMeshGPUBuffer(size_t id) const {
	return pImpl->meshGeoMap.find(id)->second;
}

ID3DBlob* RsrcMngrDX12::RegisterShaderByteCode(
	size_t id,
	const wstring& filename,
	const D3D_SHADER_MACRO* defines,
	const string& entrypoint,
	const string& target)
{
	auto shader = UDX12::Util::CompileShaderFromFile(filename, defines, entrypoint, target);
	auto ptr = shader.Detach();
	pImpl->shaderByteCodeMap.emplace(id, ptr);
	return ptr;
}

ID3DBlob* RsrcMngrDX12::RegisterShaderByteCode(
	std::size_t id,
	std::string_view source,
	const D3D_SHADER_MACRO* defines,
	const std::string& entrypoint,
	const std::string& target)
{
	auto shader = UDX12::Util::CompileShader(source, defines, entrypoint, target);
	auto ptr = shader.Detach();
	pImpl->shaderByteCodeMap.emplace(id, ptr);
	return ptr;
}

ID3DBlob* RsrcMngrDX12::GetShaderByteCode(size_t id) const {
	return pImpl->shaderByteCodeMap.find(id)->second;
}

//RsrcMngrDX12& RsrcMngrDX12::RegisterRenderTexture2D(size_t id, UINT width, UINT height, DXGI_FORMAT format) {
//	Impl::Texture tex;
//	tex.resources.resize(1);
//
//	tex.allocationSRV = UDX12::DescriptorHeapMngr::Instance().GetCSUGpuDH()->Allocate(1);
//	tex.allocationRTV = UDX12::DescriptorHeapMngr::Instance().GetRTVCpuDH()->Allocate(1);
//
//	// create resource
//	D3D12_RESOURCE_DESC texDesc;
//	ZeroMemory(&texDesc, sizeof(D3D12_RESOURCE_DESC));
//	texDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
//	texDesc.Width = width;
//	texDesc.Height = height;
//	texDesc.MipLevels = 1;
//	texDesc.Format = format;
//	texDesc.SampleDesc.Count = 1;
//	texDesc.SampleDesc.Quality = 0;
//	texDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
//	texDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
//	ThrowIfFailed(pImpl->device->CreateCommittedResource(
//		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
//		D3D12_HEAP_FLAG_NONE, &texDesc,
//		D3D12_RESOURCE_STATE_GENERIC_READ,
//		nullptr,
//		IID_PPV_ARGS(&tex.resources[0])));
//
//	// create SRV
//	pImpl->device->CreateShaderResourceView(
//		tex.resources[0],
//		&UDX12::Desc::SRV::Tex2D(format),
//		tex.allocationSRV.GetCpuHandle());
//
//	// create RTVs
//	D3D12_RENDER_TARGET_VIEW_DESC rtvDesc;
//	ZeroMemory(&rtvDesc, sizeof(rtvDesc));
//	rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2DARRAY;
//	rtvDesc.Format = format;
//	rtvDesc.Texture2D.MipSlice = 0;
//	rtvDesc.Texture2D.PlaneSlice = 0; // ?
//	pImpl->device->CreateRenderTargetView(tex.resources[0], &rtvDesc, tex.allocationRTV.GetCpuHandle());
//
//	pImpl->textureMap.emplace(id, move(tex));
//
//	return *this;
//}
//
//RsrcMngrDX12& RsrcMngrDX12::RegisterRenderTextureCube(size_t id, UINT size, DXGI_FORMAT format) {
//	Impl::Texture tex;
//	tex.resources.resize(1);
//
//	tex.allocationSRV = UDX12::DescriptorHeapMngr::Instance().GetCSUGpuDH()->Allocate(1);
//	tex.allocationRTV = UDX12::DescriptorHeapMngr::Instance().GetRTVCpuDH()->Allocate(6);
//
//	// create resource
//	D3D12_RESOURCE_DESC texDesc;
//	ZeroMemory(&texDesc, sizeof(D3D12_RESOURCE_DESC));
//	texDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
//	texDesc.Alignment = 0;
//	texDesc.Width = size;
//	texDesc.Height = size;
//	texDesc.DepthOrArraySize = 6;
//	texDesc.MipLevels = 1;
//	texDesc.Format = format;
//	texDesc.SampleDesc.Count = 1;
//	texDesc.SampleDesc.Quality = 0;
//	texDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
//	texDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
//	ThrowIfFailed(pImpl->device->CreateCommittedResource(
//		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
//		D3D12_HEAP_FLAG_NONE, &texDesc,
//		D3D12_RESOURCE_STATE_GENERIC_READ,
//		nullptr,
//		IID_PPV_ARGS(&tex.resources[0])));
//
//	// create SRV
//	pImpl->device->CreateShaderResourceView(
//		tex.resources[0],
//		&UDX12::Desc::SRV::TexCube(format),
//		tex.allocationSRV.GetCpuHandle());
//
//	// create RTVs
//	for (UINT i = 0; i < 6; i++)
//	{
//		D3D12_RENDER_TARGET_VIEW_DESC rtvDesc;
//		ZeroMemory(&rtvDesc, sizeof(rtvDesc));
//		rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2DARRAY;
//		rtvDesc.Format = format;
//		rtvDesc.Texture2DArray.MipSlice = 0;
//		rtvDesc.Texture2DArray.PlaneSlice = 0;
//		rtvDesc.Texture2DArray.FirstArraySlice = i;
//		rtvDesc.Texture2DArray.ArraySize = 1;
//		pImpl->device->CreateRenderTargetView(tex.resources[0], &rtvDesc, tex.allocationRTV.GetCpuHandle(i));
//	}
//
//	pImpl->textureMap.emplace(id, move(tex));
//
//	return *this;
//}

RsrcMngrDX12& RsrcMngrDX12::RegisterRootSignature(
	size_t id,
	const D3D12_ROOT_SIGNATURE_DESC* desc
)
{
	// create a root signature with a single slot which points to a descriptor range consisting of a single constant buffer
	ID3DBlob* serializedRootSig = nullptr;
	ID3DBlob* errorBlob = nullptr;

	HRESULT hr = D3D12SerializeRootSignature(desc, D3D_ROOT_SIGNATURE_VERSION_1,
		&serializedRootSig, &errorBlob);

	if (errorBlob != nullptr)
	{
		::OutputDebugStringA((char*)errorBlob->GetBufferPointer());
		errorBlob->Release();
	}
	ThrowIfFailed(hr);

	ID3D12RootSignature* rootSig;

	ThrowIfFailed(pImpl->device->CreateRootSignature(
		0,
		serializedRootSig->GetBufferPointer(),
		serializedRootSig->GetBufferSize(),
		IID_PPV_ARGS(&rootSig)));

	pImpl->rootSignatureMap.emplace(id, rootSig);

	serializedRootSig->Release();

	return *this;
}

ID3D12RootSignature* RsrcMngrDX12::GetRootSignature(size_t id) const {
	return pImpl->rootSignatureMap.find(id)->second;
}

RsrcMngrDX12& RsrcMngrDX12::RegisterPSO(
	size_t id,
	const D3D12_GRAPHICS_PIPELINE_STATE_DESC* desc)
{
	ID3D12PipelineState* pso;
	pImpl->device->CreateGraphicsPipelineState(desc, IID_PPV_ARGS(&pso));
	pImpl->PSOMap.emplace(id, pso);
	return *this;
}

ID3D12PipelineState* RsrcMngrDX12::GetPSO(size_t id) const {
	return pImpl->PSOMap.find(id)->second;
}

std::array<CD3DX12_STATIC_SAMPLER_DESC, 6> RsrcMngrDX12::GetStaticSamplers() const {
	return {
		pImpl->pointWrap, pImpl->pointClamp,
		pImpl->linearWrap, pImpl->linearClamp,
		pImpl->anisotropicWrap, pImpl->anisotropicClamp
	};
}