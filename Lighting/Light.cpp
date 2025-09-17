#include "Light.h"
#include "../Common/Helper.h"
#include <directxtk/simplemath.h>
#include <d3dcompiler.h>

#pragma comment (lib, "d3d11.lib")
#pragma comment(lib,"d3dcompiler.lib")

using namespace DirectX::SimpleMath;

/*
	  6-----7
	 /|    /|
	1-+---3-+
	| |   | |
	| 4---|-5
	|/    |/
	0-----2
*/


Light::Light(HINSTANCE hInstance) : GameApp(hInstance)
{

}


Light::~Light()
{
	UninitScene();
	UninitD3D();
}

void Light::SetMatrix()
{
	//부모는 World matrix = local matrix 이므로 W_ 변수명
	XMMATRIX S = XMMatrixScaling(P_Scale.x, P_Scale.y, P_Scale.z);
	XMMATRIX R = XMMatrixRotationRollPitchYaw(P_rotation.x, P_rotation.y, P_rotation.z);
	XMMATRIX T = XMMatrixTranslation(P_position.x, P_position.y, P_position.z);
	m_Wolrd = S * R * T;

	//view(카메라 수치)
	//target = XMVector3TransformCoord(XMVectorZero(), m_Wolrd);
	/*XMStoreFloat3(&P_position, target);*/
	SanitizeCamera(eye, target, up);
	m_View = XMMatrixLookAtLH(eye, target, up);

	//projection 설정
	m_Proj = XMMatrixPerspectiveFovLH(fovy, aspect, zNear, zFar);
}


void Light::SanitizeCamera(XMVECTOR& eye, XMVECTOR& target, XMVECTOR& up)
{
	//최소거리 보장용 변수
	const float kMinDist = 0.05f;

	//비교할 거리
	XMVECTOR dir = target - eye;
	//float로 변경 ( XMVector3LengthSq(dir) -> 제곱길이 계산 , XMVectorGetX()-> X성분하나만 return해주는 함수
	//제곱으로 하는 이유는 성능, 등가성, SIMD 친화 등의 이유가 있음
	float d2 = XMVectorGetX(XMVector3LengthSq(dir));
	if (d2 < kMinDist * kMinDist)
	{
		XMVECTOR fallback = XMVectorSet(0, 0, 1, 0);
		//임계치보다 높으면 dir 사용, 0에 가깝다면 fallback 사용
		XMVECTOR safeDir = (d2 > 1e-10f) ? XMVector3Normalize(dir) : fallback;
		target = eye + safeDir * kMinDist;
	}

	// up이 전방과 거의 평행이면 교체 (평행이면 다른축을 임시 UP으로 사용)
	XMVECTOR z = XMVector3Normalize(target - eye);
	XMVECTOR upN = XMVector3Normalize(up);
	float cosPar = fabsf(XMVectorGetX(XMVector3Dot(z, upN)));
	if (cosPar > 0.999f) up = XMVectorSet(0, 1, 0, 0);
}

bool Light::Initialize(UINT Width, UINT Height)
{
	__super::Initialize(Width, Height);

	if (!InitD3D())
		return false;

	if (!InitImGUI())
		return false;

	if (!InitScene())
		return false;

	return true;
}

void Light::Update()
{
	__super::Update();
	float t = GameTimer::m_Instance->DeltaTime();

	SetMatrix();
}

