#include "ModelLoader.h"
#include "../Common/Helper.h"

ModelLoader::ModelLoader() :
	dev_(nullptr),
	devcon_(nullptr),
	meshes_(),
	directory_(),
	textures_loaded_(),
	hwnd_(nullptr) {
	// empty
}


ModelLoader::~ModelLoader() {
	// empty
	dev_ = nullptr;
	devcon_ = nullptr;
}

bool ModelLoader::Load(HWND hwnd, ID3D11Device* dev, ID3D11DeviceContext* devcon, std::string filename, int _weight) {
	Assimp::Importer importer;	// 기본 임포트 옵션

	const aiScene* pScene = importer.ReadFile(filename,
		aiProcess_Triangulate |					// vertex 삼각형 으로 출력
		aiProcess_GenNormals |        // Normal 정보 생성  
		aiProcess_GenUVCoords |      // 텍스처 좌표 생성
		aiProcess_CalcTangentSpace |  // 탄젠트 벡터 생성
		aiProcess_ConvertToLeftHanded);			// DX용 왼손좌표계 변환

	if (pScene == nullptr)
		return false;

	this->directory_ = filename.substr(0, filename.find_last_of("/\\"));

	this->dev_ = dev;
	this->devcon_ = devcon;
	this->hwnd_ = hwnd;
	this->weight = _weight;

	processNode(pScene->mRootNode, pScene);

	return true;
}



void ModelLoader::Draw(ComPtr<ID3D11DeviceContext>& devcon, ComPtr<ID3D11Buffer>& materialB, ComPtr<ID3D11BlendState>& blendOn, ComPtr<ID3D11BlendState>& blendOff)
{	
	int size = meshes_.size();
	for (size_t i = 0; i < meshes_.size(); ++i) {
		
		Material meshMaterial = meshes_[i].GetMaterial();
		meshMaterial.ambient = m_Ambient;
		meshMaterial.diffuse = m_Diffuse;
		meshMaterial.specular = m_Specular;

		UINT sampleMask = 0xffffffff;
		//여기에 mesh의 enum클레스 확인후 넘겨줄값 설정 //알파블랜딩 사용여부 및 clip 사용유무까지
		if (meshes_[i].TransMode == Transparency::Opaque)
		{
			devcon->OMSetBlendState(blendOff.Get(), nullptr, 0xFFFFFFFF);
			meshMaterial.UseClip = 0;
		}
		else if (meshes_[i].TransMode == Transparency::Cutout)
		{
			devcon->OMSetBlendState(blendOff.Get(), nullptr, 0xFFFFFFFF);
			meshMaterial.UseClip = 1;
		}
		else if (meshes_[i].TransMode == Transparency::AlphaBlend)
		{
			devcon->OMSetBlendState(blendOn.Get(), nullptr, sampleMask);
			meshMaterial.UseClip = 0;
		}


		devcon->UpdateSubresource(materialB.Get(), 0, nullptr, &meshMaterial, 0, 0);
		devcon->PSSetConstantBuffers(1, 1, materialB.GetAddressOf());



		meshes_[i].Draw(devcon);
	}
}


