#include "ModelLoader.h"
#include "../Common/Helper.h"

ModelLoader::ModelLoader() :
	dev_(nullptr),
	devcon_(nullptr),
	staticMeshes(),
	skeletalMeshes(),
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

	// _$AssimpFbx$_Translation, _$AssimpFbx$_PreRotation, _$AssimpFbx$_Rotation 등의 노드 분기 생성 방지
	importer.SetPropertyBool(AI_CONFIG_IMPORT_FBX_PRESERVE_PIVOTS, false);

	const aiScene* pScene = importer.ReadFile(filename,
		aiProcess_Triangulate |					// vertex 삼각형 으로 출력
		aiProcess_GenNormals |        // Normal 정보 생성  
		aiProcess_GenUVCoords |      // 텍스처 좌표 생성
		aiProcess_CalcTangentSpace |  // 탄젠트 벡터 생성
		aiProcess_LimitBoneWeights |  // 본의 영향을 받는 정점의 최대 갯수를 4개로 제한
		aiProcess_ConvertToLeftHanded);			// DX용 왼손좌표계 변환

	if (pScene == nullptr)
		return false;

	this->directory_ = filename.substr(0, filename.find_last_of("/\\"));

	this->dev_ = dev;
	this->devcon_ = devcon;
	this->hwnd_ = hwnd;
	this->weight = _weight;

	LoadSkeleton(pScene, skel);

	if (skel.Bones.empty())
	{
		// A. 뼈는 없지만 애니메이션이 존재하는 경우 (BoxMan과 같은 Rigid 애니메이션)
		if (pScene->HasAnimations())
		{
			// 뼈가 없는 애니메이션 모델 플래그 설정 (선택 사항)
			isSkeletal = true;
			dummyBone = true;

			LoadNodeHierarchy(pScene->mRootNode, -1, skel.Bones, skel.m_BoneMappingTable);

			for (unsigned int i = 0; i < pScene->mNumMeshes; i++) {
				aiMesh* mesh = pScene->mMeshes[i];
				std::string meshName = mesh->mName.C_Str();

				// 기존에 로드된 뼈 이름-인덱스 테이블을 사용
				auto it = skel.m_BoneMappingTable.find(meshName);

				if (it != skel.m_BoneMappingTable.end()) {
					int boneIndex = it->second;
					// 메시 인덱스(i)와 해당 메시를 제어할 뼈 인덱스를 매핑
					rigidMeshToBoneMap[i] = boneIndex;
				}
				else {
					// 메시 이름과 일치하는 뼈가 없는 경우 (예: Scene Root)
					// 이 메시는 0번 뼈 (RootNode)를 사용하도록 폴백 (Fallback) 처리
					rigidMeshToBoneMap[i] = 0;
				}
			}

		}
		else
		{
			isSkeletal = false; // 정적 오브젝트로 처리
		}
	}
	else
	{
		//뼈가 존재함(애니메이션도 존재)
		isSkeletal = true;
		dummyBone = false;
	}


	aiMatrix4x4 identity;
	identity = aiMatrix4x4();

	processNode(pScene->mRootNode, pScene, identity);

	if (pScene->HasAnimations())
	{
		for (UINT i = 0; i < pScene->mNumAnimations; i++)
		{	
			//맨 처음 애니메이션 로드
			const aiAnimation* tempAnime = pScene->mAnimations[i];
			//애니메이션 이름
			string AnimeName = tempAnime->mName.C_Str();
			animeName.push_back(AnimeName);

			//동적할당
			Animation* rawAnim = new Animation();
			rawAnim->CreateFromAi(tempAnime);
			animelist[AnimeName] = rawAnim;
		}
	}

	if (m_bNeedsMerge && animelist.size() > 1)
	{
		// 1. Master Animation 생성
		Animation* masterAnim = new Animation();
		masterAnim->GetName() = string("MasterAnimation");

		double maxDuration = 0.0;

		// 2. 모든 조각난 클립을 MasterAnim에 병합
		for (auto const& pair : animelist)
		{
			const Animation* rawAnim = pair.second;
			masterAnim->MergeChannels(rawAnim); // Animation::MergeChannels 사용

			// 2. 재생 시간 업데이트 (가장 긴 클립의 정보를 사용)
			if (rawAnim->GetDurationTicks() > maxDuration)
			{
				maxDuration = rawAnim->GetDurationTicks();

				// masterAnim->CreateFromAi(rawAnim); 
				masterAnim->SetTicksPerSecond(rawAnim->GetTicksPerSecond());
			}
			delete rawAnim; // 기존 클립 메모리 해제
		}
		masterAnim->SetDurationTicks(maxDuration);

		// 3. 리스트를 비우고 MasterAnimation만 남김
		animelist.clear();
		animelist["MasterAnimation"] = masterAnim;


	}

	if (pScene->HasAnimations())
	{
		Animation* masterAnim = animelist.begin()->second; // MasterAnimation 포인터 획득
		// 뼈대가 총 70개라면 70에 가까운 숫자가 나와야 합니다.
		size_t a = masterAnim->m_Channels.size();
	}


	int b = 2;

	return true;
}