void Light::Render()
{
	float color[4] = { 0.0f, 0.5f, 0.5f, 1.0f };



	// 컬러 버퍼(RTV) 초기화
	m_pDeviceContext->ClearRenderTargetView(m_pRenderTargetView, color);
	//깊이 버퍼초기화
	m_pDeviceContext->ClearDepthStencilView(m_pDepthStencilView, D3D11_CLEAR_DEPTH, 1.0f, 0);

	/*Constant cb1;*/
	XMStoreFloat4x4(&constandices.world, XMMatrixTranspose(m_Wolrd));
	XMStoreFloat4x4(&constandices.view, XMMatrixTranspose(m_View));
	XMStoreFloat4x4(&constandices.projection, XMMatrixTranspose(m_Proj));
	constandices.vLightDir[0] = m_LightDirsEvaluated[0];
	constandices.vLightDir[1] = m_LightDirsEvaluated[1];
	constandices.vLightColor[0] = m_LightColors[0];
	constandices.vLightColor[1] = m_LightColors[1];
	constandices.vOutputColor = XMFLOAT4(0, 0, 0, 0);

	// Draw계열 함수를 호출하기전에 렌더링 파이프라인에 필수 스테이지 설정을 해야한다.	
	m_pDeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST); // 정점을 이어서 그릴 방식 설정.

	// 정점버퍼를 바인딩 해주는 단계
	m_pDeviceContext->IASetVertexBuffers(0, 1, &m_pVertexBuffer, &m_VertextBufferStride, &m_VertextBufferOffset);
	//입력 레이아웃 바인딩
	m_pDeviceContext->IASetInputLayout(m_pInputLayout);

	// 인덱스 버퍼 바인딩
	m_pDeviceContext->IASetIndexBuffer(m_pIndexBuffer, DXGI_FORMAT_R32_UINT, 0);

	
	// 버텍스 셰이더 바인딩
	m_pDeviceContext->VSSetShader(m_pVertexShader, nullptr, 0);
	// 픽셀 셰이더 바인딩
	m_pDeviceContext->PSSetShader(m_pPixelShader, nullptr, 0);

	//임시 컨테이너 생성, Map 결과를 담을 임시 구조체
	D3D11_MAPPED_SUBRESOURCE mapped{};
	//Map -> GPU의 이전 메모리 블록 주소는 버리고 새로 넘겨줄 데이터를 담을 메모리 블록 주소를 CPU에 넘겨줌 ( 이 주소를 mapped 에 담음)
	//CPU 기준으로는 이전 메모리의 주소를 잃어버려 이전 값이 폐기, GPU기준으로는 이전 데이터를 내부적으로 보유하고 있어서 그리기 가능
	//밑의 상수 버퍼에서 D3D11_USAGE_DYNAMIC 설정했다는 가정하에
	m_pDeviceContext->Map(m_pConstantBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
	
	memcpy(mapped.pData, &constandices, sizeof(constandices));
	//D3D11_MAP_WRITE_DISCARD는 내부적으로 버퍼 리네이밍(새 메모리 조각 교체)을 유도하기때문에 여기서만 사용
	//-> 카피해준후 Unmap
	m_pDeviceContext->Unmap(m_pConstantBuffer, 0);
	//상수버퍼 바인딩

	m_pDeviceContext->VSSetConstantBuffers(0, 1, &m_pConstantBuffer);
	m_pDeviceContext->PSSetConstantBuffers(0, 1, &m_pConstantBuffer);
	m_pDeviceContext->PSSetShader(m_pPixelShader, nullptr, 0);

	//m_pDeviceContext->Draw(m_VertexCount, 0);				//인덱스 미사용시
	m_pDeviceContext->DrawIndexed(m_IndexCount, 0, 0);		//인덱스 사용시
	
	//단색 박스 설정
	for (int i = 0; i < 2; i++)
	{
		XMMATRIX mLight = XMMatrixTranslationFromVector(5.0f * XMLoadFloat4(&m_LightDirsEvaluated[i]));
		XMMATRIX mLightScale = XMMatrixScaling(0.2f, 0.2f, 0.2f);
		mLight = mLightScale * mLight;

		Constant cb = {};
		XMStoreFloat4x4(&cb.world, XMMatrixTranspose(mLight));
		XMStoreFloat4x4(&cb.view, XMMatrixTranspose(m_View));
		XMStoreFloat4x4(&cb.projection, XMMatrixTranspose(m_Proj));
		cb.vLightDir[i] = m_LightDirsEvaluated[i];
		cb.vLightColor[i] = m_LightColors[i];
		cb.vOutputColor = m_LightColors[i];

		D3D11_MAPPED_SUBRESOURCE mapped{};
		m_pDeviceContext->Map(m_pConstantBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
		memcpy(mapped.pData, &cb, sizeof(cb));
		m_pDeviceContext->Unmap(m_pConstantBuffer, 0);

		m_pDeviceContext->VSSetConstantBuffers(0, 1, &m_pConstantBuffer);
		m_pDeviceContext->PSSetConstantBuffers(0, 1, &m_pConstantBuffer);

		m_pDeviceContext->PSSetShader(m_pPixelShaderSolid, nullptr, 0);
		m_pDeviceContext->DrawIndexed(m_IndexCount, 0, 0);
	}


	RenderGUI();
	// Present the information rendered to the back buffer to the front buffer (the screen)
	m_pSwapChain->Present(0, 0);
}

bool Light::InitD3D()
{
	// 결과값.
	HRESULT hr = 0;

	// 스왑체인 속성 설정 구조체 생성.
	DXGI_SWAP_CHAIN_DESC swapDesc = {};
	swapDesc.BufferCount = 1;
	swapDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	swapDesc.OutputWindow = m_hWnd;	// 스왑체인 출력할 창 핸들 값.
	swapDesc.Windowed = true;		// 창 모드 여부 설정.
	swapDesc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	// 백버퍼(텍스처)의 가로/세로 크기 설정.
	swapDesc.BufferDesc.Width = m_ClientWidth;
	swapDesc.BufferDesc.Height = m_ClientHeight;
	// 화면 주사율 설정.
	swapDesc.BufferDesc.RefreshRate.Numerator = 60;
	swapDesc.BufferDesc.RefreshRate.Denominator = 1;
	// 샘플링 관련 설정.
	swapDesc.SampleDesc.Count = 1;
	swapDesc.SampleDesc.Quality = 0;

	UINT creationFlags = 0;
#ifdef _DEBUG
	creationFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif
	// 1. 장치 생성.   2.스왑체인 생성. 3.장치 컨텍스트 생성.
	HR_T(D3D11CreateDeviceAndSwapChain(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, creationFlags, NULL, NULL,
		D3D11_SDK_VERSION, &swapDesc, &m_pSwapChain, &m_pDevice, NULL, &m_pDeviceContext));

	// 4. 렌더타겟뷰 생성.  (백버퍼를 이용하는 렌더타겟뷰)	
	ID3D11Texture2D* pBackBufferTexture = nullptr;
	HR_T(m_pSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&pBackBufferTexture));
	HR_T(m_pDevice->CreateRenderTargetView(pBackBufferTexture, NULL, &m_pRenderTargetView));  // 텍스처는 내부 참조 증가
	SAFE_RELEASE(pBackBufferTexture);	//외부 참조 카운트를 감소시킨다.
	// 렌더 타겟을 최종 출력 파이프라인에 바인딩합니다.
	/*m_pDeviceContext->OMSetRenderTargets(1, &m_pRenderTargetView, NULL);*/

	// 뷰포트 설정.	
	D3D11_VIEWPORT viewport = {};
	viewport.TopLeftX = 0;
	viewport.TopLeftY = 0;
	viewport.Width = (float)m_ClientWidth;
	viewport.Height = (float)m_ClientHeight;
	viewport.MinDepth = 0.0f;
	viewport.MaxDepth = 1.0f;
	m_pDeviceContext->RSSetViewports(1, &viewport);
	return true;
}

