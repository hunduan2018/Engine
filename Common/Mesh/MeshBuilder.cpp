#include "../../src/stdafx.h"

#include "MeshBuilder.h"
#include "FStaticMesh.h"

#include <cmath>
#include <cstring>

#ifdef MYENGINE_WITH_ASSIMP
#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#endif

namespace
{
	static void SetError(std::string* outError, const std::string& msg)
	{
		if (outError)
		{
			*outError = msg;
		}
	}

#ifdef MYENGINE_WITH_ASSIMP
	static FVector3 TransformPosition(const aiMatrix4x4& m, const aiVector3D& v)
	{
		aiVector3D r = m * v;
		return { r.x, r.y, r.z };
	}

	static FVector3 TransformDirection(const aiMatrix4x4& m, const aiVector3D& v)
	{

		// Use the upper-left 3x3 only.
		aiVector3D r;
		r.x = m.a1 * v.x + m.a2 * v.y + m.a3 * v.z;
		r.y = m.b1 * v.x + m.b2 * v.y + m.b3 * v.z;
		r.z = m.c1 * v.x + m.c2 * v.y + m.c3 * v.z;
		// Normalize (safe for zero)
		const float len2 = r.x * r.x + r.y * r.y + r.z * r.z;
		if (len2 > 0.0f)
		{
			const float invLen = 1.0f / std::sqrt(len2);
			r.x *= invLen;
			r.y *= invLen;
			r.z *= invLen;
		}
		return { r.x, r.y, r.z };
	}

	static std::string GetMaterialName(const aiScene* scene, unsigned materialIndex)
	{
		if (!scene || materialIndex >= scene->mNumMaterials)
		{
			return {};
		}
		aiString name;
		if (scene->mMaterials[materialIndex]->Get(AI_MATKEY_NAME, name) == AI_SUCCESS)
		{
			return std::string(name.C_Str());
		}
		return {};
	}

	struct CollectedMesh
	{
		aiMesh* Mesh = nullptr;
		aiMatrix4x4 GlobalTransform;
	};

	static void CollectMeshesRecursive(const aiScene* scene, aiNode* node, const aiMatrix4x4& parent, std::vector<CollectedMesh>& out)
	{
		if (!scene || !node)
		{
			return;
		}

		aiMatrix4x4 global = parent * node->mTransformation;

		for (unsigned i = 0; i < node->mNumMeshes; ++i)
		{
			unsigned meshIndex = node->mMeshes[i];
			if (meshIndex < scene->mNumMeshes)
			{
				out.push_back({ scene->mMeshes[meshIndex], global });
			}
		}

		for (unsigned c = 0; c < node->mNumChildren; ++c)
		{
			CollectMeshesRecursive(scene, node->mChildren[c], global, out);
		}
	}

