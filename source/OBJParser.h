/*=============================================================================*/
// Copyright 2021 Elite Engine 2.0
// Authors: Thomas Goussaert
/*=============================================================================*/
// EOBJParser.h: most basic OBJParser!
/*=============================================================================*/

#include <string>
#include <fstream>
#include <vector>
#include "Mesh.h"
#include "Material.h"
#include "Scene.h"
#include "LitMaterial.h"
#include "Texture.h"

#include "ResourceManager.h"
#include "MaterialManager.h"
#include "Logger.h"
#include "Utils.h"

struct Mesh_Struct 
{
	std::vector<Vertex> vertices;
	std::vector<u_int> indices;
	std::string materialName;
};

static void CreateMesh(std::vector<Mesh*>& pMeshes,  Mesh_Struct& mesh, Scene* pScene, const std::string& filepath)
{
	if (mesh.vertices.empty() || mesh.indices.empty())
		return;

	for (uint32_t i = 0; i < mesh.indices.size(); i += 3)
	{
		uint32_t index0 = mesh.indices[i];
		uint32_t index1 = mesh.indices[i + 1];
		uint32_t index2 = mesh.indices[i + 2];

		const DirectX::XMVECTOR& p0 = DirectX::XMLoadFloat3( &mesh.vertices[index0].position);
		const DirectX::XMVECTOR& p1 = DirectX::XMLoadFloat3( &mesh.vertices[index1].position);
		const DirectX::XMVECTOR& p2 = DirectX::XMLoadFloat3( &mesh.vertices[index2].position);
		const DirectX::XMFLOAT2& uv0 = mesh.vertices[index0].uv;
		const DirectX::XMFLOAT2& uv1 = mesh.vertices[index1].uv;
		const DirectX::XMFLOAT2& uv2 = mesh.vertices[index2].uv;

		const DirectX::XMVECTOR edge0 = DirectX::XMVectorSubtract(p1 , p0);
		const DirectX::XMVECTOR edge1 = DirectX::XMVectorSubtract(p2 , p0);

		const auto diffx = DirectX::XMFLOAT2(uv1.x - uv0.x, uv2.x - uv0.x);
		const auto diffy = DirectX::XMFLOAT2(uv1.y - uv0.y, uv2.y - uv0.y);

		const DirectX::XMVECTOR diffX = DirectX::XMLoadFloat2(&diffx);
		const DirectX::XMVECTOR diffY = DirectX::XMLoadFloat2(&diffy);

		float one = 1.0f;
		auto oneVector = DirectX::XMLoadFloat(&one);
		auto r = DirectX::XMVectorDivide( oneVector , DirectX::XMVector3Cross(diffX, diffY));

		DirectX::XMFLOAT2 storedDiff;
		DirectX::XMStoreFloat2(&storedDiff, diffY);

		DirectX::XMVECTOR tangent0 = DirectX::XMLoadFloat3(&mesh.vertices[index0].tangent);
		DirectX::XMVECTOR tangent1 = DirectX::XMLoadFloat3(&mesh.vertices[index1].tangent);
		DirectX::XMVECTOR tangent2 = DirectX::XMLoadFloat3(&mesh.vertices[index2].tangent);

		DirectX::XMVECTOR tangent = DirectX::XMVectorMultiply( DirectX::XMVectorSubtract( DirectX::XMVectorMultiply(edge0 , DirectX::XMLoadFloat(&storedDiff.y)) , DirectX::XMVectorMultiply(edge1 , DirectX::XMLoadFloat(&storedDiff.x))) , r);

		DirectX::XMVectorAdd(tangent0, tangent);
		DirectX::XMVectorAdd(tangent1, tangent);
		DirectX::XMVectorAdd(tangent2, tangent);

		DirectX::XMStoreFloat3(&mesh.vertices[index0].tangent, tangent0);
		DirectX::XMStoreFloat3(&mesh.vertices[index1].tangent, tangent1);
		DirectX::XMStoreFloat3(&mesh.vertices[index2].tangent, tangent2);
	}
	for (auto& v : mesh.vertices)
	{
		auto tangent = DirectX::XMLoadFloat3(&v.tangent);
		auto normal = DirectX::XMLoadFloat3(&v.normal);

		auto project = DirectX::XMVectorMultiply(normal , DirectX::XMVectorDivide(DirectX::XMVector3Dot(normal, tangent) , DirectX::XMVectorSqrt(normal) ) ) ;

		tangent = DirectX::XMVector3Normalize(DirectX::XMVectorSubtract(tangent, project));

		DirectX::XMStoreFloat3(&v.tangent, tangent);
	}

	//auto mat = new Material(MyEngine::GetSingleton()->GetDevice(), L"Resources/material_unlit.fx");
	//
	//Texture* pDiffuseTexture = new Texture(MyEngine::GetSingleton()->GetDevice(), L"Resources/uv_grid_2.png");
	//mat->SetDiffuseMap(pDiffuseTexture);

	Mesh* pMesh = new Mesh(
		MyEngine::GetSingleton()->GetDevice(),
		MyEngine::GetSingleton()->GetWindowHandle(),
		mesh.vertices,
		mesh.indices,
		filepath,
		static_cast<int>(pMeshes.size()),
		pScene->GetMaterial(mesh.materialName)
	);
	pMeshes.push_back(pMesh);
}