void ModelLoader::DrawSkeletal(ComPtr<ID3D11DeviceContext>& devcon, ComPtr<ID3D11Buffer>& materialB, ComPtr<ID3D11BlendState>& blendOn, ComPtr<ID3D11BlendState>& blendOff)
{
	int size = skeletalMeshes.size();


	for (size_t i = 0; i < skeletalMeshes.size(); ++i) {

		if (m_UsePSShader)
		{
			Material meshMaterial = skeletalMeshes[i].GetMaterial();
			skeletalMeshes[i].SetShadowV(m_UsePSShader);

			UINT sampleMask = 0xffffffff;
			//여기에 mesh의 enum클레스 확인후 넘겨줄값 설정 //알파블랜딩 사용여부 및 clip 사용유무까지
			if (skeletalMeshes[i].TransMode == Transparency::Opaque)
			{
				devcon->OMSetBlendState(blendOff.Get(), nullptr, 0xFFFFFFFF);
				meshMaterial.UseClip = 0;
			}
			else if (skeletalMeshes[i].TransMode == Transparency::Cutout)
			{
				devcon->OMSetBlendState(blendOff.Get(), nullptr, 0xFFFFFFFF);
				meshMaterial.UseClip = 1;
			}
			else if (skeletalMeshes[i].TransMode == Transparency::AlphaBlend)
			{
				devcon->OMSetBlendState(blendOn.Get(), nullptr, sampleMask);
				meshMaterial.UseClip = 0;
			}


			devcon->UpdateSubresource(materialB.Get(), 0, nullptr, &meshMaterial, 0, 0);
			devcon->PSSetConstantBuffers(1, 1, materialB.GetAddressOf());
		}
		else
		{
			skeletalMeshes[i].SetShadowV(m_UsePSShader);
		}
		skeletalMeshes[i].Draw(devcon);
	}
	if (!m_UsePSShader)
	{
		devcon->OMSetBlendState(nullptr, nullptr, 0xFFFFFFFF);
	}
}

void ModelLoader::DrawStatic(ComPtr<ID3D11DeviceContext>& devcon, ComPtr<ID3D11Buffer>& materialB, ComPtr<ID3D11BlendState>& blendOn, ComPtr<ID3D11BlendState>& blendOff)
{
	int size = staticMeshes.size();

	for (size_t i = 0; i < staticMeshes.size(); ++i) {

		if (m_UsePSShader)
		{
			Material meshMaterial = staticMeshes[i].GetMaterial();
			staticMeshes[i].SetShadowV(m_UsePSShader);

			UINT sampleMask = 0xffffffff;
			//여기에 mesh의 enum클레스 확인후 넘겨줄값 설정 //알파블랜딩 사용여부 및 clip 사용유무까지
			if (staticMeshes[i].TransMode == Transparency::Opaque)
			{
				devcon->OMSetBlendState(blendOff.Get(), nullptr, 0xFFFFFFFF);
				meshMaterial.UseClip = 0;
			}
			else if (staticMeshes[i].TransMode == Transparency::Cutout)
			{
				devcon->OMSetBlendState(blendOff.Get(), nullptr, 0xFFFFFFFF);
				meshMaterial.UseClip = 1;
			}
			else if (staticMeshes[i].TransMode == Transparency::AlphaBlend)
			{
				devcon->OMSetBlendState(blendOn.Get(), nullptr, sampleMask);
				meshMaterial.UseClip = 0;
			}

			//

			devcon->UpdateSubresource(materialB.Get(), 0, nullptr, &meshMaterial, 0, 0);
			devcon->PSSetConstantBuffers(1, 1, materialB.GetAddressOf());
		}
		else
		{
			staticMeshes[i].SetShadowV(m_UsePSShader);
		}
		staticMeshes[i].Draw(devcon);
	}
	if (!m_UsePSShader)
	{
		devcon->OMSetBlendState(nullptr, nullptr, 0xFFFFFFFF);
	}
}