Mesh ModelLoader::processMesh(aiMesh* mesh, const aiScene* scene) {
	// Data to fill
	std::vector<VERTEX> vertices;		//버텍스 정보 채우기
	std::vector<UINT> indices;			//최종 인덱스 버퍼 배열
	std::vector<Texture> textures;

	// Walk through each of the mesh's vertices
	for (UINT i = 0; i < mesh->mNumVertices; i++) {
		VERTEX vertex;

		vertex.position = { mesh->mVertices[i].x ,mesh->mVertices[i].y, mesh->mVertices[i].z };
		vertex.normal = { mesh->mNormals[i].x, mesh->mNormals[i].y, mesh->mNormals[i].z };

		if (mesh->mTextureCoords[0]) {
			vertex.tex.x = (float)mesh->mTextureCoords[0][i].x;
			vertex.tex.y = (float)mesh->mTextureCoords[0][i].y;
		}

		if (mesh->mTangents)
		{
			vertex.tangent = { mesh->mTangents[i].x ,mesh->mTangents[i].y, mesh->mTangents[i].z };
		}
		if (mesh->mBitangents)
		{
			vertex.bitangent = { mesh->mBitangents[i].x, mesh->mBitangents[i].y, mesh->mBitangents[i].z };
		}

		vertices.push_back(vertex);
	}
	
	//인덱스 배열			//mNumFaces -> 총 면의 갯수
	//인덱스채우기
	for (UINT i = 0; i < mesh->mNumFaces; i++)
	{
		aiFace face = mesh->mFaces[i];
		for (UINT j = 0; j < face.mNumIndices; j++)
		{
			indices.push_back(face.mIndices[j]);
		}
	}

	// 아래는 Tan, Bitan 구하는공식 ( Tan, Bitan 구하는 공식만 외워두기, 2x2 행렬 생성-> 역행렬 변경(제한식필수) )
	// assimp이용해서 구하면됨
	//for (UINT i = 0; i < mesh->mNumFaces; i++) {
	//	aiFace face = mesh->mFaces[i];

	//	//mNumIndices값에 따라 aiFace가 나타내는 다각형의 종류를 알수있다
	//	if (face.mNumIndices != 3) continue;	//FBX파일 로드시 aiProcess_Triangulate를 해줘야함
	//	

	//	std::vector<int> currIndices;	// 현재 삼각형의 정보를 임시저장
	//	std::vector<Vector3> pos;
	//	std::vector<Vector2> uv;

	//	for (UINT j = 0; j < face.mNumIndices; j++)	//삼각형 제한을 둬서 3번 실행 //인덱스 0~2
	//	{
	//		//mIndices -> 정점의 인덱스
	//		int currIndex = face.mIndices[j];
	//		indices.push_back(currIndex);
	//		currIndices.push_back(currIndex);
	//		pos.push_back(vertices[currIndex].position);
	//		uv.push_back(vertices[currIndex].tex);
	//	}

	//	//TBN 계산 시작
	//	Vector3 edge1 = pos[1] - pos[0];
	//	Vector3 edge2 = pos[2] - pos[0];
	//	Vector2 deltaUV1 = uv[1] - uv[0];
	//	Vector2 deltaUV2 = uv[2] - uv[0];

	//	//uv 변위 행렬식
	//	float det = deltaUV1.x * deltaUV2.y - deltaUV2.x * deltaUV1.y;
	//	if (fabs(det) < 1e-6f) continue;
	//	//역행렬의 상수
	//	float f = 1.0f / det;

	//	Vector3 tangent = f * (deltaUV2.y * edge1 - deltaUV1.y * edge2);
	//	Vector3 bitangent = f * (-deltaUV2.x * edge1 + deltaUV1.x * edge2);

	//	for (int k = 0; k < currIndices.size(); k++)
	//	{
	//		int currIndex = currIndices[k];
	//		vertices[currIndex].tangent += tangent;
	//		vertices[currIndex].bitangent += bitangent;
	//	}
	//}
	//
	if (mesh->mMaterialIndex >= 0) {
		aiMaterial* material = scene->mMaterials[mesh->mMaterialIndex];

		//데이터를 읽어왔을때 존재하지 않을 경우 기본 옵션 사용할수 있도록
		std::vector<Texture> diffuseMaps = this->loadMaterialTextures(material, aiTextureType_DIFFUSE, "DIFFUSE", scene);
		if (!diffuseMaps.empty()) {
			textures.insert(textures.end(), diffuseMaps.begin(), diffuseMaps.end());
		}

		std::vector<Texture> emissiveMaps = this->loadMaterialTextures(material, aiTextureType_EMISSIVE, "EMISSIVE", scene);
		if (!emissiveMaps.empty()) {
			textures.insert(textures.end(), emissiveMaps.begin(), emissiveMaps.end());
		}
		else
		{
			Texture defaultemissive = this->loadDefaultTexture("defaultEmissive.png", "EMISSIVE");
			textures.push_back(defaultemissive);
		}


		std::vector<Texture> normalsMaps = this->loadMaterialTextures(material, aiTextureType_NORMALS, "NORMAL", scene);
		if (!normalsMaps.empty()) {
			textures.insert(textures.end(), normalsMaps.begin(), normalsMaps.end());
		}
		else
		{
			Texture defaultNormal = this->loadDefaultTexture("defaultNormal.png", "NORMAL");
			textures.push_back(defaultNormal);
		}

		std::vector<Texture> specularMaps = this->loadMaterialTextures(material, aiTextureType_SPECULAR, "SPECULAR", scene);
		if (!specularMaps.empty()) {
			textures.insert(textures.end(), specularMaps.begin(), specularMaps.end());
		}
	}

	//mesh가 알파블랜딩/ clip/ 둘다 안쓰는지 체크하기 위한 부분
	aiMaterial* material = scene->mMaterials[mesh->mMaterialIndex];					//정보를 불러옴
	//변수 2개
	aiString opacityPath; Transparency T_Mod = Transparency::Opaque;
	//불투명도 마스크 슬롯이 연결되었는지 확인하는 bool 변수
	bool hasOpacityMap = (material->GetTexture(aiTextureType_OPACITY, 0, &opacityPath) == AI_SUCCESS);


	//초기값을 1.0으로 주는 이유는 Get으로 opacity값을 가져왔을때 없다면 불투명 기본값인 1.0f을 사용하기 위함
	float globalOpacity = 1.0f;
	//material에서 투명도를 가져오는 식 (1.0 이면 완전 불투명 0.0~1.0 미만이면 투명도가 존재)
	material->Get("$mat.opacity", // "$mat.opacity"
		aiTextureType_NONE,           // 타입 (aiPTI_Float와 유사)
		0,                            // 인덱스
		globalOpacity);

	if (hasOpacityMap)	//잘라낼 영역이 존재하기 때문에 clip함수 사용하도록
	{
		T_Mod = Transparency::Cutout;
	}
	else if (globalOpacity < 1.0f - 0.001f)	//1미만인 경우 완전 불투명이 아니기 때문에 알파블랜드 옵션사용
	{
		T_Mod = Transparency::AlphaBlend;
	}
	else
	{
		T_Mod = Transparency::Opaque;
	}


	return Mesh(dev_, vertices, indices, textures, T_Mod);
}