static Mesh* CreateSphere(float radius, int steps)
{
	auto resourceManager = ResourceManager::GetInstance();
	std::string name = "Cube(" + std::to_string(radius) + ", " + std::to_string(steps)  + ")";
	if (resourceManager->GetMeshConst(name) != nullptr)
		return resourceManager->GetMeshConst(name);

	// Vertices
	std::vector<Vertex> vertices;
	std::vector<uint32_t> indices;

	const auto vertCount = steps * (steps - 1) + 2;
	const float deltaTheta = F_PI / steps;
	const float deltaPhi = (G_PI * 2.0f) / steps;
	float theta = 0;
	float phi = 0;

	vertices.emplace_back(DirectX::XMFLOAT3{ 0, radius, 0 }, DirectX::XMFLOAT3{ 0,1,0 });

	for (auto i = 0; i < steps - 1; ++i)
	{
		theta += deltaTheta;
		for (auto j = 0; j < steps; ++j)
		{
			phi += deltaPhi;
			DirectX::XMFLOAT3 pos;
			pos.x = radius * sin(theta) * cos(phi);
			pos.z = radius * sin(theta) * sin(phi);
			pos.y = radius * cos(theta);

			const DirectX::XMVECTOR vPos = DirectX::XMLoadFloat3(&pos);
			DirectX::XMFLOAT3 normal;
			DirectX::XMStoreFloat3(&normal, DirectX::XMVector3Normalize(vPos));

			vertices.emplace_back(pos, normal);
		}
	}
	vertices.emplace_back(DirectX::XMFLOAT3{ 0, -radius, 0 }, DirectX::XMFLOAT3{ 0,-1,0 });

	for (auto i = 1; i < steps + 1; ++i)
	{
		indices.emplace_back(i);

		auto v1 = i + 1;
		if (i % steps == 0)
			v1 -= steps;

		indices.emplace_back(v1);
		indices.emplace_back(0);
	}

	//MIDDLE
	for (auto i = 1; i < vertCount - 1 - steps; ++i)
	{
		const auto v0 = i;
		auto v1 = i + 1;

		if (i % steps == 0)
			v1 -= steps;

		const auto v2 = v1 + steps;
		const auto v3 = v0 + steps;

		indices.emplace_back(v0);
		indices.emplace_back(v1);
		indices.emplace_back(v2);
		indices.emplace_back(v2);
		indices.emplace_back(v3);
		indices.emplace_back(v0);
	}

	//BOTTOM
	for (auto i = vertCount - steps - 1; i < vertCount - 1; ++i)
	{
		indices.emplace_back(i);

		auto v1 = i + 1;
		if (i % steps == 0)
			v1 -= steps;

		indices.emplace_back(v1);
		indices.emplace_back(vertCount - 1);
	}

	return new Mesh(MyEngine::GetSingleton()->GetDevice(), MyEngine::GetSingleton()->GetWindowHandle(), vertices, indices, name);
}