Mesh<VERTEX> ModelLoader::processMesh(aiMesh* mesh, const aiScene* scene, const aiMatrix4x4& parentTransform) {
	// Data to fill
	std::vector<VERTEX> vertices;
	std::vector<UINT> indices;				//최종 인덱스 버퍼 배열
	std::vector<Texture> textures;
	Material tempMaterial;
	tempMaterial.ambient = { 0.1f, 0.1f, 0.1f, 1.0f }; // 기본 Ambient
	tempMaterial.diffuse = { 1.0f, 1.0f, 1.0f, 1.0f }; // 기본 Diffuse
	tempMaterial.specular = { 0.0f, 0.0f, 0.0f, 1.0f }; // 기본 Specular


	aiMatrix4x4 M = parentTransform;
	aiMatrix3x3 N(M);   // 상단 3x3만 추출
	N.Inverse();
	N.Transpose();

	vertices.reserve(mesh->mNumVertices);

	for (UINT i = 0; i < mesh->mNumVertices; i++) {
		VERTEX vertex;

		aiVector3D pos = mesh->mVertices[i];
		pos = M * pos;  // aiMatrix4x4 * aiVector3D 연산자 있음
		vertex.position = { pos.x, pos.y, pos.z };

		// 노멀: normal matrix 적용
		if (mesh->HasNormals()) {
			aiVector3D n = mesh->mNormals[i];
			n = N * n;
			n.Normalize();
			vertex.normal = { n.x, n.y, n.z };
		}
		else {
			vertex.normal = { 0, 1, 0 }; // 혹시 노멀 없으면 기본값
		}

		// UV
		if (mesh->mTextureCoords[0]) {
			vertex.tex.x = (float)mesh->mTextureCoords[0][i].x;
			vertex.tex.y = (float)mesh->mTextureCoords[0][i].y;
		}
		else {
			vertex.tex = { 0, 0 };
		}

		// 탄젠트 / 비탄젠트도 회전
		if (mesh->mTangents) {
			aiVector3D t = mesh->mTangents[i];
			t = N * t;
			t.Normalize();
			vertex.tangent = { t.x, t.y, t.z };
		}

		if (mesh->mBitangents) {
			aiVector3D b = mesh->mBitangents[i];
			b = N * b;
			b.Normalize();
			vertex.bitangent = { b.x, b.y, b.z };
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

	if (mesh->mMaterialIndex >= 0) {
		aiMaterial* material = scene->mMaterials[mesh->mMaterialIndex];
		aiColor4D color;



		// Diffuse 색상 추출
		if (AI_SUCCESS == material->Get(AI_MATKEY_COLOR_DIFFUSE, color)) {
			tempMaterial.diffuse = { color.r, color.g, color.b, color.a };
		}
		else
		{
			tempMaterial.diffuse = { 1.0f, 1.0f, 1.0f, 1.0f };
		}
		// Ambient 색상 추출
		if (AI_SUCCESS == material->Get(AI_MATKEY_COLOR_AMBIENT, color)) {
			tempMaterial.ambient = { color.r, color.g, color.b, color.a };
		}
		// Specular 색상 추출 및 Shininess(파워)
		float shininess = 0.0f;
		if (AI_SUCCESS == material->Get(AI_MATKEY_COLOR_SPECULAR, color)) {
			tempMaterial.specular = { color.r, color.g, color.b, color.a };
		}
		if (AI_SUCCESS == material->Get(AI_MATKEY_SHININESS, shininess)) {
			tempMaterial.specular.w = shininess; // Specular의 w 컴포넌트를 Shininess로 사용 (필요하다면)
		}

		if (material->GetTextureCount(aiTextureType_DIFFUSE) > 0)
		{
			aiString texPath;
			if (material->GetTexture(aiTextureType_DIFFUSE, 0, &texPath) == AI_SUCCESS)
			{
				hasAlbedoTex = true;
			}
		}

		if (hasAlbedoTex)
		{
			// 텍스처가 진짜 색을 들고 있으니 tint 제거
			tempMaterial.diffuse = { 1,1,1,1 };
		}


	}

	if (mesh->mMaterialIndex >= 0) {
		float metallicFactor = 1.0f;
		float roughnessFactor = 1.0f;

		aiMaterial* material = scene->mMaterials[mesh->mMaterialIndex];
		

		if (material->Get(AI_MATKEY_METALLIC_FACTOR, metallicFactor) != AI_SUCCESS){
			metallicFactor = 0.0f;
		}
		if (material->Get(AI_MATKEY_ROUGHNESS_FACTOR, roughnessFactor) != AI_SUCCESS){
			roughnessFactor = 1.0f;
		}

		m_material.metallicFactor = metallicFactor;
		m_material.roughnessFactor = roughnessFactor;


		std::vector<Texture> MetallicMaps = this->loadMaterialTextures(material, aiTextureType_METALNESS, "METALLIC", scene);
		if (!MetallicMaps.empty()) {
			textures.insert(textures.end(), MetallicMaps.begin(), MetallicMaps.end());
		}
		else
		{
			Texture defaultMetallic = this->loadDefaultTexture("defaultMetallic.png", "METALLIC");
			textures.push_back(defaultMetallic);
		}
		
		std::vector<Texture> RoughnessMaps = this->loadMaterialTextures(material, aiTextureType_SHININESS, "ROUGHNESS", scene);
		if (!RoughnessMaps.empty()) {
			textures.insert(textures.end(), RoughnessMaps.begin(), RoughnessMaps.end());
		}
		else
		{
			Texture defaultRoughness = this->loadDefaultTexture("defaultRoughness.png", "ROUGHNESS");
			textures.push_back(defaultRoughness);
		}

		//aiMaterial* mat = material;

		//auto printTextures = [&](aiTextureType type, const char* name)
		//	{
		//		unsigned int count = mat->GetTextureCount(type);
		//		if (count == 0) return;

		//		std::cout << name << " count = " << count << std::endl;
		//		for (unsigned int i = 0; i < count; ++i)
		//		{
		//			aiString path;
		//			if (mat->GetTexture(type, i, &path) == AI_SUCCESS)
		//			{
		//				std::cout << "  [" << i << "] " << path.C_Str() << std::endl;
		//			}
		//		}
		//	};

		//printTextures(aiTextureType_DIFFUSE, "DIFFUSE");
		//printTextures(aiTextureType_SPECULAR, "SPECULAR");
		//printTextures(aiTextureType_NORMALS, "NORMALS");
		//printTextures(aiTextureType_SHININESS, "SHININESS");
		//printTextures(aiTextureType_METALNESS, "METALNESS");
		//printTextures(aiTextureType_DIFFUSE_ROUGHNESS, "DIFFUSE_ROUGHNESS");
		//printTextures(aiTextureType_UNKNOWN, "UNKNOWN");
	}


	if (mesh->mMaterialIndex >= 0) {
		aiMaterial* material = scene->mMaterials[mesh->mMaterialIndex];

		//데이터를 읽어왔을때 존재하지 않을 경우 기본 옵션 사용할수 있도록
		std::vector<Texture> diffuseMaps = this->loadMaterialTextures(material, aiTextureType_DIFFUSE, "DIFFUSE", scene);
		if (!diffuseMaps.empty()) {
			textures.insert(textures.end(), diffuseMaps.begin(), diffuseMaps.end());
		}
		else
		{
			Texture defaultdiffuse = this->loadDefaultTexture("defaultDiffuse.png", "DIFFUSE");
			textures.push_back(defaultdiffuse);
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


	return Mesh<VERTEX>(dev_, vertices, indices, textures, T_Mod, tempMaterial);
}

Mesh<BONEVERTEX> ModelLoader::processMesh(aiMesh* mesh, const aiScene* scene, int meshIndex)
{
	// Data to fill
	vector<BONEVERTEX> vertices;
	vector<UINT> indices;				//최종 인덱스 버퍼 배열
	vector<Texture> textures;
	Material tempMaterial;
	tempMaterial.ambient = { 0.1f, 0.1f, 0.1f, 1.0f }; // 기본 Ambient
	tempMaterial.diffuse = { 1.0f, 1.0f, 1.0f, 1.0f }; // 기본 Diffuse
	tempMaterial.specular = { 0.0f, 0.0f, 0.0f, 1.0f }; // 기본 Specular


	for (UINT i = 0; i < mesh->mNumVertices; i++) {
		BONEVERTEX vertex;

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


		vertex.BLENDINDICES = XMUINT4(0, 0, 0, 0);
		vertex.BLENDWEIGHT = XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f);

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

	if (mesh->mMaterialIndex >= 0) {
		aiMaterial* material = scene->mMaterials[mesh->mMaterialIndex];
		aiColor4D color;


		// Diffuse 색상 추출
		if (AI_SUCCESS == material->Get(AI_MATKEY_COLOR_DIFFUSE, color)) {
			tempMaterial.diffuse = { color.r, color.g, color.b, color.a };
		}
		// Ambient 색상 추출
		if (AI_SUCCESS == material->Get(AI_MATKEY_COLOR_AMBIENT, color)) {
			tempMaterial.ambient = { color.r, color.g, color.b, color.a };
		}
		// Specular 색상 추출 및 Shininess(파워)
		float shininess = 0.0f;
		if (AI_SUCCESS == material->Get(AI_MATKEY_COLOR_SPECULAR, color)) {
			tempMaterial.specular = { color.r, color.g, color.b, color.a };
		}
		if (AI_SUCCESS == material->Get(AI_MATKEY_SHININESS, shininess)) {
			tempMaterial.specular.w = shininess; // Specular의 w 컴포넌트를 Shininess로 사용 (필요하다면)
		}

		//데이터를 읽어왔을때 존재하지 않을 경우 기본 옵션 사용할수 있도록
		std::vector<Texture> diffuseMaps = this->loadMaterialTextures(material, aiTextureType_DIFFUSE, "DIFFUSE", scene);
		if (!diffuseMaps.empty()) {
			textures.insert(textures.end(), diffuseMaps.begin(), diffuseMaps.end());
		}
		else
		{
			Texture defaultdiffuse = this->loadDefaultTexture("defaultDiffuse.png", "DIFFUSE");
			textures.push_back(defaultdiffuse);
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

	if (dummyBone)
	{
		int targetBoneIndex = 0;

		auto it = this->rigidMeshToBoneMap.find(meshIndex);
		if (it != this->rigidMeshToBoneMap.end())
		{
			targetBoneIndex = it->second;
		}


		for (UINT v = 0; v < mesh->mNumVertices; v++)
		{
			vertices[v].BLENDINDICES = XMUINT4(targetBoneIndex, 0, 0, 0);
			vertices[v].BLENDWEIGHT = XMFLOAT4(1.0f, 0.0f, 0.0f, 0.0f);
		}
	}


	if (mesh->HasBones())
	{
		auto it = skel.m_MeshBoneData.find(meshIndex);
		if (it != skel.m_MeshBoneData.end())
		{
			const auto& perVertex = it->second;
			for (UINT v = 0; v < mesh->mNumVertices; ++v)
			{
				const TempVertexBoneData& bd = perVertex[v];
				vertices[v].BLENDINDICES = XMUINT4(
					bd.BoneIndices[0], bd.BoneIndices[1],
					bd.BoneIndices[2], bd.BoneIndices[3]
				);
				vertices[v].BLENDWEIGHT = XMFLOAT4(
					bd.BoneWeights[0], bd.BoneWeights[1],
					bd.BoneWeights[2], bd.BoneWeights[3]
				);
			}
		}
	}

	return Mesh<BONEVERTEX>(dev_, vertices, indices, textures, T_Mod , tempMaterial);
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
				loadEmbeddedTexture(embeddedTexture, texture.texture);
			}
			else {
				std::string filename = std::string(str.C_Str());
				filename = directory_ + '\\' + filename;
				std::wstring filenamews = std::wstring(filename.begin(), filename.end());
				std::replace(filenamews.begin(), filenamews.end(), '\\', '/');
				hr = loadTextureFromFile(filenamews, texture.texture);
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
	
	if (isSkeletal)
	{
		for (size_t i = 0; i < skeletalMeshes.size(); i++) {
			skeletalMeshes[i].Close();
		}
	}
	else
	{
		for (size_t i = 0; i < staticMeshes.size(); i++) {
			staticMeshes[i].Close();
		}
	}
}

int ModelLoader::Getweight() const
{
	return weight;
}

void ModelLoader::LoadSkeleton(const aiScene* scene, SkeletonInfo& skelinfo)
{
	skelinfo.Bones.clear();
	skelinfo.m_BoneMappingTable.clear();
	skelinfo.m_MeshMappingTable.clear();
	skelinfo.m_MeshBoneData.clear();

	// ----------------------------
	// PASS A) "씬 전체"에서 사용되는 본 이름 수집 + 노드 DFS로 전역 순서 고정
	// ----------------------------
	std::unordered_set<std::string> usedBoneNames;
	for (UINT mi = 0; mi < scene->mNumMeshes; ++mi) {
		aiMesh* m = scene->mMeshes[mi];
		if (!m->HasBones()) continue;
		for (UINT bj = 0; bj < m->mNumBones; ++bj) {
			usedBoneNames.insert(m->mBones[bj]->mName.C_Str());
		}
	}

	// 루트부터 DFS하며 "사용되는 본"만 뽑아 전역 순서 확정
	std::vector<std::string> orderedBones;
	std::function<void(aiNode*)> dfs = [&](aiNode* n) {
		std::string nm = n->mName.C_Str();
		if (usedBoneNames.count(nm)) orderedBones.push_back(nm);
		for (UINT i = 0; i < n->mNumChildren; ++i) dfs(n->mChildren[i]);
		};
	dfs(scene->mRootNode);

	// 혹시 사용본인데 트리에 없었다면(이례적) 뒤에라도 붙임
	if (orderedBones.size() < usedBoneNames.size()) {
		for (auto& nm : usedBoneNames) {
			// 트리에 없던 이름만 추가
			if (std::find(orderedBones.begin(), orderedBones.end(), nm) == orderedBones.end())
				orderedBones.push_back(nm);
		}
	}

	// ----------------------------
	// PASS B) 전역 순서대로 Bones/매핑 테이블 구성 + 계층/바인드로컬 채우기
	// ----------------------------
	skelinfo.Bones.reserve(orderedBones.size());
	for (size_t i = 0; i < orderedBones.size(); ++i) {
		skelinfo.m_BoneMappingTable[orderedBones[i]] = (int)i;
		BoneInfo bi{};
		bi.Name = orderedBones[i];
		bi.ParentIndex = -1;               // 일단 -1, 아래 SetBoneHierarchy에서 채움
		bi.BindLocal = Matrix::Identity;  //              "
		bi.OffsetTransform = Matrix::Identity; // PASS C에서 채움
		skelinfo.Bones.push_back(bi);
	}

	// 계층/바인드 로컬 채우기: 매핑에 없는 노드는 스킵
	auto SetBoneHierarchyFixed = [&](aiNode* node, int parentBoneIndex, auto&& self) -> void {
		std::string nodeName = node->mName.C_Str();
		int currentBoneIndex = -1;

		auto it = skelinfo.m_BoneMappingTable.find(nodeName);
		if (it != skelinfo.m_BoneMappingTable.end()) {
			currentBoneIndex = it->second;

			Matrix f = ConvertAiMatrixToDXMatrix(node->mTransformation);
			skelinfo.Bones[currentBoneIndex].BindLocal = f;

			skelinfo.Bones[currentBoneIndex].ParentIndex = parentBoneIndex;

			parentBoneIndex = currentBoneIndex;
		}

		for (UINT i = 0; i < node->mNumChildren; ++i)
			self(node->mChildren[i], parentBoneIndex, self);
		};
	SetBoneHierarchyFixed(scene->mRootNode, -1, SetBoneHierarchyFixed);

	// ----------------------------
	// PASS C) 메시별: 오프셋/가중치만 전역 인덱스로 채우기 + 가중치 정규화
	// ----------------------------
	for (UINT mi = 0; mi < scene->mNumMeshes; ++mi)
	{
		aiMesh* mesh = scene->mMeshes[mi];
		if (!mesh->HasBones()) continue;

		int meshIndex = (int)mi;
		std::string meshName = mesh->mName.C_Str();
		skelinfo.m_MeshMappingTable[meshName] = meshIndex;

		// per-vertex 컨테이너 준비
		skelinfo.m_MeshBoneData[meshIndex].clear();
		skelinfo.m_MeshBoneData[meshIndex].resize(mesh->mNumVertices);

		for (UINT bj = 0; bj < mesh->mNumBones; ++bj)
		{
			aiBone* b = mesh->mBones[bj];
			std::string boneName = b->mName.C_Str();

			auto it = skelinfo.m_BoneMappingTable.find(boneName);
			if (it == skelinfo.m_BoneMappingTable.end()) {
				// 정상적이면 여기 안 옴(usedBoneNames/orderedBones에 포함되어야 함)
				continue;
			}
			int boneIndex = it->second;

			// Offset: 처음 한 번만 세팅(루트는 필요시 Identity 유지 테스트 권장)
			if (skelinfo.Bones[boneIndex].OffsetTransform == Matrix::Identity) {
				Matrix off = ConvertAiMatrixToDXMatrix(b->mOffsetMatrix);
				skelinfo.Bones[boneIndex].OffsetTransform = off;
			}
			else {
				// 필요시 동일성(가까움) 체크 가능
			}

			// 가중치 기록(최대 4개)
			for (UINT w = 0; w < b->mNumWeights; ++w)
			{
				const aiVertexWeight& vw = b->mWeights[w];
				auto& data = skelinfo.m_MeshBoneData[meshIndex][vw.mVertexId];

				if (data.Count < 4) {
					data.BoneIndices[data.Count] = boneIndex;
					data.BoneWeights[data.Count] = (float)vw.mWeight;
					data.Count++;
				}
			}
		}

		// 정규화
		for (auto& v : skelinfo.m_MeshBoneData[meshIndex]) {
			float sum = v.BoneWeights[0] + v.BoneWeights[1] + v.BoneWeights[2] + v.BoneWeights[3];
			if (sum > 1e-6f) {
				float inv = 1.0f / sum;
				for (int k = 0; k < 4; ++k) v.BoneWeights[k] *= inv;
			}
			else {
				// 어떤 뼈에도 배정되지 않은 정점은 루트 100%로 고정
				// (루트 인덱스가 0이 아니라면 실제 루트 인덱스로 바꿔줄 것)
				v.BoneIndices[0] = 0;
				v.BoneWeights[0] = 1.0f;
				v.BoneIndices[1] = v.BoneIndices[2] = v.BoneIndices[3] = 0;
				v.BoneWeights[1] = v.BoneWeights[2] = v.BoneWeights[3] = 0.0f;
			}

		}
	}

	skelinfo.DebugValidateBindPoseOnce();
}

SkeletonInfo* ModelLoader::GetSkeletonInfo()
{
	return &skel;
}

const Animation* ModelLoader::GetAnimation(const string& name) const
{
	auto it = animelist.find(name);
	return (it != animelist.end()) ? it->second : nullptr;
}

map<string, Animation*>& ModelLoader::Getanimelist()
{
	return animelist;
}

const vector<string>* ModelLoader::GetAnimeName()
{
	return &animeName;
}

bool ModelLoader::HasSkeleton() const
{
	//비어있다면 false리턴하도록
	return !skel.Bones.empty();
}

bool ModelLoader::HasAnimations() const
{
	//비어있다면 false 리턴하도록
	return !animelist.empty();
}

void ModelLoader::LoadNodeHierarchy(const aiNode* node, int parentBoneIndex, vector<BoneInfo>& outBones, map<string, int>& outBoneMappingTable)
{
	BoneInfo newBone;
	newBone.Name = node->mName.C_Str();
	newBone.ParentIndex = parentBoneIndex;

	// aiMatrix4x4를 사용하는 Matrix 타입으로 변환 (변환 함수가 있다고 가정)
	newBone.BindLocal = ConvertAiMatrixToDXMatrix(node->mTransformation);

	//  BoxMan의 핵심: 스키닝이 없으므로 Offset은 Identity로 설정
	// (Final = GBT * Offset 이므로, GBT만 사용하게 됨)
	newBone.OffsetTransform = Matrix::Identity;

	// (선택 사항) GBT 계산에 필요한 BindGlobal 계산을 할 수도 있으나,
	// AnimationController의 GBT 누적 로직이 이를 대신합니다.

	outBones.push_back(newBone);


	int currentBoneIndex = (int)outBones.size() - 1;

	outBoneMappingTable[newBone.Name] = currentBoneIndex;

	// 자식 노드를 순회하며 재귀 호출
	for (UINT i = 0; i < node->mNumChildren; ++i)
	{
		// 현재 노드의 인덱스를 자식의 ParentIndex로 전달
		LoadNodeHierarchy(node->mChildren[i], currentBoneIndex, outBones, outBoneMappingTable);
	}
}

XMFLOAT4X4 ModelLoader::ConvertAiMatrixToDXMatrix(const aiMatrix4x4& aiM)
{
	DirectX::XMFLOAT4X4 convertmatrix;


	convertmatrix._11 = aiM.a1; convertmatrix._12 = aiM.b1; convertmatrix._13 = aiM.c1; convertmatrix._14 = aiM.d1;
	convertmatrix._21 = aiM.a2; convertmatrix._22 = aiM.b2; convertmatrix._23 = aiM.c2; convertmatrix._24 = aiM.d2;
	convertmatrix._31 = aiM.a3; convertmatrix._32 = aiM.b3; convertmatrix._33 = aiM.c3; convertmatrix._34 = aiM.d3;
	convertmatrix._41 = aiM.a4; convertmatrix._42 = aiM.b4; convertmatrix._43 = aiM.c4; convertmatrix._44 = aiM.d4;

	return convertmatrix;
}

void ModelLoader::SetBoneHierarchy(aiNode* currentNode, int parentBoneIndex, SkeletonInfo& skelinfo)
{
	//넘겨받은 노드의 이름을 저장 ( 처음에는 Root로 시작함 , root를 던져주니깐 )
	string nodeName = currentNode->mName.C_Str();
	
	//변수생성으로 -1 , 루트의 부모는 없기 때문에 -1로 초기값 설정
	int currentBoneIndex = -1;
	//map변수인 테이블에서 nodeName이름이 존재하는지 확인
	if (skelinfo.m_BoneMappingTable.count(nodeName))
	{
		//존재할때 테이블에서 해당 이름이 가진 index를 저장
		currentBoneIndex = skelinfo.m_BoneMappingTable.at(nodeName);
		
		skelinfo.Bones[currentBoneIndex].BindLocal = ConvertAiMatrixToDXMatrix(currentNode->mTransformation);

		//이때 해당 노드가 root가 아닐경우
		if (parentBoneIndex != -1)
		{
			//현재 노드의 본의 부모를 설정해줌 , root의 부모는 없다
			skelinfo.Bones[currentBoneIndex].ParentIndex = parentBoneIndex;
		}
	}



	//현재 노드의 자식 수만큼 재귀하기 위한 for문
	for (UINT i = 0; i < currentNode->mNumChildren; i++)
	{
		//root 일경우 해당 이름이 가진 index를 아닐경우 부모의 index로 설정
		int nextParentIndex = (currentBoneIndex != -1) ? currentBoneIndex : parentBoneIndex;

		//재귀함수에 자식 정보를 던져준다
		SetBoneHierarchy(currentNode->mChildren[i], nextParentIndex, skelinfo);
	}
}

void ModelLoader::processNode(aiNode* node, const aiScene* scene, const aiMatrix4x4& parentTransform) {

	aiMatrix4x4 nodeTransform = parentTransform * node->mTransformation;

	for (UINT i = 0; i < node->mNumMeshes; i++) {
		int meshIndex = node->mMeshes[i]; // scene 전체 기준 인덱스
		aiMesh* mesh = scene->mMeshes[node->mMeshes[i]];
		if (isSkeletal)
		{
			// 스키닝 메시
				skeletalMeshes.push_back(this->processMesh(mesh, scene, meshIndex));
		}
		else
		{
			// 정적 메시
			staticMeshes.push_back(this->processMesh(mesh, scene, nodeTransform));
		}
	}

	for (UINT i = 0; i < node->mNumChildren; i++) {
		this->processNode(node->mChildren[i], scene, nodeTransform);
	}
}

bool ModelLoader::loadEmbeddedTexture(const aiTexture* embeddedTexture, ComPtr<ID3D11ShaderResourceView>& o_texture) {
	HRESULT hr;
	ID3D11ShaderResourceView* texture = nullptr;
	ComPtr<ID3D11Texture2D> tex2D;

	if (embeddedTexture->mHeight != 0) {
		// Load an uncompressed ARGB8888 embedded texture

		int width = embeddedTexture->mWidth;
		int height = embeddedTexture->mHeight;
		const unsigned char* pixelData = reinterpret_cast<const unsigned char*>(embeddedTexture->pcData);

		D3D11_TEXTURE2D_DESC desc;
		desc.Width = width;
		desc.Height = height;
		desc.MipLevels = 1;
		desc.ArraySize = 1;
		desc.SampleDesc.Count = 1;
		//desc.SampleDesc.Quality = 0; //Count1을 쓰면 생략 가능
		desc.Usage = D3D11_USAGE_DEFAULT;
		desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
		desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
		desc.CPUAccessFlags = 0;
		desc.MiscFlags = 0;

		D3D11_SUBRESOURCE_DATA subresourceData;
		subresourceData.pSysMem = pixelData;
		subresourceData.SysMemPitch = width * 4;
		subresourceData.SysMemSlicePitch = width * height * 4;

		
		HRESULT hr = dev_->CreateTexture2D(&desc, &subresourceData, tex2D.GetAddressOf());
		if (FAILED(hr)) {
			MessageBox(hwnd_, L"RGB Texture created failed!", L"Error!", MB_ICONERROR | MB_OK);
			return false;
		}
		hr = dev_->CreateShaderResourceView(tex2D.Get(), nullptr, o_texture.GetAddressOf());
		return SUCCEEDED(hr);
	}

	// mHeight is 0, so try to load a compressed texture of mWidth bytes
	const uint8_t* data = reinterpret_cast<const uint8_t*>(embeddedTexture->pcData);
	const size_t size = embeddedTexture->mWidth;

	hr = CreateWICTextureFromMemory(dev_.Get(), devcon_.Get(), data, size, nullptr, o_texture.GetAddressOf());
	//if (FAILED(hr))
	//	MessageBox(hwnd_, L"Texture couldn't be created from memory!", L"Error!", MB_ICONERROR | MB_OK);
	if (SUCCEEDED(hr)) return true;

	//tga 파일 로드, 이후 dds 파일도 사용한다면 아래에 분기 추가
	{
		TexMetadata meta;
		ScratchImage img;
		hr = LoadFromTGAMemory(data, size, &meta, img);
		if (SUCCEEDED(hr)) {
			hr = CreateShaderResourceView(dev_.Get(), img.GetImages(), img.GetImageCount(), meta, o_texture.GetAddressOf());
			return SUCCEEDED(hr);
		}
	}	

	return false;
}

HRESULT ModelLoader::loadTextureFromFile(const std::wstring& filename, ComPtr<ID3D11ShaderResourceView>& o_texture)
{
	std::ifstream file(filename, std::ios::binary | std::ios::ate);
	if (!file) { 
		MessageBox(hwnd_, L"Texture not found", L"Error!", MB_ICONERROR | MB_OK);
		return E_FAIL;
	}

	auto size = file.tellg();
	file.seekg(0);
	std::vector<uint8_t> data(size);
	file.read(reinterpret_cast<char*>(data.data()), size);

	// 시도할 파일로더(png jpg tga 등) 순차적으로 추가 //dds나 다른형태의파일이 있을때 추가적으로 넣기
	std::vector<std::function<HRESULT()>> loaders;
	loaders.push_back([&]() { return CreateWICTextureFromMemory(dev_.Get(), devcon_.Get(), data.data(), size, nullptr, o_texture.GetAddressOf()); });
	loaders.push_back([&]() {
		TexMetadata meta;
		ScratchImage img;
		HRESULT hr = LoadFromTGAMemory(data.data(), size, &meta, img);
		if (SUCCEEDED(hr))
			hr = CreateShaderResourceView(dev_.Get(), img.GetImages(), img.GetImageCount(), meta, o_texture.GetAddressOf());
		return hr;
		});

	HRESULT hr = E_FAIL;
	for (auto& loader : loaders) {
		hr = loader();
		if (SUCCEEDED(hr)) break;
	}
	return hr;
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
		std::wstring msg = L"Failed to load default texture: " + fullPathWS + L" HRESULT: " + std::to_wstring(hr);
		MessageBox(NULL, msg.c_str(), L"Texture Load Error", MB_ICONERROR | MB_OK);
	}

	texture.type = typeName;
	texture.path = filename; // 경로를 파일명으로 저장
	this->textures_loaded_.push_back(texture);
	return texture;
}