#pragma once
#include <d3d11.h>
#include <vector>
#include "../Common/GameApp.h"
#include <imgui.h>
#include <imgui_impl_win32.h>
#include <imgui_impl_dx11.h>
#include <directxtk/SimpleMath.h>
#include <memory>
#include <utility>
#include <algorithm>
#include "ModelLoader.h"
#include "AnimationController.h"
#include "../Common/DebugDraw.h"


using namespace DirectX::SimpleMath;
using namespace std;

//윈도우 스마트 포인터용
#include <wrl/client.h>
using Microsoft::WRL::ComPtr;


struct VertexSky
{
	Vector3 position;		// 정점 위치
	VertexSky(Vector3 _position) : position(_position) {};
};


struct FloorVertex
{
	XMFLOAT3 pos;
	XMFLOAT2 Tex;
	XMFLOAT3 Tan;
	XMFLOAT3 BiTan;
	XMFLOAT3 Norm;
};

struct SetPSValue
{
	int Setvalue = 0;
	Vector3 padding;
};

struct PBRValue
{
	float metalicV;
	float roughnessV;
	Vector2 padding;

	int UseModelValue;
	Vector3 MetalColor;
};

struct Constant
{
	XMFLOAT4X4 world;
	XMFLOAT4X4 view;
	XMFLOAT4X4 projection;
	XMFLOAT4X4 worldinverseT;

	Vector4 vLightDir;
	Vector4 vLightColor;

	Vector3 camPos;						//카메라위치

	float clipValue = 0.5;
};

struct FinalBoneMatrix
{
	Matrix gBoneMatrices[128];
};


struct SkyConstant
{
	XMFLOAT4X4 world;
	XMFLOAT4X4 view;
	XMFLOAT4X4 projection;
};

struct TransformVP
{
	XMFLOAT4X4 ShadowView;
	XMFLOAT4X4 ShadowProjection;
};


class PBR : public GameApp
{
public:
	PBR(HINSTANCE hInstance);
	~PBR();

	// Model
	std::shared_ptr<ModelLoader> BoxMan = nullptr;
	std::shared_ptr<ModelLoader> SkinningTest = nullptr;
	std::shared_ptr<ModelLoader> Table = nullptr;
	std::shared_ptr<ModelLoader> Box = nullptr;
	std::shared_ptr<ModelLoader> Char = nullptr;



	// 렌더링 파이프라인을 구성하는 필수 객체의 인터페이스 (  뎊스 스텐실 뷰도 있지만 아직 사용하지 않는다.)
	ComPtr<ID3D11Device> m_pDevice = nullptr;						// 디바이스	
	ComPtr<ID3D11DeviceContext> m_pDeviceContext = nullptr;		// 즉시 디바이스 컨텍스트
	IDXGISwapChain* m_pSwapChain = nullptr;					// 스왑체인
	ID3D11RenderTargetView* m_pRenderTargetView = nullptr;	// 렌더링 타겟뷰
	ID3D11DepthStencilView* m_pDepthStencilView = nullptr;	// 깊이 타겟뷰


	// 렌더링 파이프라인에 적용하는  객체와 정보
	ID3D11VertexShader* m_pVertexShader = nullptr;		// 정점 셰이더.
	ComPtr<ID3D11VertexShader> m_pBoneVertexShader = nullptr;		// 정점 셰이더.
	ComPtr<ID3D11VertexShader> m_pShadowVertexShader = nullptr;		// 정점 셰이더.
	ComPtr<ID3D11VertexShader> m_NormalShadowVertexShader = nullptr;		// 정점 셰이더.
	ID3D11PixelShader* m_pPixelShader = nullptr;		// 픽셀 셰이더.	
	ComPtr<ID3D11PixelShader> m_pBlinnPhongShader = nullptr;		// 픽셀 셰이더
	ComPtr<ID3D11PixelShader> m_pPBRShader = nullptr;		// 픽셀 셰이더
	ID3D11PixelShader* m_pPixelShaderSolid = nullptr;	//단일 색상 픽셀 셰이더
	ID3D11InputLayout* m_pInputLayout = nullptr;		// 입력 레이아웃.
	ID3D11Buffer* m_pConstantBuffer = nullptr;			// 상수 버퍼.
	ID3D11Buffer* m_tConstantBuffer = nullptr;			// 텍스쳐 상수 버퍼.
	ID3D11ShaderResourceView* m_pTextureRV = nullptr;	// 텍스처 리소스 뷰.
	ID3D11SamplerState* m_pSamplerLinear = nullptr;		// 샘플러 상태.

	UINT m_VertextBufferStride = 0;					// 버텍스 하나의 크기.
	UINT m_VertextBufferOffset = 0;					// 버텍스 버퍼의 오프셋.
	UINT m_VertexCount = 0;							// 버텍스 개수.
	UINT m_IndexCount = 0;							// 인덱스 개수

	void Reset();
	void SetMatrix();

	//Update에서 갱신할 Matrix들
	XMMATRIX m_View{};
	XMMATRIX m_Proj{};