void Light::UninitD3D()
{
	SAFE_RELEASE(m_pRenderTargetView);
	SAFE_RELEASE(m_pSwapChain);
	SAFE_RELEASE(m_pDeviceContext);
	SAFE_RELEASE(m_pDevice);
	UninitImGUI();
}

bool Light::InitScene()
{
	HRESULT hr = 0; // 결과값.
	ID3D10Blob* errorMessage = nullptr;	 // 컴파일 에러 메시지가 저장될 버퍼.	

	//정점하나에 법선 3개를 담을 수 없기 때문에 중복된 정점 버퍼도 정의해줘야함
	VertexL vertices[] =
	{
		//Noarmal Y+
		VertexL(Vector3(-1.0f, 1.0f, -1.0f), Vector3(0.0f, 1.0f, 0.0f)),
		VertexL(Vector3(1.0f, 1.0f, -1.0f), Vector3(0.0f, 1.0f, 0.0f)),
		VertexL(Vector3(1.0f, 1.0f, 1.0f), Vector3(0.0f, 1.0f, 0.0f)),
		VertexL(Vector3(-1.0f, 1.0f, 1.0f), Vector3(0.0f, 1.0f, 0.0f)),
		//Normal Y-
		VertexL(Vector3(-1.0f, -1.0f, -1.0f), Vector3(0.0f, -1.0f, 0.0f)),
		VertexL(Vector3(1.0f, -1.0f, -1.0f), Vector3(0.0f, -1.0f, 0.0f)),
		VertexL(Vector3(1.0f, -1.0f, 1.0f), Vector3(0.0f, -1.0f, 0.0f)),
		VertexL(Vector3(-1.0f, -1.0f, 1.0f), Vector3(0.0f, -1.0f, 0.0f)),
		//Normal X-
		VertexL(Vector3(-1.0f, -1.0f, 1.0f), Vector3(-1.0f, 0.0f, 0.0f)),
		VertexL(Vector3(-1.0f, -1.0f, -1.0f), Vector3(-1.0f, 0.0f, 0.0f)),
		VertexL(Vector3(-1.0f, 1.0f, -1.0f), Vector3(-1.0f, 0.0f, 0.0f)),
		VertexL(Vector3(-1.0f, 1.0f, 1.0f), Vector3(-1.0f, 0.0f, 0.0f)),
		//Normal X+
		VertexL(Vector3(1.0f, -1.0f, 1.0f), Vector3(1.0f, 0.0f, 0.0f)),
		VertexL(Vector3(1.0f, -1.0f, -1.0f), Vector3(1.0f, 0.0f, 0.0f)),
		VertexL(Vector3(1.0f, 1.0f, -1.0f), Vector3(1.0f, 0.0f, 0.0f)),
		VertexL(Vector3(1.0f, 1.0f, 1.0f), Vector3(1.0f, 0.0f, 0.0f)),
		//Normal Z-
		VertexL(Vector3(-1.0f, -1.0f, -1.0f), Vector3(0.0f, 0.0f, -1.0f)),
		VertexL(Vector3(1.0f, -1.0f, -1.0f), Vector3(0.0f, 0.0f, -1.0f)),
		VertexL(Vector3(1.0f, 1.0f, -1.0f), Vector3(0.0f, 0.0f, -1.0f)),
		VertexL(Vector3(-1.0f, 1.0f, -1.0f), Vector3(0.0f, 0.0f, -1.0f)),
		//Normal Z+
		VertexL(Vector3(-1.0f, -1.0f, 1.0f), Vector3(0.0f, 0.0f, 1.0f)),
		VertexL(Vector3(1.0f, -1.0f, 1.0f), Vector3(0.0f, 0.0f, 1.0f)),
		VertexL(Vector3(1.0f, 1.0f, 1.0f), Vector3(0.0f, 0.0f, 1.0f)),
		VertexL(Vector3(-1.0f, 1.0f, 1.0f), Vector3(0.0f, 0.0f, 1.0f)),
	};

	D3D11_BUFFER_DESC vbDesc = {};
	m_VertexCount = ARRAYSIZE(vertices);	// 정점의 수
	vbDesc.ByteWidth = sizeof(VertexL) * m_VertexCount; // 버텍스 버퍼의 크기(Byte).
	vbDesc.CPUAccessFlags = 0;
	vbDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER; // 정점 버퍼로 사용.
	vbDesc.MiscFlags = 0;
	vbDesc.Usage = D3D11_USAGE_DEFAULT;	// CPU는 접근불가 ,  GPU에서 읽기/쓰기 가능한 버퍼로 생성.

	// 정점 버퍼 생성.
	D3D11_SUBRESOURCE_DATA vbData = {};
	vbData.pSysMem = vertices;	// 버퍼를 생성할때 복사할 데이터의 주소 설정 
	HR_T(hr = m_pDevice->CreateBuffer(&vbDesc, &vbData, &m_pVertexBuffer));


	// 인덱스 리스트
	// index를 그릴때 순열이 중요함 ( 그리는 방향이 같아도 순열이 다르면 회전시 다른 방향이 될 수 있다 )
	UINT indices[] = {
		//Y+
		3,1,0, 2,1,3,
		//Y-
		6,4,5, 7,4,6,
		//X-
		11,9,8, 10,9,11,
		//X+
		14,12,13, 15,12,14,
		//Z-
		19,17,16, 18,17,19,
		//Z+
		22,20,21, 23,20,22
	};

	m_IndexCount = ARRAYSIZE(indices);	// 인덱스 수
	D3D11_BUFFER_DESC idDesc = {};
	idDesc.ByteWidth = sizeof(UINT) * m_IndexCount; // 인덱스 버퍼의 크기(Byte).
	idDesc.CPUAccessFlags = 0;
	idDesc.BindFlags = D3D11_BIND_INDEX_BUFFER; // 인덱스 버퍼로 사용.
	idDesc.MiscFlags = 0;
	idDesc.Usage = D3D11_USAGE_DEFAULT;	// CPU는 접근불가 ,  GPU에서 읽기/쓰기 가능한 버퍼로 생성.


	// 인덱스 버퍼 생성.
	D3D11_SUBRESOURCE_DATA idData = {};
	idData.pSysMem = indices;	// 버퍼를 생성할때 복사할 데이터의 주소 설정 
	HR_T(hr = m_pDevice->CreateBuffer(&idDesc, &idData, &m_pIndexBuffer));


	// 버텍스 버퍼 정보 
	m_VertextBufferStride = sizeof(VertexL); // 버텍스 하나의 크기
	m_VertextBufferOffset = 0;	// 버텍스 시작 주소에서 더할 오프셋 주소

	// 2. Render에서 파이프라인에 바인딩할  버텍스 셰이더 생성
	ID3DBlob* vertexShaderBuffer = nullptr; // 버텍스 세이더 HLSL의 컴파일된 결과(바이트코드)를 담을수 있는 버퍼 객체
	HR_T(CompileShaderFromFile(L"BasicVertexShader.hlsl", "main", "vs_4_0", &vertexShaderBuffer));
	HR_T(m_pDevice->CreateVertexShader(vertexShaderBuffer->GetBufferPointer(), // 필요한 데이터를 복사하며 객체 생성 
		vertexShaderBuffer->GetBufferSize(), NULL, &m_pVertexShader));

	// 3. Render() 에서 파이프라인에 바인딩할 InputLayout 생성 	
	D3D11_INPUT_ELEMENT_DESC layout[] =  // 인풋 레이아웃은 버텍스 쉐이더가 입력받을 데이터의 형식을 지정한다.
	{// SemanticName , SemanticIndex , Format , InputSlot , AlignedByteOffset , InputSlotClass , InstanceDataStepRate		
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "NORMAL",    0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0}
	};
	// 버텍스 셰이더의 Input에 지정된 내용과 같은지 검증하면서 InputLayout을 생성한다.
	HR_T(hr = m_pDevice->CreateInputLayout(layout, ARRAYSIZE(layout),
		vertexShaderBuffer->GetBufferPointer(), vertexShaderBuffer->GetBufferSize(), &m_pInputLayout));

	SAFE_RELEASE(vertexShaderBuffer); // 복사했으니 버퍼는 해제 가능


	// 4. Render에서 파이프라인에 바인딩할 픽셀 셰이더 생성
	ID3DBlob* pixelShaderBuffer = nullptr; // 픽셀 세이더 HLSL의 컴파일된 결과(바이트코드)를 담을수 있는 버퍼 객체
	HR_T(CompileShaderFromFile(L"BasicPixelShader.hlsl", "main", "ps_4_0", &pixelShaderBuffer));
	HR_T(m_pDevice->CreatePixelShader(	  // 필요한 데이터를 복사하며 객체 생성 
		pixelShaderBuffer->GetBufferPointer(),
		pixelShaderBuffer->GetBufferSize(), NULL, &m_pPixelShader));
	//SAFE_RELEASE(pixelShaderBuffer); // 복사했으니 버퍼는 해제 가능

	// 단일색상 픽셸 셰이더 생성
	HR_T(CompileShaderFromFile(L"BasicPixelShader.hlsl", "PSSolid", "ps_4_0", &pixelShaderBuffer));
	HR_T(m_pDevice->CreatePixelShader(pixelShaderBuffer->GetBufferPointer(),
		pixelShaderBuffer->GetBufferSize(), NULL, &m_pPixelShaderSolid));
	SAFE_RELEASE(pixelShaderBuffer);



	D3D11_BUFFER_DESC cbDesc{};
	//상수버퍼는 보통 구조체 1개 크기로 만들기 때문에 아래와 같이 설정
	cbDesc.ByteWidth = (sizeof(Constant) + 15) & ~15;;
	//자주 갱신하는 상수 버퍼 옵션
	cbDesc.Usage = D3D11_USAGE_DYNAMIC;
	//상수버퍼로 쓰겠다는 선언
	cbDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;

	//D3D11_MAPPED_SUBRESOURCE 을 사용할때는 아래
	cbDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	//Map 없이 하는 방식 UpdateSubresource를 사용할때는 아래옵션
	/*cbDesc.CPUAccessFlags = 0;*/
	cbDesc.MiscFlags = 0;
	cbDesc.StructureByteStride = 0;

	D3D11_SUBRESOURCE_DATA idDataP{};
	idData.pSysMem = indices;	// 버퍼를 생성할때 복사할 데이터의 주소 설정 
	HR_T(hr = m_pDevice->CreateBuffer(&cbDesc, nullptr, &m_pConstantBuffer));


	//6. 뎊스&스텐실 뷰 생성
	D3D11_TEXTURE2D_DESC descDepth = {};
	descDepth.Width = m_ClientWidth;
	descDepth.Height = m_ClientHeight;
	descDepth.MipLevels = 1;
	descDepth.ArraySize = 1;
	descDepth.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	descDepth.SampleDesc.Count = 1;
	descDepth.SampleDesc.Quality = 0;
	descDepth.Usage = D3D11_USAGE_DEFAULT;
	descDepth.BindFlags = D3D11_BIND_DEPTH_STENCIL;
	descDepth.CPUAccessFlags = 0;
	descDepth.MiscFlags = 0;

	ID3D11Texture2D* textureDepthStencil = nullptr;
	HR_T(m_pDevice->CreateTexture2D(&descDepth, nullptr, &textureDepthStencil));

	// Create the depth stencil view
	D3D11_DEPTH_STENCIL_VIEW_DESC descDSV = {};
	descDSV.Format = descDepth.Format;
	descDSV.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
	descDSV.Texture2D.MipSlice = 0;
	HR_T(m_pDevice->CreateDepthStencilView(textureDepthStencil, &descDSV, &m_pDepthStencilView));
	m_pDeviceContext->OMSetRenderTargets(1, &m_pRenderTargetView, m_pDepthStencilView);
	SAFE_RELEASE(textureDepthStencil);

	return true;
}