std::vector<Texture> ModelLoader::loadMaterialTextures(aiMaterial* mat, aiTextureType type, std::string typeName, const aiScene* scene) {
	std::vector<Texture> textures;
	for (UINT i = 0; i < mat->GetTextureCount(type); i++) {
		aiString str;
		mat->GetTexture(type, i, &str);
		// Check if texture was loaded before and if so, continue to next iteration: skip loading a new texture
		bool skip = false;
		for (UINT j = 0; j < textures_loaded_.size(); j++) {
			if (std::strcmp(textures_loaded_[j].path.c_str(), str.C_Str()) == 0) {
				textures.push_back(textures_loaded_[j]);
				skip = true; // A texture with the same filepath has already been loaded, continue to next one. (optimization)
				break;
			}
		}
		if (!skip) {   // If texture hasn't been loaded already, load it
			HRESULT hr;
			Texture texture;

			const aiTexture* embeddedTexture = scene->GetEmbeddedTexture(str.C_Str());
			if (embeddedTexture != nullptr) {
				loadEmbeddedTexture(embeddedTexture , texture.texture);
			}
			else {
				std::string filename = std::string(str.C_Str());
				filename = directory_ + '\\' + filename;
				std::wstring filenamews = std::wstring(filename.begin(), filename.end());
				hr = CreateWICTextureFromFile(dev_.Get(), devcon_.Get(), filenamews.c_str(), nullptr, &texture.texture);
				if (FAILED(hr))
					MessageBox(hwnd_, L"Texture couldn't be loaded", L"Error!", MB_ICONERROR | MB_OK);
			}
			texture.type = typeName;
			texture.path = str.C_Str();
			textures.push_back(texture);
			this->textures_loaded_.push_back(texture);  // Store it as texture loaded for entire model, to ensure we won't unnecesery load duplicate textures.
		}
	}
	return textures;
}

