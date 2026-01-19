#pragma once

#include <filesystem>
#include <string>

class FStaticMesh;

struct MeshImportOptions
{
	bool Triangulate = true;
	bool GenerateNormals = true;
	bool GenerateTangents = true;
	bool FlipUVs = false;
	bool Optimize = true;
	bool MergeMeshes = true;
	bool ApplyNodeTransforms = true;
};

class MeshBuilder
{
public:
	// Loads an FBX file into outMesh.
	// Returns true on success; on failure, outError (if provided) will contain a readable reason.
	static bool LoadFromFBX(const std::filesystem::path& fbxPath, FStaticMesh& outMesh, std::string* outError = nullptr, const MeshImportOptions& options = {});
};
