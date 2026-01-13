#ifndef MODEL_LOADER_H
#define MODEL_LOADER_H

#include <vector>
#include <map>
#include <d3d11_1.h>
#include <DirectXMath.h>

#include <DirectXTex.h>

#include <assimp\Importer.hpp>
#include <assimp\scene.h>
#include <assimp\postprocess.h>

#include <assimp\material.h>
#include <assimp\types.h>

#include "Mesh.h"
#include "TextureLoader.h"
#include "SkeletonInfo.h"
#include "Animation.h"



using namespace DirectX;

struct MaterialInfo {
	// PBR scalar factors (always present; default 1 for textures)
	float metallicFactor = 0.0f;
	float roughnessFactor = 1.0f;
	//bool hasMetallicTex = false;	//없으면 default쓰도록 해서 없어도됨
	//bool hasRoughnessTex = false;
	//std::shared_ptr<Texture> metallicTex;   // or ORM packed texture
	//std::shared_ptr<Texture> roughnessTex;
};






class ModelLoader
{
public:
	ModelLoader();
	~ModelLoader();

	Vector4 m_Ambient{ 1.0f, 1.0f, 1.0f, 1.0f }; // 환경광 반사 계수
	Vector4 m_Diffuse{ 1.0f, 1.0f, 1.0f, 1.0f }; // 난반사 계수
	Vector4 m_Specular{ 1.0f, 1.0f, 1.0f, 1.0f }; // 정반사 계수


	bool Load(HWND hwnd, ID3D11Device* dev, ID3D11DeviceContext* devcon, std::string filename, int _weight);
	void DrawSkeletal(ComPtr<ID3D11DeviceContext>& devcon, ComPtr<ID3D11Buffer>& materialB, ComPtr<ID3D11BlendState>& blendOn, ComPtr<ID3D11BlendState>& blendOff);
	void DrawStatic(ComPtr<ID3D11DeviceContext>& devcon, ComPtr<ID3D11Buffer>& materialB, ComPtr<ID3D11BlendState>& blendOn, ComPtr<ID3D11BlendState>& blendOff);
	void Close();

	int Getweight()const;

	void LoadSkeleton(const aiScene* scene, SkeletonInfo& skelinfo);

	SkeletonInfo* GetSkeletonInfo();
	const Animation* GetAnimation(const string& name) const;
	map<string, Animation*>& Getanimelist();

	const vector<string>* GetAnimeName();

	//애니메이션 유무, 본 유무에 따라 분기를 나눠야함
	bool HasSkeleton() const;
	bool HasAnimations() const;


	void LoadNodeHierarchy(
		const aiNode* node,
		int parentBoneIndex,
		vector<BoneInfo>& outBones,
		map<string, int>& outBoneMappingTable);

	void SetMergeValue(bool value) { m_bNeedsMerge = value; };

	void SetPSValue(bool value) { m_UsePSShader = value; }


	//PBR
	const MaterialInfo& GetMaterial() const { return m_material; }



private:
	ComPtr<ID3D11Device> dev_{};
	ComPtr<ID3D11DeviceContext> devcon_{};
	std::vector<Mesh<VERTEX>> staticMeshes;
	std::vector<Mesh<BONEVERTEX>> skeletalMeshes;
	bool isSkeletal = true;
	std::string directory_;
	std::vector<Texture> textures_loaded_;
	HWND hwnd_;
	//가중치
	int weight;

	//뼈모델 포인터
	SkeletonInfo skel;
	map<string, Animation*> animelist{};
	//애니메이션 리스트 이름
	vector<string> animeName{};

	//rigid용 변수
	bool dummyBone = false;

	XMFLOAT4X4 ConvertAiMatrixToDXMatrix(const aiMatrix4x4& aiM);
	
	void SetBoneHierarchy(aiNode* currentNode, int parentBoneIndex, SkeletonInfo& skel);

	void processNode(aiNode* node, const aiScene* scene, const aiMatrix4x4& parentTransfrom);
	
	Mesh<VERTEX> processMesh(aiMesh* mesh, const aiScene* scene, const aiMatrix4x4& parentTransform);
	Mesh<BONEVERTEX> processMesh(aiMesh* mesh, const aiScene* scene, int meshIndex);
	std::vector<Texture> loadMaterialTextures(aiMaterial* mat, aiTextureType type, std::string typeName, const aiScene* scene);

	Texture loadDefaultTexture(const std::string& filename, const std::string& typeName);

	bool loadEmbeddedTexture(const aiTexture* embeddedTexture, ComPtr<ID3D11ShaderResourceView>& o_texture);

	HRESULT loadTextureFromFile(const std::wstring& filename, ComPtr<ID3D11ShaderResourceView>& o_texture);

	
	map<int, int> rigidMeshToBoneMap{};

	bool m_bNeedsMerge = false;
	bool m_UsePSShader = false;


	//PBR
	MaterialInfo m_material;	

	bool hasAlbedoTex = false;

};

#endif // !MODEL_LOADER_H