static Mesh* CreateCube(float width, float height, float depth)
{
	auto resourceManager = ResourceManager::GetInstance();

	std::string name = "Cube(" + std::to_string(width) + ", " + std::to_string(height) + ", " + std::to_string(depth) + ")";
	if (resourceManager->GetMeshConst(name) != nullptr)
		return resourceManager->GetMeshConst(name);

	std::vector<Vertex> vertices;
	std::vector<uint32_t> indices;

	const float halfWidth = width / 2.f;
	const float halfHeight = height / 2.f;
	const float halfDepth = depth / 2.f;

	vertices.emplace_back(Vertex{ DirectX::XMFLOAT3{-halfWidth, halfHeight, -halfDepth}, DirectX::XMFLOAT3{0,0,-1} });
	vertices.emplace_back(Vertex{ DirectX::XMFLOAT3{halfWidth, halfHeight, -halfDepth}, DirectX::XMFLOAT3{0,0,-1} });
	vertices.emplace_back(Vertex{ DirectX::XMFLOAT3{halfWidth, -halfHeight, -halfDepth}, DirectX::XMFLOAT3{0,0,-1} });
	vertices.emplace_back(Vertex{ DirectX::XMFLOAT3{-halfWidth, -halfHeight, -halfDepth}, DirectX::XMFLOAT3{0,0,-1} });

	indices.emplace_back(static_cast<uint32_t>(vertices.size() - 4));
	indices.emplace_back(static_cast<uint32_t>(vertices.size() - 3));
	indices.emplace_back(static_cast<uint32_t>(vertices.size() - 2));

	indices.emplace_back(static_cast<uint32_t>(vertices.size() - 2));
	indices.emplace_back(static_cast<uint32_t>(vertices.size() - 1));
	indices.emplace_back(static_cast<uint32_t>(vertices.size() - 4));

	vertices.emplace_back(Vertex{ DirectX::XMFLOAT3{halfWidth, halfHeight, halfDepth}, DirectX::XMFLOAT3{0,0,-1} });
	vertices.emplace_back(Vertex{ DirectX::XMFLOAT3{-halfWidth, halfHeight, halfDepth}, DirectX::XMFLOAT3{0,0,-1} });
	vertices.emplace_back(Vertex{ DirectX::XMFLOAT3{-halfWidth, -halfHeight, halfDepth}, DirectX::XMFLOAT3{0,0,-1} });
	vertices.emplace_back(Vertex{ DirectX::XMFLOAT3{halfWidth, -halfHeight, halfDepth}, DirectX::XMFLOAT3{0,0,-1} });

	indices.emplace_back(static_cast<uint32_t>(vertices.size() - 4));
	indices.emplace_back(static_cast<uint32_t>(vertices.size() - 3));
	indices.emplace_back(static_cast<uint32_t>(vertices.size() - 2));

	indices.emplace_back(static_cast<uint32_t>(vertices.size() - 2));
	indices.emplace_back(static_cast<uint32_t>(vertices.size() - 1));
	indices.emplace_back(static_cast<uint32_t>(vertices.size() - 4));

	vertices.emplace_back(Vertex{ DirectX::XMFLOAT3{halfWidth, halfHeight, halfDepth}, DirectX::XMFLOAT3{0,0,1} });
	vertices.emplace_back(Vertex{ DirectX::XMFLOAT3{-halfWidth, halfHeight, halfDepth}, DirectX::XMFLOAT3{0,0,1} });
	vertices.emplace_back(Vertex{ DirectX::XMFLOAT3{-halfWidth, -halfHeight, halfDepth}, DirectX::XMFLOAT3{0,0,1} });
	vertices.emplace_back(Vertex{ DirectX::XMFLOAT3{halfWidth, -halfHeight, halfDepth}, DirectX::XMFLOAT3{0,0,1} });

	indices.emplace_back(static_cast<uint32_t>(vertices.size() - 4));
	indices.emplace_back(static_cast<uint32_t>(vertices.size() - 3));
	indices.emplace_back(static_cast<uint32_t>(vertices.size() - 2));

	indices.emplace_back(static_cast<uint32_t>(vertices.size() - 2));
	indices.emplace_back(static_cast<uint32_t>(vertices.size() - 1));
	indices.emplace_back(static_cast<uint32_t>(vertices.size() - 4));

	vertices.emplace_back(Vertex{ DirectX::XMFLOAT3{-halfWidth, halfHeight, halfDepth}, DirectX::XMFLOAT3{-1,0,0} });
	vertices.emplace_back(Vertex{ DirectX::XMFLOAT3{-halfWidth, halfHeight, -halfDepth}, DirectX::XMFLOAT3{-1,0,0} });
	vertices.emplace_back(Vertex{ DirectX::XMFLOAT3{-halfWidth, -halfHeight, -halfDepth}, DirectX::XMFLOAT3{-1,0,0} });
	vertices.emplace_back(Vertex{ DirectX::XMFLOAT3{-halfWidth, -halfHeight, halfDepth}, DirectX::XMFLOAT3{-1,0,0} });

	indices.emplace_back(static_cast<uint32_t>(vertices.size() - 4));
	indices.emplace_back(static_cast<uint32_t>(vertices.size() - 3));
	indices.emplace_back(static_cast<uint32_t>(vertices.size() - 2));

	indices.emplace_back(static_cast<uint32_t>(vertices.size() - 2));
	indices.emplace_back(static_cast<uint32_t>(vertices.size() - 1));
	indices.emplace_back(static_cast<uint32_t>(vertices.size() - 4));

	vertices.emplace_back(Vertex{ DirectX::XMFLOAT3{halfWidth, halfHeight, -halfDepth}, DirectX::XMFLOAT3{1,0,0} });
	vertices.emplace_back(Vertex{ DirectX::XMFLOAT3{halfWidth, halfHeight, halfDepth}, DirectX::XMFLOAT3{1,0,0} });
	vertices.emplace_back(Vertex{ DirectX::XMFLOAT3{halfWidth, -halfHeight, halfDepth}, DirectX::XMFLOAT3{1,0,0} });
	vertices.emplace_back(Vertex{ DirectX::XMFLOAT3{halfWidth, -halfHeight, -halfDepth}, DirectX::XMFLOAT3{1,0,0} });

	indices.emplace_back(static_cast<uint32_t>(vertices.size() - 4));
	indices.emplace_back(static_cast<uint32_t>(vertices.size() - 3));
	indices.emplace_back(static_cast<uint32_t>(vertices.size() - 2));

	indices.emplace_back(static_cast<uint32_t>(vertices.size() - 2));
	indices.emplace_back(static_cast<uint32_t>(vertices.size() - 1));
	indices.emplace_back(static_cast<uint32_t>(vertices.size() - 4));

	vertices.emplace_back(Vertex{ DirectX::XMFLOAT3{-halfWidth, halfHeight, halfDepth}, DirectX::XMFLOAT3{0,1,0} });
	vertices.emplace_back(Vertex{ DirectX::XMFLOAT3{halfWidth, halfHeight, halfDepth}, DirectX::XMFLOAT3{0,1,0} });
	vertices.emplace_back(Vertex{ DirectX::XMFLOAT3{halfWidth, halfHeight, -halfDepth}, DirectX::XMFLOAT3{0,1,0} });
	vertices.emplace_back(Vertex{ DirectX::XMFLOAT3{-halfWidth, halfHeight, -halfDepth}, DirectX::XMFLOAT3{0,1,0} });

	indices.emplace_back(static_cast<uint32_t>(vertices.size() - 4));
	indices.emplace_back(static_cast<uint32_t>(vertices.size() - 3));
	indices.emplace_back(static_cast<uint32_t>(vertices.size() - 2));

	indices.emplace_back(static_cast<uint32_t>(vertices.size() - 2));
	indices.emplace_back(static_cast<uint32_t>(vertices.size() - 1));
	indices.emplace_back(static_cast<uint32_t>(vertices.size() - 4));

	vertices.emplace_back(Vertex{ DirectX::XMFLOAT3{-halfWidth, -halfHeight, -halfDepth}, DirectX::XMFLOAT3{0,-1,0} });
	vertices.emplace_back(Vertex{ DirectX::XMFLOAT3{halfWidth, -halfHeight, -halfDepth}, DirectX::XMFLOAT3{0,-1,0} });
	vertices.emplace_back(Vertex{ DirectX::XMFLOAT3{halfWidth, -halfHeight, halfDepth}, DirectX::XMFLOAT3{0,-1,0} });
	vertices.emplace_back(Vertex{ DirectX::XMFLOAT3{-halfWidth, -halfHeight, halfDepth}, DirectX::XMFLOAT3{0,-1,0} });

	indices.emplace_back(static_cast<uint32_t>(vertices.size() - 4));
	indices.emplace_back(static_cast<uint32_t>(vertices.size() - 3));
	indices.emplace_back(static_cast<uint32_t>(vertices.size() - 2));

	indices.emplace_back(static_cast<uint32_t>(vertices.size() - 2));
	indices.emplace_back(static_cast<uint32_t>(vertices.size() - 1));
	indices.emplace_back(static_cast<uint32_t>(vertices.size() - 4));

	return new Mesh(MyEngine::GetSingleton()->GetDevice(), MyEngine::GetSingleton()->GetWindowHandle(), vertices, indices, name);
}