	XMMATRIX m_ZeldaWorld{};
	XMMATRIX m_CharWorld{};
	XMMATRIX m_TreeWorld{};


	//카메라 변수
	Matrix m_CWorld;
	Vector3 m_Position;
	Vector3 m_Rotation;


	//카메라 보정함수
	void SanitizeCamera(Vector3& eye, Vector3& target, Vector3& up);

	//Projection 변수
	float fovy = XMConvertToRadians(45.0f);    // 라디안 단위 세로 시야각
	float aspect = 1024.0f / 768.0f;  // width / height
	float zNear = 0.1f;;   // near plane
	float zFar = 1000.0f;    // far plane


	//update를 위해 맴버변수로 둠
	Constant constandices{};
	


	virtual bool Initialize(UINT Width, UINT Height);
	virtual void Update();
	virtual void Render();

	bool InitD3D();
	void UninitD3D();
	void UninitSkyBox();

	bool InitScene();		// 쉐이더,버텍스,인덱스
	bool InitSkyBox();		// 스카이 박스 init용
	void RenderSkyBox();	// 스카이 박스 render용
	void UninitScene();

	bool InitEffect();

	//Vertex Buffer + Index Buffer 조합을 쓰기때문에 아래와 같이 따로 필요가 없음
	//Vertex Buffer에는 중복없는 정점 데이터만 넣고 Index Buffer에는 이것들을 어떻게 이을건지에 대한 정보를 담음
	UINT StartSlot = 0;					//연결을 시작할 첫 슬롯의 번호, 여러개의 버퍼에 정보를 나눠서 담을때 사용
	UINT NumBuffers = 0;				//이때 몇개의 버퍼를 이어서 그릴지 필요한 숫자


	// 멤버 변수 (ImGui UI 상태)
	bool  m_show_another_window = false;
	bool  m_show_demo_window = true;
	float m_f = 0.0f;
	int   m_counter = 0;

	// ImGui 초기화/해제 선언
	bool InitImGUI();
	void UninitImGUI();
	void RenderGUI();

	float CamPosition[3] = { 0.0f , 0.0f, -10.0f };
	float CamRotation[3] = { 0.0f , 0.0f, 0.0f };
	const float eps = 0.01f;
	float CamFovy = 45.0f;

	float Pspeed = 2.0f;
	float Ch1speed = 4.0f;