void Light::UninitScene()
{
	SAFE_RELEASE(m_pVertexBuffer);
	SAFE_RELEASE(m_pInputLayout);
	SAFE_RELEASE(m_pVertexShader);
	SAFE_RELEASE(m_pPixelShader);
	SAFE_RELEASE(m_pIndexBuffer);
	SAFE_RELEASE(m_pConstantBuffer);
	SAFE_RELEASE(m_pDepthStencilView);
}

bool Light::InitImGUI()
{
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();

	// 스타일
	ImGui::StyleColorsDark();
	// ImGui::StyleColorsLight();

	// 플랫폼/렌더러 백엔드 연결
	ImGui_ImplWin32_Init(m_hWnd);
	ImGui_ImplDX11_Init(this->m_pDevice, this->m_pDeviceContext);
	return true;
}

void Light::UninitImGUI()
{
	ImGui_ImplDX11_Shutdown();
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext();
}

void Light::RenderGUI()
{
	ImGuiIO& io = ImGui::GetIO(); (void)io;
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;

	ImGui_ImplDX11_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();

	{
		ImGui::Begin("Controller");

		// Camera
		ImGui::PushID(0);
		ImGui::Text("Camera");
		ImGui::DragFloat3("Position", CamPosition, 0.05f, -1000.0f, 1000.0f);

		const float eps_local = 0.001f; // 안전 간격
		ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x * 0.35f);
		ImGui::DragFloat("zNear", &zNear, 0.1f, eps_local, zFar - eps_local);
		ImGui::SameLine();
		ImGui::DragFloat("zFar", &zFar, 0.1f, zNear + eps_local, 1000.0f);
		ImGui::PopItemWidth();

		ImGui::DragFloat("Fov", &CamFovy, 0.05f, 10.0f, 170.0f);

		if (ImGui::Button("Reset##Cam")) {
			CamPosition[0] = 0.0f; CamPosition[1] = 0.0f; CamPosition[2] = -10.0f; // 헤더 기본값
			zNear = 0.1f; zFar = 1000.0f;
			CamFovy = 45.0f;
		}

		// 적용
		eye = XMVectorSet(CamPosition[0], CamPosition[1], CamPosition[2], 1.0f);
		fovy = XMConvertToRadians(CamFovy);

		ImGui::PopID();
		ImGui::NewLine();

		// Parent
		ImGui::PushID(1);
		ImGui::Text("Parent");
		ImGui::DragFloat3("Position", &P_position.x, 0.05f, -1000.0f, 1000.0f);
		ImGui::DragFloat3("Rotation", &P_rotation.x, 0.05f, -1000.0f, 1000.0f);
		ImGui::DragFloat3("Scale", &P_Scale.x, 0.05f, -1000.0f, 1000.0f);
		if (ImGui::Button("Reset##Parent")) {
			P_position = XMFLOAT3(0.0f, 0.0f, 5.0f);
			P_rotation = XMFLOAT3(0.0f, 0.0f, 0.0f);
			P_Scale = XMFLOAT3(1.0f, 1.0f, 1.0f);
		}
		ImGui::PopID();
		ImGui::NewLine();
		// 색깔
		const float eps_color = 0.0001f; // 안전 간격
		ImGui::PushID(2);
		ImGui::Text("Color");
		ImGui::DragFloat3("Box1", LightColor1, 0.01f, 0.0f + eps_color, 1.0f - eps_color);
		ImGui::DragFloat3("Box2", LightColor2, 0.01f, 0.0f + eps_color, 1.0f - eps_color);
		if (ImGui::Button("Reset##Color")) {
			LightColor1[0] = 1.0f; LightColor1[1] = 1.0f; LightColor1[2] = 1.0f;
			LightColor2[0] = 0.0f; LightColor2[1] = 1.0f; LightColor2[2] = 0.0f;
		}
		m_LightColors[0].x = LightColor1[0]; m_LightColors[0].y = LightColor1[1]; m_LightColors[0].z = LightColor1[2];
		m_LightColors[1].x = LightColor2[0]; m_LightColors[1].y = LightColor2[1]; m_LightColors[1].z = LightColor2[2];
		ImGui::PopID();
		ImGui::NewLine();
		//빛벡터
		ImGui::PushID(3);
		ImGui::Text("Direction");
		ImGui::DragFloat3("Box1", LightDir1, 0.01f, -1.0f + eps_local, 1.0f - eps_local);
		ImGui::DragFloat3("Box2", LightDir2, 0.01f, -1.0f + eps_local, 1.0f - eps_local);
		if (ImGui::Button("Reset##Direction")) {
			LightDir1[0] = 1.0f; LightDir1[1] = 0.0f; LightDir1[2] = 0.0f;
			LightDir2[0] = 0.0f; LightDir2[1] = 1.0f; LightDir2[2] = 0.0f;
		}
		m_LightDirsEvaluated[0].x = LightDir1[0]; m_LightDirsEvaluated[0].y = LightDir1[1]; m_LightDirsEvaluated[0].z = LightDir1[2];
		m_LightDirsEvaluated[1].x = LightDir2[0]; m_LightDirsEvaluated[1].y = LightDir2[1]; m_LightDirsEvaluated[1].z = LightDir2[2];

		ImGui::PopID();

		ImGui::End();
	}

	ImGui::Render();
	ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
}

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

LRESULT Light::WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	if (ImGui_ImplWin32_WndProcHandler(hWnd, message, wParam, lParam))
		return 1; // LRESULT 반환

	return __super::WndProc(hWnd, message, wParam, lParam);
}