static void CreateMesh(std::vector<Mesh*>& pMeshes, Mesh_Struct& mesh, const std::string& filepath)
{
	if (mesh.vertices.empty() || mesh.indices.empty())
		return;

	for (uint32_t i = 0; i < mesh.indices.size(); i += 3)
	{
		uint32_t index0 = mesh.indices[i];
		uint32_t index1 = mesh.indices[i + 1];
		uint32_t index2 = mesh.indices[i + 2];

		const DirectX::XMVECTOR& p0 = DirectX::XMLoadFloat3(&mesh.vertices[index0].position);
		const DirectX::XMVECTOR& p1 = DirectX::XMLoadFloat3(&mesh.vertices[index1].position);
		const DirectX::XMVECTOR& p2 = DirectX::XMLoadFloat3(&mesh.vertices[index2].position);
		const DirectX::XMFLOAT2& uv0 = mesh.vertices[index0].uv;
		const DirectX::XMFLOAT2& uv1 = mesh.vertices[index1].uv;
		const DirectX::XMFLOAT2& uv2 = mesh.vertices[index2].uv;

		const DirectX::XMVECTOR edge0 = DirectX::XMVectorSubtract(p1, p0);
		const DirectX::XMVECTOR edge1 = DirectX::XMVectorSubtract(p2, p0);

		const auto diffx = DirectX::XMFLOAT2(uv1.x - uv0.x, uv2.x - uv0.x);
		const auto diffy = DirectX::XMFLOAT2(uv1.y - uv0.y, uv2.y - uv0.y);

		const DirectX::XMVECTOR diffX = DirectX::XMLoadFloat2(&diffx);
		const DirectX::XMVECTOR diffY = DirectX::XMLoadFloat2(&diffy);

		float one = 1.0f;
		auto oneVector = DirectX::XMLoadFloat(&one);
		auto r = DirectX::XMVectorDivide(oneVector, DirectX::XMVector3Cross(diffX, diffY));

		DirectX::XMFLOAT2 storedDiff;
		DirectX::XMStoreFloat2(&storedDiff, diffY);

		DirectX::XMVECTOR tangent0 = DirectX::XMLoadFloat3(&mesh.vertices[index0].tangent);
		DirectX::XMVECTOR tangent1 = DirectX::XMLoadFloat3(&mesh.vertices[index1].tangent);
		DirectX::XMVECTOR tangent2 = DirectX::XMLoadFloat3(&mesh.vertices[index2].tangent);

		DirectX::XMVECTOR tangent = DirectX::XMVectorMultiply(DirectX::XMVectorSubtract(DirectX::XMVectorMultiply(edge0, DirectX::XMLoadFloat(&storedDiff.y)), DirectX::XMVectorMultiply(edge1, DirectX::XMLoadFloat(&storedDiff.x))), r);

		DirectX::XMVectorAdd(tangent0, tangent);
		DirectX::XMVectorAdd(tangent1, tangent);
		DirectX::XMVectorAdd(tangent2, tangent);

		DirectX::XMStoreFloat3(&mesh.vertices[index0].tangent, tangent0);
		DirectX::XMStoreFloat3(&mesh.vertices[index1].tangent, tangent1);
		DirectX::XMStoreFloat3(&mesh.vertices[index2].tangent, tangent2);
	}
	for (auto& v : mesh.vertices)
	{
		auto tangent = DirectX::XMLoadFloat3(&v.tangent);
		auto normal = DirectX::XMLoadFloat3(&v.normal);

		auto project = DirectX::XMVectorMultiply(normal, DirectX::XMVectorDivide(DirectX::XMVector3Dot(normal, tangent), DirectX::XMVectorSqrt(normal)));

		tangent = DirectX::XMVector3Normalize(DirectX::XMVectorSubtract(tangent, project));

		DirectX::XMStoreFloat3(&v.tangent, tangent);
	}

	//auto mat = new Material(MyEngine::GetSingleton()->GetDevice(), L"Resources/material_unlit.fx");
	//
	//Texture* pDiffuseTexture = new Texture(MyEngine::GetSingleton()->GetDevice(), L"Resources/uv_grid_2.png");
	//mat->SetDiffuseMap(pDiffuseTexture);

	auto materialManager = MaterialManager::GetInstance();

	Mesh* pMesh = new Mesh(
		MyEngine::GetSingleton()->GetDevice(),
		MyEngine::GetSingleton()->GetWindowHandle(),
		mesh.vertices,
		mesh.indices,
		filepath,
		static_cast<int>(pMeshes.size()),
		materialManager->GetMaterial( (mesh.materialName.empty() ? "default" : mesh.materialName))
	);
	pMeshes.push_back(pMesh);
}