	static bool AppendAssimpMesh(const aiScene* scene, const CollectedMesh& cm, FStaticMesh& outMesh, bool applyTransform)
	{
		aiMesh* mesh = cm.Mesh;
		if (!mesh)
		{
			return false;
		}

		const uint32_t baseVertex = static_cast<uint32_t>(outMesh.Vertices.size());
		const uint32_t startIndex = static_cast<uint32_t>(outMesh.Indices.size());

		const aiMatrix4x4& xform = cm.GlobalTransform;

		outMesh.Vertices.reserve(outMesh.Vertices.size() + mesh->mNumVertices);
		for (unsigned v = 0; v < mesh->mNumVertices; ++v)
		{
			const aiVector3D& pos = mesh->mVertices[v];
			const aiVector3D& nrm = mesh->HasNormals() ? mesh->mNormals[v] : aiVector3D(0, 1, 0);
			const aiVector3D& tan = (mesh->HasTangentsAndBitangents() && mesh->mTangents) ? mesh->mTangents[v] : aiVector3D(1, 0, 0);

			aiVector3D uv(0, 0, 0);
			if (mesh->HasTextureCoords(0) && mesh->mTextureCoords[0])
			{
				uv = mesh->mTextureCoords[0][v];
			}

			FStaticMeshVertex vert{};
			if (applyTransform)
			{
				vert.Position = TransformPosition(xform, pos);
				vert.Normal = TransformDirection(xform, nrm);
				vert.Tangent = TransformDirection(xform, tan);
			}
			else
			{
				vert.Position = { pos.x, pos.y, pos.z };
				vert.Normal = { nrm.x, nrm.y, nrm.z };
				vert.Tangent = { tan.x, tan.y, tan.z };
			}
			vert.UV0 = { uv.x, uv.y };

			outMesh.Vertices.push_back(vert);
		}

		// Indices: triangulated only.
		for (unsigned f = 0; f < mesh->mNumFaces; ++f)
		{
			const aiFace& face = mesh->mFaces[f];
			if (face.mNumIndices != 3)
			{
				continue;
			}

			outMesh.Indices.push_back(baseVertex + static_cast<uint32_t>(face.mIndices[0]));
			outMesh.Indices.push_back(baseVertex + static_cast<uint32_t>(face.mIndices[1]));
			outMesh.Indices.push_back(baseVertex + static_cast<uint32_t>(face.mIndices[2]));
		}

		const uint32_t endIndex = static_cast<uint32_t>(outMesh.Indices.size());

		FStaticMeshSection sec;
		sec.MaterialIndex = mesh->mMaterialIndex;
		sec.IndexStart = startIndex;
		sec.IndexCount = endIndex - startIndex;
		sec.VertexBase = 0;
		sec.Name = mesh->mName.length ? std::string(mesh->mName.C_Str()) : std::string();
		outMesh.Sections.push_back(std::move(sec));

		// Material name cache (best-effort)
		if (scene)
		{
			if (outMesh.MaterialNames.size() < scene->mNumMaterials)
			{
				outMesh.MaterialNames.resize(scene->mNumMaterials);
			}
			if (sec.MaterialIndex < outMesh.MaterialNames.size() && outMesh.MaterialNames[sec.MaterialIndex].empty())
			{
				outMesh.MaterialNames[sec.MaterialIndex] = GetMaterialName(scene, sec.MaterialIndex);
			}
		}

		return true;
	}
#endif
}

bool MeshBuilder::LoadFromFBX(const std::filesystem::path& fbxPath, FStaticMesh& outMesh, std::string* outError, const MeshImportOptions& options)
{
	outMesh.Clear();

	if (fbxPath.empty())
	{
		SetError(outError, "FBX path is empty");
		return false;
	}

	if (!std::filesystem::exists(fbxPath))
	{
		SetError(outError, "FBX file not found: " + fbxPath.u8string());
		return false;
	}

#ifndef MYENGINE_WITH_ASSIMP
	SetError(outError, "FBX import is disabled (Assimp not available). Enable Assimp and rebuild with MYENGINE_WITH_ASSIMP.");
	return false;
#else
	Assimp::Importer importer;

	unsigned flags = 0;
	if (options.Triangulate) flags |= aiProcess_Triangulate;
	flags |= aiProcess_JoinIdenticalVertices;
	flags |= aiProcess_SortByPType;
	flags |= aiProcess_ImproveCacheLocality;
	if (options.GenerateNormals) flags |= aiProcess_GenSmoothNormals;
	if (options.GenerateTangents) flags |= aiProcess_CalcTangentSpace;
	if (options.FlipUVs) flags |= aiProcess_FlipUVs;
	if (options.Optimize) flags |= (aiProcess_OptimizeMeshes | aiProcess_OptimizeGraph);

	// NOTE: We keep Assimp's default handedness (commonly right-handed). This project uses RH camera matrices.
	const aiScene* scene = importer.ReadFile(fbxPath.u8string(), flags);
	if (!scene)
	{
		SetError(outError, std::string("Assimp failed: ") + importer.GetErrorString());
		return false;
	}

	if (!scene->HasMeshes())
	{
		SetError(outError, "FBX contains no meshes");
		return false;
	}

	std::vector<CollectedMesh> meshes;
	meshes.reserve(scene->mNumMeshes);
	aiMatrix4x4 identity;
	CollectMeshesRecursive(scene, scene->mRootNode, identity, meshes);
	if (meshes.empty())
	{
		SetError(outError, "FBX scene graph contains no mesh nodes");
		return false;
	}

	bool any = false;
	for (const auto& cm : meshes)
	{
		if (!cm.Mesh || cm.Mesh->mPrimitiveTypes == 0)
		{
			continue;
		}
		any |= AppendAssimpMesh(scene, cm, outMesh, options.ApplyNodeTransforms);
		if (!options.MergeMeshes && any)
		{
			break;
		}
	}

	if (!any || !outMesh.IsValid())
	{
		SetError(outError, "No valid triangle meshes were imported");
		outMesh.Clear();
		return false;
	}

	outMesh.RecomputeBounds();
	return true;
#endif
}