	virtual LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);

	//빛 관련 변수들

	//라이트 색
	XMFLOAT4 m_LightColors = { XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f) };
	XMFLOAT4 m_LightDirsEvaluated = { XMFLOAT4(1.0f , 0.0f, 0.0f , 1.0f) };


	float LightColor1[3] = { 1.0f, 1.0f, 1.0f };

	float LightDir1[3] = { 0.0f, -1.0f, 1.0f };

	ComPtr<ID3D11VertexShader> S_VertexShader;
	ComPtr<ID3D11PixelShader> S_PixelShader;
	ComPtr<ID3D11InputLayout> S_InputLayout;

	ComPtr<ID3D11Buffer> S_VertexBuffer;
	ComPtr<ID3D11Buffer> S_IndexBuffer;
	ComPtr<ID3D11Buffer> S_ConstantBuffer;   // Constant buffer (view/proj 등, 16바이트 배수 정렬)

	ComPtr<ID3D11Buffer> m_pMaterialBuffer{};					// 재질 버퍼

	ComPtr<ID3D11ShaderResourceView> S_CubeSRV;   // TextureCube
	ComPtr<ID3D11SamplerState> S_Sampler;   // 보통 공용 가능

	ComPtr<ID3D11DepthStencilState> S_DepthStencilState;
	ComPtr<ID3D11RasterizerState>   S_RasterizerState;



	ComPtr<ID3D11DepthStencilState>	m_pDepthStateDefault;
	ComPtr<ID3D11RasterizerState>  m_pRasterizerStateDefault;
	// 노말맵 리소스
	ComPtr<ID3D11ShaderResourceView> M_ptexture;
	ComPtr<ID3D11ShaderResourceView> M_pnormal;
	ComPtr<ID3D11ShaderResourceView> M_pspecular;

	// 블랜더 변수
	ComPtr<ID3D11BlendState> m_pBlendON;
	ComPtr<ID3D11BlendState> m_pBlendOFF;

	UINT S_VertexCount = 0;
	UINT S_IndexCount = 0;
	UINT S_VertexBufferStride = 0;
	UINT S_VertexBufferOffset = 0;


	//카메라 변수
	Quaternion m_COri = Quaternion::Identity; // 카메라 방향
	bool       m_RotateAboutLocal = true;
	Vector3    m_UIRotPrev = { 0,0,0 };

	float speed = 20.0f;
	bool BlinPhongTrue = true;

	//랜더링 순서 계산 함수와 변수
	void SetRenderSort();
	vector< pair<float, shared_ptr<ModelLoader>>> renderlist{};

	float clipValue = 0.5f;

	//애니메이션 로드 함수
	//BoxMan
	//SkinningTest
	AnimationController* PlayBoxHuman{};
	AnimationController* PlaySkinningTest{};
	vector<string> animelist_BoxHuman{};
	vector<string> animelist_SkinningTest{};
	string tempplayname1{};
	string tempplayname2{};

	FinalBoneMatrix FinalBoneCS{};
	TransformVP ShadowVP{};
	//SetPSValue SetPSV{};
	PBRValue SetPBR{};

	ComPtr<ID3D11Buffer> m_pBoneConstantBuffer = nullptr;			// 뼈상수 버퍼.
	ComPtr<ID3D11Buffer> m_pShadowConstantBuffer = nullptr;			// 그림자 상수 버퍼.
	ComPtr<ID3D11Buffer> m_pPBRConstantBuffer = nullptr;			// PBR상수버퍼.
	ComPtr<ID3D11InputLayout> m_pBoneInputLayout = nullptr;		// 입력 레이아웃.



	XMMATRIX m_BoxManWorld{};
	XMMATRIX m_SkinningTestWorld{};
	XMMATRIX m_TableWorld{};
	XMMATRIX m_Floor{};
	XMMATRIX m_Char{};

	//world 관련 변수 //
	XMFLOAT3 P_BoxManposition{ -5.0f, 0.0f, 5.0f };
	XMFLOAT3 P_BoxManrotation{ 0.0f, 0.0f, 0.0f };
	XMFLOAT3 P_BoxManScale{ 1.0f, 1.0f, 1.0f };

	XMFLOAT3 P_SkinningTestposition{ 0.0f, 0.0f, 0.0f };
	XMFLOAT3 P_SkinningTestrotation{ 0.0f, 0.0f, 0.0f };
	XMFLOAT3 P_SkinningTestScale{ 0.01f, 0.01f, 0.01f };

	XMFLOAT3 P_Tableposition{ 5.0f, -5.0f, 5.0f };
	XMFLOAT3 P_Tablerotation{ 0.0f, 0.0f, 0.0f };
	XMFLOAT3 P_TableScale{ 0.01f, 0.01f, 0.01f };


	XMFLOAT3 P_Floorposition{ 0.0f, -2.0f, 0.0f };
	XMFLOAT3 P_Floorrotation{ 0.0f, 0.0f, 0.0f };
	XMFLOAT3 P_FloorScale{ 0.07f, 0.001f, 0.07f };

	XMFLOAT3 P_Charposition{ 1.0f, 1.0f, 0.0f };
	XMFLOAT3 P_Charrotation{ 0.0f, 0.0f, 0.0f };
	XMFLOAT3 P_CharScale{ 0.01f, 0.01f, 0.01f };

	
	

	double DurationAnime;
	double TicksPerSecond;
	double currentTime;
	bool stoptime = false;

	double DurationAnime2;
	double TicksPerSecond2;
	double currentTime2;
	bool stoptime2 = false;

	D3D11_VIEWPORT m_ShadowViewport{};

	ComPtr<ID3D11DepthStencilView> m_ShadowDepthStencilView{};	
	ComPtr<ID3D11Texture2D> m_pShadowMap{};						
	ComPtr<ID3D11DepthStencilView> m_pShadowMapDSV{};			//깊이값 기록을 설정하기 위한 객체
	ComPtr<ID3D11ShaderResourceView> m_pShadowMapSRV{};			//셰이더에서 깊이버퍼를 슬롯에 설정하고 사용하기 위한 객체

	void SetLightVP();
	//void GetFrustumCorners(Vector3 frustumCorners[8]);

	float INITIAL_DISTANCE = 500.0f;

	D3D11_VIEWPORT viewport = {};

	ComPtr<ID3D11RasterizerState> m_pRasterizerShadowBias;			//얇은 검은 줄/얼룩(애크네)을 막기위해 설정

	
	FloorVertex testFloor[4]{};

	void RenderFloor(int value);

	UINT FloorIndex[6]{};

	
	ComPtr <ID3D11Buffer> m_fVertextBuffer{};
	ComPtr <ID3D11Buffer> m_fIndexBuffer{};


	void SetHoldLight();

	void DebugMatrix4x4(const char* label, const Matrix& matrix);

	float boxSize = 80.0f;
	float depthNear = 10.0f;
	float depthFar = 200.0f;

	void SetDebugLightFrustum();
	void RenderDebugLightFrustum();
	ComPtr<ID3D11InputLayout> S_DebugInputLayout;

	using VertexType = DirectX::VertexPositionColor;

	std::unique_ptr<DirectX::CommonStates> m_states;
	std::unique_ptr<DirectX::BasicEffect> m_effect;
	std::unique_ptr<DirectX::PrimitiveBatch<VertexType>> m_batch;
	Microsoft::WRL::ComPtr<ID3D11InputLayout> m_inputLayout;


	//PBR
	float m_metalic = 0.0f;
	float m_roughness = 1.0f;

	void SetPBRValue(std::shared_ptr<ModelLoader> mesh);
	float Metalcolor[3] = { 1.0f, 1.0f, 1.0f };

	bool useModelValues = true;
};