static bool ParseOBJ(const std::string& filename, std::vector<Mesh*>& m_pMeshes)
{
	std::ifstream file(filename);
	if (!file)
		return false;

	std::vector<DirectX::XMFLOAT3> positions;
	std::vector<DirectX::XMFLOAT3> normals;
	std::vector<DirectX::XMFLOAT2> UVs;

	std::vector<Vertex> vertices;
	std::vector<u_int> indices;
	std::vector<Mesh_Struct> meshes;
	std::vector<std::string> meshmatNames;

	std::string sCommand;
	while (!file.eof())
	{
		//read the first word of the string, use the >> operator (istream::operator>>) 
		file >> sCommand;
		//use conditional statements to process the different commands	
		if (sCommand == "#")
		{
			// Ignore Comment
		}
		else if (sCommand == "v")
		{
			//Vertex
			float x, y, z;
			file >> x >> y >> z;
			positions.push_back(DirectX::XMFLOAT3(x, y, -z));
		}
		else if (sCommand == "vt")
		{
			// Vertex TexCoord
			float u, v;
			file >> u >> v;
			UVs.push_back(DirectX::XMFLOAT2(u, 1 - v));
		}
		else if (sCommand == "vn")
		{
			// Vertex Normal
			float x, y, z;
			file >> x >> y >> z;
			normals.push_back(DirectX::XMFLOAT3(x, y, z));
		}
		else if (sCommand == "f")
		{
			//if a face is read:
			//construct the 3 vertices, add them to the vertex array
			//add three indices to the index array
			//add the material index as attibute to the attribute array
			//
			// Faces or triangles
			Vertex vertex{};
			size_t iPosition, iTexCoord, iNormal;
			for (size_t iFace = 0; iFace < 3; iFace++)
			{
				// OBJ format uses 1-based arrays
				file >> iPosition;
				vertex.position = positions[iPosition - 1];

				if ('/' == file.peek())//is next in buffer ==  '/' ?
				{
					file.ignore();//read and ignore one element ('/')

					if ('/' != file.peek())
					{
						// Optional texture coordinate
						file >> iTexCoord;
						vertex.uv = UVs[iTexCoord - 1];
					}

					if ('/' == file.peek())
					{
						file.ignore();

						// Optional vertex normal
						file >> iNormal;
						vertex.normal = normals[iNormal - 1];
					}
				}

				vertices.push_back(vertex);
				indices.push_back(uint32_t(vertices.size()) - 1);
			}
		}
		else if (sCommand == "o")
		{
			if (!vertices.empty() && !indices.empty())
				meshes.push_back(Mesh_Struct{ vertices, indices, meshmatNames[meshmatNames.size() - 1] });

			vertices.clear();
			indices.clear();
		}
		else if (sCommand == "usemtl")
		{
			std::string name;
			file >> name;

			meshmatNames.push_back(name);

			if (!indices.empty() && !vertices.empty())
			{
				//Create mesh
				meshes.push_back(Mesh_Struct{ vertices, indices });
			}

			auto mat = new Material(MyEngine::GetSingleton()->GetDevice(), "Resources/material_unlit.fx");

			Texture* pDiffuseTexture = new Texture(MyEngine::GetSingleton()->GetDevice(), "Resources/uv_grid_2.png");
			mat->SetDiffuseMap(pDiffuseTexture);

			MaterialManager::GetInstance()->AddMaterial(name, mat);
		}

		//read till end of line and ignore all remaining chars
		file.ignore(1000, '\n');
	}
	file.close();

	// Create the final mesh
	meshes.emplace_back(Mesh_Struct{ vertices, indices, meshmatNames[meshmatNames.size() - 1] });
	vertices.clear();
	indices.clear();
	for (auto& meshStruct : meshes)
	{
		CreateMesh(m_pMeshes, meshStruct, filename);
	}

	return true;
}

