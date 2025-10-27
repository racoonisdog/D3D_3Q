#ifndef MODEL_LOADER_H
#define MODEL_LOADER_H

#include <vector>
#include <d3d11_1.h>
#include <DirectXMath.h>

#include <assimp\Importer.hpp>
#include <assimp\scene.h>
#include <assimp\postprocess.h>

#include "Mesh.h"
#include "TextureLoader.h"

using namespace DirectX;

class ModelLoader
{
public:
	ModelLoader();
	~ModelLoader();

	Vector4 m_Ambient{ 1.0f, 1.0f, 1.0f, 1.0f }; // 환경광 반사 계수
	Vector4 m_Diffuse{ 1.0f, 1.0f, 1.0f, 1.0f }; // 난반사 계수
	Vector4 m_Specular{ 1.0f, 1.0f, 1.0f, 1.0f }; // 정반사 계수


	bool Load(HWND hwnd, ID3D11Device* dev, ID3D11DeviceContext* devcon, std::string filename);
	void Draw(ComPtr<ID3D11DeviceContext>& devcon, ComPtr<ID3D11Buffer>& materialB);


	void Close();
private:
	ComPtr<ID3D11Device> dev_{};
	ComPtr<ID3D11DeviceContext> devcon_{};
	std::vector<Mesh> meshes_;
	std::string directory_;
	std::vector<Texture> textures_loaded_;
	HWND hwnd_;



	void processNode(aiNode* node, const aiScene* scene);
	Mesh processMesh(aiMesh* mesh, const aiScene* scene);
	std::vector<Texture> loadMaterialTextures(aiMaterial* mat, aiTextureType type, std::string typeName, const aiScene* scene);

	Texture loadDefaultTexture(const std::string& filename, const std::string& typeName);

	ID3D11ShaderResourceView* loadEmbeddedTexture(const aiTexture* embeddedTexture, ComPtr<ID3D11ShaderResourceView>& o_texture);
};

#endif // !MODEL_LOADER_H