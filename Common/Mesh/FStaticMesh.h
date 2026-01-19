#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include <DirectXMath.h>
#include <d3d12.h>

struct FStaticMeshVertex
{
	DirectX::XMFLOAT3 Position;
	DirectX::XMFLOAT3 Normal;
	DirectX::XMFLOAT2 UV0;
	DirectX::XMFLOAT3 Tangent;
};

static_assert(sizeof(FStaticMeshVertex) == 44, "FStaticMeshVertex must match the sample input layout stride (44 bytes)");

struct FStaticMeshSection
{
	std::string Name;
	uint32_t MaterialIndex = 0;
	uint32_t IndexStart = 0;
	uint32_t IndexCount = 0;
	int32_t VertexBase = 0;
};

class FStaticMesh
{
public:
	std::vector<FStaticMeshVertex> Vertices;
	std::vector<uint32_t> Indices;
	std::vector<FStaticMeshSection> Sections;
	std::vector<std::string> MaterialNames;

	DirectX::XMFLOAT3 BoundsMin = { 0,0,0 };
	DirectX::XMFLOAT3 BoundsMax = { 0,0,0 };

	void Clear()
	{
		Vertices.clear();
		Indices.clear();
		Sections.clear();
		MaterialNames.clear();
		BoundsMin = { 0,0,0 };
		BoundsMax = { 0,0,0 };
	}

	bool IsValid() const
	{
		return !Vertices.empty() && !Indices.empty();
	}

	void RecomputeBounds()
	{
		if (Vertices.empty())
		{
			BoundsMin = { 0,0,0 };
			BoundsMax = { 0,0,0 };
			return;
		}

		BoundsMin = Vertices[0].Position;
		BoundsMax = Vertices[0].Position;

		for (const auto& v : Vertices)
		{
			BoundsMin.x = (v.Position.x < BoundsMin.x) ? v.Position.x : BoundsMin.x;
			BoundsMin.y = (v.Position.y < BoundsMin.y) ? v.Position.y : BoundsMin.y;
			BoundsMin.z = (v.Position.z < BoundsMin.z) ? v.Position.z : BoundsMin.z;
			BoundsMax.x = (v.Position.x > BoundsMax.x) ? v.Position.x : BoundsMax.x;
			BoundsMax.y = (v.Position.y > BoundsMax.y) ? v.Position.y : BoundsMax.y;
			BoundsMax.z = (v.Position.z > BoundsMax.z) ? v.Position.z : BoundsMax.z;
		}
	}

	static constexpr uint32_t VertexStrideBytes()
	{
		return static_cast<uint32_t>(sizeof(FStaticMeshVertex));
	}

	static constexpr DXGI_FORMAT IndexFormat()
	{
		return DXGI_FORMAT_R32_UINT;
	}

	static inline const D3D12_INPUT_ELEMENT_DESC* InputLayout(uint32_t& outCount)
	{
		static const D3D12_INPUT_ELEMENT_DESC layout[] =
		{
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,  D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{ "NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,    0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{ "TANGENT",  0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 32, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		};
		outCount = static_cast<uint32_t>(_countof(layout));
		return layout;
	}
};