static bool ParseOBJ(const std::string& filename,  std::vector<Mesh*>& pMeshes, Scene* pScene)
{
	std::ifstream file(filename);
	if (!file)
		return false;

	std::vector<DirectX::XMFLOAT3> positions;
	std::vector<DirectX::XMFLOAT3> normals;
	std::vector<DirectX::XMFLOAT2> UVs;

	std::vector<Vertex> vertices;
	std::vector<u_int> indices;
	std::vector<Mesh_Struct> meshes;
	std::vector<std::string> meshmatNames;


	std::string sCommand;
	while (!file.eof())
	{
		//read the first word of the string, use the >> operator (istream::operator>>) 
		file >> sCommand;
		//use conditional statements to process the different commands	
		if (sCommand == "#")
		{
			// Ignore Comment
		}
		else if (sCommand == "v")
		{
			//Vertex
			float x, y, z;
			file >> x >> y >> z;
			positions.push_back(DirectX::XMFLOAT3(x, y, -z));
		}
		else if (sCommand == "vt")
		{
			// Vertex TexCoord
			float u, v;
			file >> u >> v;
			UVs.push_back(DirectX::XMFLOAT2(u, 1 - v));
		}
		else if (sCommand == "vn")
		{
			// Vertex Normal
			float x, y, z;
			file >> x >> y >> z;
			normals.push_back(DirectX::XMFLOAT3(x, y, z));
		}
		else if (sCommand == "f")
		{
			//if a face is read:
			//construct the 3 vertices, add them to the vertex array
			//add three indices to the index array
			//add the material index as attibute to the attribute array
			//
			// Faces or triangles
			Vertex vertex{};
			size_t iPosition, iTexCoord, iNormal;
			for (size_t iFace = 0; iFace < 3; iFace++)
			{
				// OBJ format uses 1-based arrays
				file >> iPosition;
				vertex.position = positions[iPosition - 1];

				if ('/' == file.peek())//is next in buffer ==  '/' ?
				{
					file.ignore();//read and ignore one element ('/')

					if ('/' != file.peek())
					{
						// Optional texture coordinate
						file >> iTexCoord;
						vertex.uv = UVs[iTexCoord - 1];
					}

					if ('/' == file.peek())
					{
						file.ignore();

						// Optional vertex normal
						file >> iNormal;
						vertex.normal = normals[iNormal - 1];
					}
				}

				vertices.push_back(vertex);
				indices.push_back(uint32_t(vertices.size()) - 1);
			}
		}
		else if (sCommand == "o")
		{
			if(!vertices.empty() && !indices.empty())
				meshes.push_back(Mesh_Struct{ vertices, indices, meshmatNames[meshmatNames.size() -1]});
			
			vertices.clear();
			indices.clear();
		}
		else if (sCommand == "usemtl")
		{
			std::string name;
			file >> name;

			meshmatNames.push_back(name);

			if (!indices.empty() && !vertices.empty())
			{
				//Create mesh
				meshes.push_back(Mesh_Struct{ vertices, indices });
			}

			auto mat = new Material(MyEngine::GetSingleton()->GetDevice(), "Resources/material_unlit.fx", meshmatNames[meshmatNames.size() - 1]);

			Texture* pDiffuseTexture = new Texture(MyEngine::GetSingleton()->GetDevice(), "Resources/uv_grid_2.png");
			mat->SetDiffuseMap(pDiffuseTexture);

			pScene->AddMaterial(name, mat);
		}

		//read till end of line and ignore all remaining chars
		file.ignore(1000, '\n');
	}
	file.close();

	// Create the final mesh
	meshes.push_back(Mesh_Struct{ vertices, indices, meshmatNames[meshmatNames.size()-1] });
	vertices.clear();
	indices.clear();
	for (auto& meshStruct : meshes)
		CreateMesh(pMeshes, meshStruct, pScene, filename);

	return true;
}