void ModelLoader::Close() {
	for (auto& t : textures_loaded_)
		t.Release();

	for (size_t i = 0; i < meshes_.size(); i++) {
		meshes_[i].Close();
	}
}

int ModelLoader::Getweight() const
{
	return weight;
}

void ModelLoader::processNode(aiNode* node, const aiScene* scene) {
	for (UINT i = 0; i < node->mNumMeshes; i++) {
		aiMesh* mesh = scene->mMeshes[node->mMeshes[i]];
		meshes_.push_back(this->processMesh(mesh, scene));
	}

	for (UINT i = 0; i < node->mNumChildren; i++) {
		this->processNode(node->mChildren[i], scene);
	}
}

ID3D11ShaderResourceView* ModelLoader::loadEmbeddedTexture(const aiTexture* embeddedTexture , ComPtr<ID3D11ShaderResourceView>& o_texture) {
	HRESULT hr;
	ID3D11ShaderResourceView* texture = nullptr;

	if (embeddedTexture->mHeight != 0) {
		// Load an uncompressed ARGB8888 embedded texture
		D3D11_TEXTURE2D_DESC desc;
		desc.Width = embeddedTexture->mWidth;
		desc.Height = embeddedTexture->mHeight;
		desc.MipLevels = 1;
		desc.ArraySize = 1;
		desc.SampleDesc.Count = 1;
		desc.SampleDesc.Quality = 0;
		desc.Usage = D3D11_USAGE_DEFAULT;
		desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
		desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
		desc.CPUAccessFlags = 0;
		desc.MiscFlags = 0;

		D3D11_SUBRESOURCE_DATA subresourceData;
		subresourceData.pSysMem = embeddedTexture->pcData;
		subresourceData.SysMemPitch = embeddedTexture->mWidth * 4;
		subresourceData.SysMemSlicePitch = embeddedTexture->mWidth * embeddedTexture->mHeight * 4;

		ID3D11Texture2D* texture2D = nullptr;

		HR_T(dev_->CreateTexture2D(&desc, &subresourceData, &texture2D));

		HR_T(dev_->CreateShaderResourceView(texture2D, nullptr, o_texture.GetAddressOf()));

		return texture;
	}

	// mHeight is 0, so try to load a compressed texture of mWidth bytes
	const size_t size = embeddedTexture->mWidth;

	hr = CreateWICTextureFromMemory(dev_.Get(), devcon_.Get(), reinterpret_cast<const unsigned char*>(embeddedTexture->pcData), size, nullptr, o_texture.GetAddressOf());
	if (FAILED(hr))
		MessageBox(hwnd_, L"Texture couldn't be created from memory!", L"Error!", MB_ICONERROR | MB_OK);

	return texture;
}

Texture ModelLoader::loadDefaultTexture(const std::string& filename, const std::string& typeName) {
	// 1. 이미 로드되었는지 확인 (textures_loaded_ 목록 활용)
	std::wstring filenamews = std::wstring(filename.begin(), filename.end());
	for (const auto& loadedTex : textures_loaded_) {
		// 경로 비교 (path는 aiString str.C_Str()로 저장되므로, 여기서는 wstring 대신 string 경로를 사용해야 함)
		if (loadedTex.path == filename) {
			return loadedTex;
		}
	}

	// 2. 로드되지 않았다면 새로 생성
	HRESULT hr;
	Texture texture;

	// CreateWICTextureFromFile을 사용하여 기본 텍스처 파일을 로드
	// directory_를 기본 경로로 사용
	std::string fullPath = directory_ + '\\' + filename;
	std::wstring fullPathWS = std::wstring(fullPath.begin(), fullPath.end());

	hr = CreateWICTextureFromFile(dev_.Get(), devcon_.Get(), fullPathWS.c_str(), nullptr, &texture.texture);

	if (FAILED(hr)) {
		// 기본 텍스처 파일이 없을 때의 최종 에러 처리
		MessageBox(hwnd_, L"Default Texture couldn't be loaded", L"Error!", MB_ICONERROR | MB_OK);
	}

	texture.type = typeName;
	texture.path = filename; // 경로를 파일명으로 저장
	this->textures_loaded_.push_back(texture);
	return texture;
}