static bool ParseOBJ(const std::string& filename, std::vector<Vertex>& vertices, std::vector<uint32_t>& indices)
{
	std::ifstream file(filename);
	if (!file)
		return false;

	std::vector<DirectX::XMFLOAT3> positions;
	std::vector<DirectX::XMFLOAT3> normals;
	std::vector<DirectX::XMFLOAT2> UVs;

	vertices.clear();
	indices.clear();

	std::string sCommand;
	// start a while iteration ending when the end of file is reached (ios::eof)
	while (!file.eof())
	{
		//read the first word of the string, use the >> operator (istream::operator>>) 
		file >> sCommand;
		//use conditional statements to process the different commands	
		if (sCommand == "#")
		{
			// Ignore Comment
		}
		else if (sCommand == "v")
		{
			//Vertex
			float x, y, z;
			file >> x >> y >> z;
			positions.push_back(DirectX::XMFLOAT3(x, y, z));
		}
		else if (sCommand == "vt")
		{
			// Vertex TexCoord
			float u, v;
			file >> u >> v;
			UVs.push_back(DirectX::XMFLOAT2(u, 1 - v));
		}
		else if (sCommand == "vn")
		{
			// Vertex Normal
			float x, y, z;
			file >> x >> y >> z;
			normals.push_back(DirectX::XMFLOAT3(x, y, z));
		}
		else if (sCommand == "f")
		{
			//if a face is read:
			//construct the 3 vertices, add them to the vertex array
			//add three indices to the index array
			//add the material index as attibute to the attribute array
			//
			// Faces or triangles
			Vertex vertex{};
			size_t iPosition, iTexCoord, iNormal;
			for (size_t iFace = 0; iFace < 3; iFace++)
			{
				// OBJ format uses 1-based arrays
				file >> iPosition;
				vertex.position = positions[iPosition - 1];

				if ('/' == file.peek())//is next in buffer ==  '/' ?
				{
					file.ignore();//read and ignore one element ('/')

					if ('/' != file.peek())
					{
						// Optional texture coordinate
						file >> iTexCoord;
						vertex.uv = UVs[iTexCoord - 1];
					}

					if ('/' == file.peek())
					{
						file.ignore();

						// Optional vertex normal
						file >> iNormal;
						vertex.normal = normals[iNormal - 1];
					}
				}

				vertices.push_back(vertex);
				indices.push_back(uint32_t(vertices.size()) - 1);
			}
		}
		//read till end of line and ignore all remaining chars
		file.ignore(1000, '\n');
	}

	for (uint32_t i = 0; i < indices.size(); i += 3)
	{
		uint32_t index0 = indices[i];
		uint32_t index1 = indices[i + 1];
		uint32_t index2 = indices[i + 2];

		const DirectX::XMVECTOR& p0 = DirectX::XMLoadFloat3(&vertices[index0].position);
		const DirectX::XMVECTOR& p1 = DirectX::XMLoadFloat3(&vertices[index1].position);
		const DirectX::XMVECTOR& p2 = DirectX::XMLoadFloat3(&vertices[index2].position);
		const DirectX::XMFLOAT2& uv0 = vertices[index0].uv;
		const DirectX::XMFLOAT2& uv1 = vertices[index1].uv;
		const DirectX::XMFLOAT2& uv2 = vertices[index2].uv;

		const DirectX::XMVECTOR edge0 = DirectX::XMVectorSubtract(p1, p0);
		const DirectX::XMVECTOR edge1 = DirectX::XMVectorSubtract(p2, p0);

		const auto diffx = DirectX::XMFLOAT2(uv1.x - uv0.x, uv2.x - uv0.x);
		const auto diffy = DirectX::XMFLOAT2(uv1.y - uv0.y, uv2.y - uv0.y);

		const DirectX::XMVECTOR diffX = DirectX::XMLoadFloat2(&diffx);
		const DirectX::XMVECTOR diffY = DirectX::XMLoadFloat2(&diffy);

		float one = 1.0f;
		auto oneVector = DirectX::XMLoadFloat(&one);
		auto r = DirectX::XMVectorDivide(oneVector, DirectX::XMVector3Cross(diffX, diffY));

		DirectX::XMFLOAT2 storedDiff;
		DirectX::XMStoreFloat2(&storedDiff, diffY);

		DirectX::XMVECTOR tangent0 = DirectX::XMLoadFloat3(&vertices[index0].tangent);
		DirectX::XMVECTOR tangent1 = DirectX::XMLoadFloat3(&vertices[index1].tangent);
		DirectX::XMVECTOR tangent2 = DirectX::XMLoadFloat3(&vertices[index2].tangent);

		DirectX::XMVECTOR tangent = DirectX::XMVectorMultiply(DirectX::XMVectorSubtract(DirectX::XMVectorMultiply(edge0, DirectX::XMLoadFloat(&storedDiff.y)), DirectX::XMVectorMultiply(edge1, DirectX::XMLoadFloat(&storedDiff.x))), r);

		DirectX::XMVectorAdd(tangent0, tangent);
		DirectX::XMVectorAdd(tangent1, tangent);
		DirectX::XMVectorAdd(tangent2, tangent);

		DirectX::XMStoreFloat3(&vertices[index0].tangent, tangent0);
		DirectX::XMStoreFloat3(&vertices[index1].tangent, tangent1);
		DirectX::XMStoreFloat3(&vertices[index2].tangent, tangent2);
	}
	for (auto& v : vertices)
	{
		auto tangent = DirectX::XMLoadFloat3(&v.tangent);
		auto normal = DirectX::XMLoadFloat3(&v.normal);

		auto project = DirectX::XMVectorMultiply(normal, DirectX::XMVectorDivide(DirectX::XMVector3Dot(normal, tangent), DirectX::XMVectorSqrt(normal)));

		tangent = DirectX::XMVector3Normalize(DirectX::XMVectorSubtract(tangent, project));

		DirectX::XMStoreFloat3(&v.tangent, tangent);
	}

	return true;
}