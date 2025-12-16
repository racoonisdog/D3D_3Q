#include "PBR.h"
#include "../Common/Helper.h"
#include <directxtk/simplemath.h>
#include <d3dcompiler.h>
#include <Directxtk/DDSTextureLoader.h>
#include "ModelLoader.h"

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


PBR::PBR(HINSTANCE hInstance) : GameApp(hInstance)
{

}


PBR::~PBR()
{
	UninitScene();
	UninitD3D();
}

void PBR::Reset()
{
	m_ZeldaWorld = Matrix::Identity;
	m_CharWorld = Matrix::Identity;
	m_TreeWorld = Matrix::Identity;
	m_CWorld = Matrix::Identity;
	m_Rotation = Vector3(0.0f, 0.0f, 0.0f);
	m_Position = Vector3(0.0f, 0.0f, -10.0f);
}

void PBR::SetMatrix()
{
	XMMATRIX S = XMMatrixScaling(P_BoxManScale.x, P_BoxManScale.y, P_BoxManScale.z);
	XMMATRIX R = XMMatrixRotationRollPitchYaw(P_BoxManrotation.x, P_BoxManrotation.y, P_BoxManrotation.z);
	XMMATRIX T = XMMatrixTranslation(P_BoxManposition.x, P_BoxManposition.y, P_BoxManposition.z);
	m_BoxManWorld = S * R * T;

	S = XMMatrixScaling(P_SkinningTestScale.x, P_SkinningTestScale.y, P_SkinningTestScale.z);
	R = XMMatrixRotationRollPitchYaw(P_SkinningTestrotation.x, P_SkinningTestrotation.y, P_SkinningTestrotation.z);
	T = XMMatrixTranslation(P_SkinningTestposition.x, P_SkinningTestposition.y, P_SkinningTestposition.z);
	m_SkinningTestWorld = S * R * T;


	S = XMMatrixScaling(P_FloorScale.x, P_FloorScale.y, P_FloorScale.z);
	R = XMMatrixRotationRollPitchYaw(P_Floorrotation.x, P_Floorrotation.y, P_Floorrotation.z);
	T = XMMatrixTranslation(P_Floorposition.x, P_Floorposition.y, P_Floorposition.z);
	m_Floor = S * R * T;

	S = XMMatrixScaling(P_CharScale.x, P_CharScale.y, P_CharScale.z);
	R = XMMatrixRotationRollPitchYaw(P_Charrotation.x, P_Charrotation.y, P_Charrotation.z);
	T = XMMatrixTranslation(P_Charposition.x, P_Charposition.y, P_Charposition.z);
	m_Char = S * R * T;

	//view(카메라 수치)
	Vector3 eye = m_CWorld.Translation();
	Vector3 target = m_CWorld.Translation() + m_CWorld.Backward();
	Vector3 up = m_CWorld.Up();
	SanitizeCamera(eye, target, up);
	m_View = XMMatrixLookAtLH(eye, target, up);

	//projection 설정
	m_Proj = XMMatrixPerspectiveFovLH(fovy, aspect, zNear, zFar);

}


void PBR::SanitizeCamera(Vector3& eye, Vector3& target, Vector3& up)
{
	// eye == target 방지
	if ((eye - target).LengthSquared() < 1e-12f) {
		target = eye + Vector3(0, 0, 1); // +Z 기준
	}

	// up 정규화 및 forward와 평행 방지
	Vector3 fwd = (target - eye);
	if (fwd.LengthSquared() < 1e-12f) fwd = Vector3(0, 0, 1);
	fwd.Normalize();

	if (up.LengthSquared() < 1e-12f) up = Vector3(0, 1, 0);
	up.Normalize();

	// up이 forward와 너무 나란하면 살짝 비틀기
	if (fabsf(fwd.Dot(up)) > 0.999f) {
		up = fabsf(fwd.y) < 0.9f ? Vector3(0, 1, 0) : Vector3(1, 0, 0);
	}
}

bool PBR::Initialize(UINT Width, UINT Height)
{
	__super::Initialize(Width, Height);

	if (!InitD3D())
		return false;

	if (!InitImGUI())
		return false;

	if (!InitScene())
		return false;

	if (!InitSkyBox())
		return false;

	if (!InitEffect())
		return false;

	return true;
}

void PBR::Update()
{
	__super::Update();
	SetMatrix();
}

void PBR::Render()
{
	//pass1
	// 사용하지 않는 SRV 해제
	ID3D11ShaderResourceView* nullSRVs[D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT] = {};
	m_pDeviceContext->PSSetShaderResources(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT, nullSRVs);
	m_pDeviceContext->VSSetShaderResources(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT, nullSRVs);
	ID3D11ShaderResourceView* nullSRV[1] = { nullptr };
	m_pDeviceContext->PSSetShaderResources(10, 1, nullSRV);
	m_pDeviceContext->VSSetShaderResources(10, 1, nullSRV);

	//nullptr인 RTV/DSV를 그림자 맵용으로 설정
	ID3D11RenderTargetView* rtvs[1] = { nullptr };
	//RTV를 NULL로 설정하고, DSV는 그림자 맵용 DSV(m_pShadowMapDSV)를 바인딩합니다.
	m_pDeviceContext->OMSetRenderTargets(1, rtvs, m_pShadowMapDSV.Get());

	//깊이 버퍼 초기화
	// m_pShadowMapDSV를 초기화
	m_pDeviceContext->ClearDepthStencilView(m_pShadowMapDSV.Get(), D3D11_CLEAR_DEPTH, 1.0f, 0);

	//뷰포트 설정 (그림자 맵 해상도에 맞게)
	m_pDeviceContext->RSSetViewports(1, &m_ShadowViewport); // m_ShadowViewport은 그림자 맵 크기여야 함
	m_pDeviceContext->OMSetDepthStencilState(m_pDepthStateDefault.Get(), 0);
	m_pDeviceContext->RSSetState(m_pRasterizerShadowBias.Get());
	//셰이더 설정 (깊이 전용 셰이더)
	m_pDeviceContext->PSSetShader(nullptr, nullptr, 0);
	
	m_pDeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST); // 정점을 이어서 그릴 방식 설정.
	// Draw 호출 (빛 시점)
	// DrawSkeletal 함수 호출 시, 그리기 위한 World 행렬을 상수 버퍼에 업데이트 및 빛의 상수버퍼도 업데이트
	//SetHoldLight();
	SetLightVP();
	m_pDeviceContext->UpdateSubresource(m_pShadowConstantBuffer.Get(), 0, nullptr, &ShadowVP, 0, 0);
	m_pDeviceContext->VSSetConstantBuffers(3, 1, m_pShadowConstantBuffer.GetAddressOf());
	
	m_pDeviceContext->IASetInputLayout(m_pBoneInputLayout.Get());
	m_pDeviceContext->VSSetShader(m_pShadowVertexShader.Get(), nullptr, 0);

	PlaySkinningTest->GetFinalMatrices(FinalBoneCS.gBoneMatrices);
	m_pDeviceContext->UpdateSubresource(m_pBoneConstantBuffer.Get(), 0, nullptr, &FinalBoneCS, 0, 0);
	XMStoreFloat4x4(&constandices.world, XMMatrixTranspose(m_SkinningTestWorld));
	m_pDeviceContext->UpdateSubresource(m_pConstantBuffer, 0, nullptr, &constandices, 0, 0);

	m_pDeviceContext->VSSetConstantBuffers(0, 1, &m_pConstantBuffer);


	//시간 업데이트 설정
	float u = GameTimer::m_Instance->DeltaTime();
	//float t = GameTimer::m_Instance->TotalTime();

	if (!stoptime)
	{
		currentTime += (double)u;
	}
	PlaySkinningTest->Update(currentTime);
	PlaySkinningTest->GetFinalMatrices(FinalBoneCS.gBoneMatrices);

	m_pDeviceContext->UpdateSubresource(m_pBoneConstantBuffer.Get(), 0, nullptr, &FinalBoneCS, 0, 0);
	m_pDeviceContext->VSSetConstantBuffers(2, 1, m_pBoneConstantBuffer.GetAddressOf());
	SkinningTest.get()->SetPSValue(false);
	SkinningTest.get()->DrawSkeletal(m_pDeviceContext, m_pMaterialBuffer, m_pBlendON, m_pBlendOFF);




	m_pDeviceContext->IASetInputLayout(m_pInputLayout);
	m_pDeviceContext->VSSetShader(m_NormalShadowVertexShader.Get(), nullptr, 0);


	XMStoreFloat4x4(&constandices.world, XMMatrixTranspose(m_Char));
	m_pDeviceContext->UpdateSubresource(m_pConstantBuffer, 0, nullptr, &constandices, 0, 0);
	Char.get()->SetPSValue(false);
	Char.get()->DrawStatic(m_pDeviceContext, m_pMaterialBuffer, m_pBlendON, m_pBlendOFF);

	XMStoreFloat4x4(&constandices.world, XMMatrixTranspose(m_Floor));
	m_pDeviceContext->UpdateSubresource(m_pConstantBuffer, 0, nullptr, &constandices, 0, 0);
	Box.get()->SetPSValue(false);
	Box.get()->DrawStatic(m_pDeviceContext, m_pMaterialBuffer, m_pBlendON, m_pBlendOFF);

	//Table.get()->SetPSValue(false);
	//XMStoreFloat4x4(&constandices.world, XMMatrixTranspose(m_TableWorld));
	//m_pDeviceContext->UpdateSubresource(m_pConstantBuffer, 0, nullptr, &constandices, 0, 0);
	//Table.get()->DrawSkeletal(m_pDeviceContext, m_pMaterialBuffer, m_pBlendON, m_pBlendOFF);


	//초기화
	m_pDeviceContext->VSSetShader(nullptr, nullptr, 0);
	m_pDeviceContext->IASetInputLayout(nullptr);
	ID3D11Buffer* nullBuffer[1] = { nullptr };
	UINT zero = 0;
	//m_pDeviceContext->IASetVertexBuffers(0, 1, { nullptr }, 0, 0);
	m_pDeviceContext->IASetVertexBuffers(0, 1, nullBuffer, &zero, &zero);
	//m_pDeviceContext->IASetIndexBuffer(nullptr, DXGI_FORMAT_UNKNOWN, 0);
	ID3D11Buffer* nullCB[1] = { nullptr };
	m_pDeviceContext->VSSetConstantBuffers(3, 1, nullCB);
	m_pDeviceContext->VSSetConstantBuffers(2, 1, nullCB);
	//

	//RenderDebugLightFrustum();

	//pass2
	float color[4] = { 0.0f, 0.5f, 0.5f, 1.0f };
	m_pDeviceContext->OMSetDepthStencilState(m_pDepthStateDefault.Get(), 0);

	m_pDeviceContext->RSSetViewports(1, &viewport);

	m_pDeviceContext->OMSetRenderTargets(1, &m_pRenderTargetView, m_pDepthStencilView);
	// 컬러 버퍼(RTV) 초기화
	m_pDeviceContext->ClearRenderTargetView(m_pRenderTargetView, color);
	//깊이 버퍼초기화
	m_pDeviceContext->ClearDepthStencilView(m_pDepthStencilView, D3D11_CLEAR_DEPTH, 1.0f, 0);

	m_pDeviceContext->PSSetShaderResources(10, 1, m_pShadowMapSRV.GetAddressOf());
	

	//상수버퍼 재활용 부분
	constandices.vLightDir = m_LightDirsEvaluated;
	constandices.vLightColor = m_LightColors;
	constandices.clipValue = clipValue;
	XMStoreFloat4x4(&constandices.view, XMMatrixTranspose(m_View));
	XMStoreFloat4x4(&constandices.projection, XMMatrixTranspose(m_Proj));
	//constandices.vOutputColor = XMFLOAT4(0, 0, 0, 0);

	Vector3 tempPos = m_CWorld.Translation();
	constandices.camPos = { tempPos.x, tempPos.y, tempPos.z };

	m_pDeviceContext->UpdateSubresource(m_pConstantBuffer, 0, nullptr, &constandices, 0, 0);
	// Draw계열 함수를 호출하기전에 렌더링 파이프라인에 필수 스테이지 설정을 해야한다.	
	m_pDeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST); // 정점을 이어서 그릴 방식 설정.



	//상수버퍼
	m_pDeviceContext->VSSetConstantBuffers(0, 1, &m_pConstantBuffer);
	m_pDeviceContext->PSSetConstantBuffers(0, 1, &m_pConstantBuffer);
	m_pDeviceContext->VSSetConstantBuffers(3, 1, m_pShadowConstantBuffer.GetAddressOf());
	m_pDeviceContext->PSSetConstantBuffers(3, 1, m_pShadowConstantBuffer.GetAddressOf());

	//리소스
	m_pDeviceContext->PSSetShaderResources(0, 1, M_ptexture.GetAddressOf());
	m_pDeviceContext->PSSetShaderResources(1, 1, M_pnormal.GetAddressOf());
	m_pDeviceContext->PSSetShaderResources(2, 1, M_pspecular.GetAddressOf());

	m_pDeviceContext->PSSetSamplers(0, 1, &m_pSamplerLinear);

	m_pDeviceContext->RSSetState(m_pRasterizerStateDefault.Get());



	// 픽셀 셰이더 바인딩
	m_pDeviceContext->PSSetShader(m_pPBRShader.Get(), nullptr, 0);

	// 버텍스 셰이더 바인딩
	m_pDeviceContext->VSSetShader(m_pBoneVertexShader.Get(), nullptr, 0);
	//입력 레이아웃 바인딩
	m_pDeviceContext->IASetInputLayout(m_pBoneInputLayout.Get());


	m_pDeviceContext->UpdateSubresource(m_pBoneConstantBuffer.Get(), 0, nullptr, &FinalBoneCS, 0, 0);
	m_pDeviceContext->VSSetConstantBuffers(2, 1, m_pBoneConstantBuffer.GetAddressOf());

	//PlaySkinningTest->SetMatrix(OffsetBoneCS.OffsetMatrix, AnimeBoneCS.AnimateMatrix);

	////skele
	XMStoreFloat4x4(&constandices.world, XMMatrixTranspose(m_SkinningTestWorld));
	XMMATRIX W = m_SkinningTestWorld;
	XMMATRIX WInvT = XMMatrixTranspose(XMMatrixInverse(nullptr, W));
	XMStoreFloat4x4(&constandices.worldinverseT, WInvT);
	m_pDeviceContext->UpdateSubresource(m_pConstantBuffer, 0, nullptr, &constandices, 0, 0);
	//PBR 상수버퍼 갱신
	SetPBRValue(SkinningTest);
	m_pDeviceContext->UpdateSubresource(m_pPBRConstantBuffer.Get(), 0, nullptr, &SetPBR, 0, 0);
	m_pDeviceContext->PSSetConstantBuffers(4, 1, m_pPBRConstantBuffer.GetAddressOf());
	SkinningTest.get()->SetPSValue(true);
	////
	SkinningTest.get()->DrawSkeletal(m_pDeviceContext, m_pMaterialBuffer, m_pBlendON, m_pBlendOFF);


	//char draw
	m_pDeviceContext->IASetInputLayout(m_pInputLayout);
	m_pDeviceContext->VSSetShader(m_pVertexShader, nullptr, 0);

	XMStoreFloat4x4(&constandices.world, XMMatrixTranspose(m_Char));
	W = m_Char;
	WInvT = XMMatrixTranspose(XMMatrixInverse(nullptr, W));
	XMStoreFloat4x4(&constandices.worldinverseT, WInvT);
	m_pDeviceContext->UpdateSubresource(m_pConstantBuffer, 0, nullptr, &constandices, 0, 0);
	//PBR 상수버퍼 갱신
	SetPBRValue(Char);
	m_pDeviceContext->UpdateSubresource(m_pPBRConstantBuffer.Get(), 0, nullptr, &SetPBR, 0, 0);
	m_pDeviceContext->PSSetConstantBuffers(4, 1, m_pPBRConstantBuffer.GetAddressOf());
	Char.get()->SetPSValue(true);
	Char.get()->DrawStatic(m_pDeviceContext, m_pMaterialBuffer, m_pBlendON, m_pBlendOFF);



	m_pDeviceContext->IASetInputLayout(m_pInputLayout);
	m_pDeviceContext->VSSetShader(m_pVertexShader, nullptr, 0);
	// m_pVertexShader가 일반 셰이더여야 함. m_pBoneVertexShader와 혼동 금지.
	XMStoreFloat4x4(&constandices.world, XMMatrixTranspose(m_Floor));
	W = m_Floor;
	WInvT = XMMatrixTranspose(XMMatrixInverse(nullptr, W));
	XMStoreFloat4x4(&constandices.worldinverseT, WInvT);
	m_pDeviceContext->UpdateSubresource(m_pConstantBuffer, 0, nullptr, &constandices, 0, 0);
	Box.get()->SetPSValue(true);
	Box.get()->DrawStatic(m_pDeviceContext, m_pMaterialBuffer, m_pBlendON, m_pBlendOFF);
	

	RenderSkyBox();
	RenderDebugLightFrustum();
	RenderGUI();

	// Present the information rendered to the back buffer to the front buffer (the screen)
	m_pSwapChain->Present(0, 0);
}

bool PBR::InitD3D()
{
	// 초기화
	Reset();
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
	//D3D11_VIEWPORT viewport = {};
	viewport.TopLeftX = 0;
	viewport.TopLeftY = 0;
	viewport.Width = (float)m_ClientWidth;
	viewport.Height = (float)m_ClientHeight;
	viewport.MinDepth = 0.0f;
	viewport.MaxDepth = 1.0f;
	m_pDeviceContext->RSSetViewports(1, &viewport);

	///
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

	D3D11_DEPTH_STENCIL_DESC depthDesc = {};
	depthDesc.DepthEnable = TRUE;
	depthDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL; // 깊이 쓰기: 켜짐
	depthDesc.DepthFunc = D3D11_COMPARISON_LESS;           // 깊이 함수: 작으면 통과 (표준)
	depthDesc.StencilEnable = FALSE;

	D3D11_RASTERIZER_DESC rd = {};
	rd.FillMode = D3D11_FILL_SOLID;
	rd.CullMode = D3D11_CULL_BACK;
	rd.FrontCounterClockwise = FALSE;
	rd.DepthClipEnable = TRUE;

	m_pDevice->CreateDepthStencilState(&depthDesc, m_pDepthStateDefault.GetAddressOf());
	m_pDevice->CreateRasterizerState(&rd, m_pRasterizerStateDefault.GetAddressOf());
	///

	ID3D11Texture2D* textureDepthStencil = nullptr;
	HR_T(m_pDevice->CreateTexture2D(&descDepth, nullptr, &textureDepthStencil));

	// Create the depth stencil view
	D3D11_DEPTH_STENCIL_VIEW_DESC descDSV = {};
	descDSV.Format = descDepth.Format;
	descDSV.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
	descDSV.Texture2D.MipSlice = 0;
	HR_T(m_pDevice->CreateDepthStencilView(textureDepthStencil, &descDSV, &m_pDepthStencilView));
	//m_pDeviceContext->OMSetRenderTargets(1, &m_pRenderTargetView, m_pDepthStencilView);
	SAFE_RELEASE(textureDepthStencil);

	//******************
	//그림자 깊이 버퍼 리소스 정의
	m_ShadowViewport.TopLeftX = 0.0f;
	m_ShadowViewport.TopLeftY = 0.0f;
	m_ShadowViewport.Width = 8192.0f;
	m_ShadowViewport.Height = 8192.0f;
	m_ShadowViewport.MinDepth = 0.0f;
	m_ShadowViewport.MaxDepth = 1.0f;

	D3D11_TEXTURE2D_DESC shadowtexDesc = {};
	shadowtexDesc.Width = (UINT)m_ShadowViewport.Width;
	shadowtexDesc.Height = (UINT)m_ShadowViewport.Height;
	shadowtexDesc.MipLevels = 1;
	shadowtexDesc.ArraySize = 1;
	shadowtexDesc.Usage = D3D11_USAGE_DEFAULT;
	shadowtexDesc.Format = DXGI_FORMAT_R32_TYPELESS;
	//앞에꺼는 깊이 기록용도, 뒤에꺼는 셰이더에서 텍스처 슬롯에 설정용도
	shadowtexDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE;
	shadowtexDesc.SampleDesc.Count = 1;
	shadowtexDesc.SampleDesc.Quality = 0;
	HR_T(m_pDevice.Get()->CreateTexture2D(&shadowtexDesc, NULL, m_pShadowMap.GetAddressOf()));

	//깊이 스텐실 상태 정의
	D3D11_DEPTH_STENCIL_VIEW_DESC shadowdescDSV = {};
	shadowdescDSV.Format = DXGI_FORMAT_D32_FLOAT;
	shadowdescDSV.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
	//m_pShadowMapDSV -> 깊이값 기록을 설정하기 위한 객체
	HR_T(m_pDevice.Get()->CreateDepthStencilView(m_pShadowMap.Get(), &shadowdescDSV, m_pShadowMapDSV.GetAddressOf()));

	//깊이 스텐실 뷰 생성
	D3D11_SHADER_RESOURCE_VIEW_DESC shadowsrvDesc = {};
	shadowsrvDesc.Format = DXGI_FORMAT_R32_FLOAT;
	shadowsrvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
	shadowsrvDesc.Texture2D.MipLevels = 1;
	HR_T(m_pDevice.Get()->CreateShaderResourceView(m_pShadowMap.Get(), &shadowsrvDesc, m_pShadowMapSRV.GetAddressOf()));


	//레스터라이저 설정
	D3D11_RASTERIZER_DESC rs{};
	rs.FillMode = D3D11_FILL_SOLID;
	rs.CullMode = D3D11_CULL_BACK;           // 시작은 BACK로. 필요하면 FRONT도 테스트
	rs.FrontCounterClockwise = FALSE;
	rs.DepthClipEnable = TRUE;

	// Bias 설정 (시작값 예시; 상황에 따라 조정)
	rs.DepthBias = 1000;                      // 정수. 0~200 사이부터 시작해서 튜닝
	rs.SlopeScaledDepthBias = 1.0f;          // 0.8 ~ 2.0 근처에서 튜닝
	rs.DepthBiasClamp = 0.0f;                // 보통 0.0 유지
	HR_T(m_pDevice->CreateRasterizerState(&rs, m_pRasterizerShadowBias.GetAddressOf()));
	//******************

	//알파 블랜딩
	D3D11_BLEND_DESC descBlend = {};
	descBlend.RenderTarget[0].BlendEnable = true;						// blend 사용 여부

	descBlend.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
	descBlend.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
	descBlend.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;


	descBlend.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
	descBlend.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO;
	descBlend.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;

	descBlend.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;  // 모든 색 전부 사용

	m_pDevice->CreateBlendState(&descBlend, m_pBlendON.GetAddressOf());

	descBlend.RenderTarget[0].BlendEnable = false;
	m_pDevice->CreateBlendState(&descBlend, m_pBlendOFF.GetAddressOf());

	//객체생성
	D3D11_SAMPLER_DESC sampDesc = {};
	//텍스처를 어떻게 보간해서 읽을지 지정 ( MIN: 멀리서볼때 , MAG: 가까이서 볼때, MIP: 밉맵 사이 전환할때 )
	// 조합에 따라 다르게 씀
	sampDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
	//텍스처 좌표가 0~1 범위를 벗어났을때 어떻게 처리할지 결정
	//U, V, W 는 각각 텍스처의 3차원 축 ( 2D 텍스처라면 U/V만 사용)
	//WRAP: 1.2->0.2로 바꿔서 텍스처를 타일처럼 반복,  CLAMP:1.2 ->1.0(가장자리 색 유지) , MIRROR:1.2->0.8(대칭 반복)
	sampDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
	sampDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
	sampDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
	//샘플러가 비교모드로 동작할때 사용, 보통 섀도우 맵(그림자 계산용)에서 SampleCmp함수로 깊이 비교할때 사용, Never은 비교기능 사용 X -> 일반 텍스처 샘플링 용도로 사용
	sampDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
	//샘플링할 수 있는 최소 LOD 지정, 0 -> 밉맵 레벨 0(가장 고해상도)부터 사용가능
	sampDesc.MinLOD = 0;
	// 샘플링 할 수 있는 최대 LOD 지정  Max-> 제한없음 (가장 낮은 해상도 밉맵까지 다 사용 가능)
	sampDesc.MaxLOD = D3D11_FLOAT32_MAX;

	HR_T(m_pDevice->CreateSamplerState(&sampDesc, &m_pSamplerLinear));

	SetHoldLight();
	SetDebugLightFrustum();

	return true;
}

void PBR::UninitD3D()
{
	SAFE_RELEASE(m_pRenderTargetView);
	SAFE_RELEASE(m_pSwapChain);
	UninitImGUI();
}

void PBR::UninitSkyBox()
{
	S_VertexShader = nullptr;
	S_PixelShader = nullptr;;
	S_InputLayout = nullptr;

	S_VertexBuffer = nullptr;
	S_IndexBuffer = nullptr;
	S_ConstantBuffer = nullptr;

	S_CubeSRV = nullptr;
	S_Sampler = nullptr;

	S_DepthStencilState = nullptr;
	S_RasterizerState = nullptr;
}

bool PBR::InitScene()
{
	HRESULT hr = 0; // 결과값.
	ID3D10Blob* errorMessage = nullptr;	 // 컴파일 에러 메시지가 저장될 버퍼.	


	// 모델 생성




	SkinningTest = std::make_shared<ModelLoader>();
	SkinningTest.get()->SetMergeValue(false);
	if (!SkinningTest->Load(m_hWnd, m_pDevice.Get(), m_pDeviceContext.Get(), "resource\\SkinningTest.fbx", 0))
	{
		MessageBox(m_hWnd, L"FBX couldn't be loaded ", NULL, MB_ICONERROR | MB_OK);
	}
	BoxMan = std::make_unique<ModelLoader>();
	BoxMan.get()->SetMergeValue(false);
	if (!BoxMan->Load(m_hWnd, m_pDevice.Get(), m_pDeviceContext.Get(), "resource\\BoxHuman.fbx", 0))
	{
		MessageBox(m_hWnd, L"FBX couldn't be loaded ", NULL, MB_ICONERROR | MB_OK);
	}


	Box = std::make_unique<ModelLoader>();
	Box.get()->SetMergeValue(false);
	if (!Box->Load(m_hWnd, m_pDevice.Get(), m_pDeviceContext.Get(), "resource\\Ground.fbx", 0))
	{
		MessageBox(m_hWnd, L"FBX couldn't be loaded ", NULL, MB_ICONERROR | MB_OK);
	}


	//Char
	Char = std::make_unique<ModelLoader>();
	Char.get()->SetMergeValue(false);
	if (!Char->Load(m_hWnd, m_pDevice.Get(), m_pDeviceContext.Get(), "resource\\char.fbx", 0))
	{
		MessageBox(m_hWnd, L"FBX couldn't be loaded ", NULL, MB_ICONERROR | MB_OK);
	}
	


	size_t BoxManSize = BoxMan.get()->GetAnimeName()->size();
	size_t SkinningTestSize = SkinningTest.get()->GetAnimeName()->size();

	animelist_BoxHuman.resize(BoxManSize);
	animelist_SkinningTest.resize(SkinningTestSize);

	copy(BoxMan.get()->GetAnimeName()->begin(), BoxMan.get()->GetAnimeName()->end(), animelist_BoxHuman.begin());
	copy(SkinningTest.get()->GetAnimeName()->begin(), SkinningTest.get()->GetAnimeName()->end(), animelist_SkinningTest.begin());

	PlayBoxHuman = new AnimationController();
	PlaySkinningTest = new AnimationController();

	PlayBoxHuman->Initialize(BoxMan.get()->GetSkeletonInfo());
	PlayBoxHuman->SetClipTable(&BoxMan->Getanimelist());

	PlaySkinningTest->Initialize(SkinningTest.get()->GetSkeletonInfo());
	PlaySkinningTest->SetClipTable(&SkinningTest->Getanimelist());


	//tempplayname1 = string("MasterAnimation");
	tempplayname1 = animelist_BoxHuman[0];

	tempplayname2 = animelist_SkinningTest[0];

	PlaySkinningTest->SetClipByName(tempplayname2);

	PlayBoxHuman->SetClipByName(tempplayname1);

	DurationAnime = SkinningTest.get()->GetAnimation(tempplayname2)->GetDurationTicks();
	TicksPerSecond = SkinningTest.get()->GetAnimation(tempplayname2)->GetTicksPerSecond();

	DurationAnime2 = BoxMan.get()->GetAnimation(tempplayname1)->GetDurationTicks();
	TicksPerSecond2 = BoxMan.get()->GetAnimation(tempplayname1)->GetTicksPerSecond();

	D3D11_BUFFER_DESC cbDesc{};
	//상수버퍼는 보통 구조체 1개 크기로 만들기 때문에 아래와 같이 설정
	cbDesc.ByteWidth = (sizeof(Constant) + 15) & ~15;;
	//자주 갱신하는 상수 버퍼 옵션 //map 사용시 아래
	//cbDesc.Usage = D3D11_USAGE_DYNAMIC;
	//UpdateSubresource 사용시 DEFAULT옵션
	cbDesc.Usage = D3D11_USAGE_DEFAULT;
	//상수버퍼로 쓰겠다는 선언
	cbDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;

	//D3D11_MAPPED_SUBRESOURCE 을 사용할때는 아래
	//cbDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	//Map 없이 하는 방식 UpdateSubresource를 사용할때는 아래옵션
	cbDesc.CPUAccessFlags = 0;
	cbDesc.MiscFlags = 0;
	cbDesc.StructureByteStride = 0;
	HR_T(hr = m_pDevice->CreateBuffer(&cbDesc, nullptr, &m_pConstantBuffer));

	// material buffer
	cbDesc.ByteWidth = (sizeof(Material) + 15) & ~15;;
	HR_T(m_pDevice->CreateBuffer(&cbDesc, nullptr, m_pMaterialBuffer.GetAddressOf()));


	//본 상수버퍼 설정
	cbDesc.ByteWidth = (sizeof(FinalBoneMatrix) + 15) & ~15;;
	HR_T(hr = m_pDevice->CreateBuffer(&cbDesc, nullptr, m_pBoneConstantBuffer.GetAddressOf()));

	cbDesc.ByteWidth = (sizeof(TransformVP) + 15) & ~15;;
	HR_T(hr = m_pDevice->CreateBuffer(&cbDesc, nullptr, m_pShadowConstantBuffer.GetAddressOf()));


	cbDesc.ByteWidth = (sizeof(PBRValue) + 15) & ~15;;
	HR_T(hr = m_pDevice->CreateBuffer(&cbDesc, nullptr, m_pPBRConstantBuffer.GetAddressOf()));

	return true;
}

bool PBR::InitSkyBox()
{
	HRESULT hr = 0; // 결과값.
	ID3D10Blob* errorMessage = nullptr;	 // 컴파일 에러 메시지가 저장될 버퍼.	

	//정점하나에 법선 3개를 담을 수 없기 때문에 중복된 정점 버퍼도 정의해줘야함
	VertexSky vertices[] =
	{
		//Noarmal Y+
		VertexSky(Vector3(-1.0f, 1.0f, -1.0f)),
		VertexSky(Vector3(1.0f, 1.0f, -1.0f)),
		VertexSky(Vector3(1.0f, 1.0f, 1.0f)),
		VertexSky(Vector3(-1.0f, 1.0f, 1.0f)),
		//Normal Y-
		VertexSky(Vector3(-1.0f, -1.0f, -1.0f)),
		VertexSky(Vector3(1.0f, -1.0f, -1.0f)),
		VertexSky(Vector3(1.0f, -1.0f, 1.0f)),
		VertexSky(Vector3(-1.0f, -1.0f, 1.0f)),
		//Normal X-
		VertexSky(Vector3(-1.0f, -1.0f, 1.0f)),
		VertexSky(Vector3(-1.0f, -1.0f, -1.0f)),
		VertexSky(Vector3(-1.0f, 1.0f, -1.0f)),
		VertexSky(Vector3(-1.0f, 1.0f, 1.0f)),
		//Normal X+
		VertexSky(Vector3(1.0f, -1.0f, 1.0f)),
		VertexSky(Vector3(1.0f, -1.0f, -1.0f)),
		VertexSky(Vector3(1.0f, 1.0f, -1.0f)),
		VertexSky(Vector3(1.0f, 1.0f, 1.0f)),
		//Normal Z-
		VertexSky(Vector3(-1.0f, -1.0f, -1.0f)),
		VertexSky(Vector3(1.0f, -1.0f, -1.0f)),
		VertexSky(Vector3(1.0f, 1.0f, -1.0f)),
		VertexSky(Vector3(-1.0f, 1.0f, -1.0f)),
		//Normal Z+
		VertexSky(Vector3(-1.0f, -1.0f, 1.0f)),
		VertexSky(Vector3(1.0f, -1.0f, 1.0f)),
		VertexSky(Vector3(1.0f, 1.0f, 1.0f)),
		VertexSky(Vector3(-1.0f, 1.0f, 1.0f))
	};

	D3D11_BUFFER_DESC vbDesc = {};
	S_VertexCount = ARRAYSIZE(vertices);	// 정점의 수
	vbDesc.ByteWidth = sizeof(VertexSky) * S_VertexCount; // 버텍스 버퍼의 크기(Byte).
	vbDesc.CPUAccessFlags = 0;
	vbDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER; // 정점 버퍼로 사용.
	vbDesc.MiscFlags = 0;
	vbDesc.Usage = D3D11_USAGE_DEFAULT;	// CPU는 접근불가 ,  GPU에서 읽기/쓰기 가능한 버퍼로 생성.

	// 정점 버퍼 생성.
	D3D11_SUBRESOURCE_DATA vbData = {};
	vbData.pSysMem = vertices;	// 버퍼를 생성할때 복사할 데이터의 주소 설정 
	HR_T(hr = m_pDevice->CreateBuffer(&vbDesc, &vbData, S_VertexBuffer.GetAddressOf()));

	// 인덱스 리스트 ( 반시계 방향 )
	UINT indices[] = {
		// Y+
		1,0,3, 1,3,2,
		// Y-
		4,5,6, 4,6,7,
		// X-
		9,8,11, 9,11,10,
		// X+
		12,13,14, 12,14,15,
		// Z-
		17,16,19, 17,19,18,
		// Z+
		20,21,22, 20,22,23
	};



	S_IndexCount = ARRAYSIZE(indices);	// 인덱스 수
	D3D11_BUFFER_DESC idDesc = {};
	idDesc.ByteWidth = sizeof(UINT) * S_IndexCount; // 인덱스 버퍼의 크기(Byte).
	idDesc.CPUAccessFlags = 0;
	idDesc.BindFlags = D3D11_BIND_INDEX_BUFFER; // 인덱스 버퍼로 사용.
	idDesc.MiscFlags = 0;
	idDesc.Usage = D3D11_USAGE_DEFAULT;	// CPU는 접근불가 ,  GPU에서 읽기/쓰기 가능한 버퍼로 생성.


	// 인덱스 버퍼 생성.
	D3D11_SUBRESOURCE_DATA idData = {};
	idData.pSysMem = indices;	// 버퍼를 생성할때 복사할 데이터의 주소 설정 
	HR_T(hr = m_pDevice->CreateBuffer(&idDesc, &idData, S_IndexBuffer.GetAddressOf()));


	// 버텍스 버퍼 정보 
	S_VertexBufferStride = sizeof(VertexSky); // 버텍스 하나의 크기
	S_VertexBufferOffset = 0;	// 버텍스 시작 주소에서 더할 오프셋 주소


	ComPtr<ID3DBlob> vertexShaderBuffer = nullptr;
	HR_T(CompileShaderFromFile(L"SkyVertexShader.hlsl", "main", "vs_4_0", &vertexShaderBuffer));
	HR_T(m_pDevice->CreateVertexShader(vertexShaderBuffer->GetBufferPointer(), // 필요한 데이터를 복사하며 객체 생성 
		vertexShaderBuffer->GetBufferSize(), NULL, S_VertexShader.GetAddressOf()));

	// 3. Render() 에서 파이프라인에 바인딩할 InputLayout 생성 	
	D3D11_INPUT_ELEMENT_DESC layout[] =  // 인풋 레이아웃은 버텍스 쉐이더가 입력받을 데이터의 형식을 지정한다.
	{// SemanticName , SemanticIndex , Format , InputSlot , AlignedByteOffset , InputSlotClass , InstanceDataStepRate		
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
	};
	// 버텍스 셰이더의 Input에 지정된 내용과 같은지 검증하면서 InputLayout을 생성한다.
	HR_T(hr = m_pDevice->CreateInputLayout(layout, ARRAYSIZE(layout),
		vertexShaderBuffer->GetBufferPointer(), vertexShaderBuffer->GetBufferSize(), S_InputLayout.GetAddressOf()));

	HR_T(CreateDDSTextureFromFile(m_pDevice.Get(), L"../resource/box/skybox.dds", nullptr, S_CubeSRV.GetAddressOf()));

	//객체생성
	D3D11_SAMPLER_DESC sampDesc = {};
	//텍스처를 어떻게 보간해서 읽을지 지정 ( MIN: 멀리서볼때 , MAG: 가까이서 볼때, MIP: 밉맵 사이 전환할때 )
	// 조합에 따라 다르게 씀
	sampDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
	//텍스처 좌표가 0~1 범위를 벗어났을때 어떻게 처리할지 결정
	//U, V, W 는 각각 텍스처의 3차원 축 ( 2D 텍스처라면 U/V만 사용)
	//WRAP: 1.2->0.2로 바꿔서 텍스처를 타일처럼 반복,  CLAMP:1.2 ->1.0(가장자리 색 유지) , MIRROR:1.2->0.8(대칭 반복)
	sampDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
	sampDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
	sampDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
	//샘플러가 비교모드로 동작할때 사용, 보통 섀도우 맵(그림자 계산용)에서 SampleCmp함수로 깊이 비교할때 사용, Never은 비교기능 사용 X -> 일반 텍스처 샘플링 용도로 사용
	sampDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
	//샘플링할 수 있는 최소 LOD 지정, 0 -> 밉맵 레벨 0(가장 고해상도)부터 사용가능
	sampDesc.MinLOD = 0;
	// 샘플링 할 수 있는 최대 LOD 지정  Max-> 제한없음 (가장 낮은 해상도 밉맵까지 다 사용 가능)
	sampDesc.MaxLOD = D3D11_FLOAT32_MAX;

	HR_T(m_pDevice->CreateSamplerState(&sampDesc, S_Sampler.GetAddressOf()));

	// 4. Render에서 파이프라인에 바인딩할 픽셀 셰이더 생성
	ComPtr<ID3DBlob> pixelShaderBuffer = nullptr; // 픽셀 세이더 HLSL의 컴파일된 결과(바이트코드)를 담을수 있는 버퍼 객체
	HR_T(CompileShaderFromFile(L"SkyPixelShader.hlsl", "main", "ps_4_0", &pixelShaderBuffer));
	HR_T(m_pDevice->CreatePixelShader(	  // 필요한 데이터를 복사하며 객체 생성 
		pixelShaderBuffer->GetBufferPointer(),
		pixelShaderBuffer->GetBufferSize(), NULL, S_PixelShader.GetAddressOf()));


	D3D11_BUFFER_DESC cbDesc{};
	//상수버퍼는 보통 구조체 1개 크기로 만들기 때문에 아래와 같이 설정
	cbDesc.ByteWidth = (sizeof(SkyConstant) + 15) & ~15;;
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
	HR_T(hr = m_pDevice->CreateBuffer(&cbDesc, nullptr, S_ConstantBuffer.GetAddressOf()));


	//6. 뎊스&스텐실 뷰 생성
	D3D11_DEPTH_STENCIL_DESC descDepth = {};
	descDepth.DepthEnable = TRUE;
	descDepth.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
	descDepth.DepthFunc = D3D11_COMPARISON_LESS_EQUAL; // 또는 LESS
	descDepth.StencilEnable = FALSE;
	HR_T(m_pDevice->CreateDepthStencilState(&descDepth, S_DepthStencilState.GetAddressOf()));


	D3D11_RASTERIZER_DESC rd = {};
	rd.FillMode = D3D11_FILL_SOLID;
	rd.CullMode = D3D11_CULL_FRONT;
	rd.DepthClipEnable = TRUE;

	HR_T(m_pDevice->CreateRasterizerState(&rd, S_RasterizerState.GetAddressOf()));

	return true;
}

void PBR::RenderSkyBox()
{
	// 상수버퍼 업데이트 (world/view/projection)
	SkyConstant scb{};
	XMStoreFloat4x4(&scb.world, XMMatrixTranspose(XMMatrixIdentity()));
	XMMATRIX viewNoTrans = m_View;
	viewNoTrans.r[3] = XMVectorSet(0.0f, 0.0f, 0.0f, 1.0f);
	XMStoreFloat4x4(&scb.view, XMMatrixTranspose(viewNoTrans));
	XMStoreFloat4x4(&scb.projection, XMMatrixTranspose(m_Proj));

	D3D11_MAPPED_SUBRESOURCE mapped{};
	HR_T(m_pDeviceContext->Map(S_ConstantBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped));
	memcpy(mapped.pData, &scb, sizeof(scb));
	m_pDeviceContext->Unmap(S_ConstantBuffer.Get(), 0);


	// IA
	UINT stride = sizeof(VertexSky), offset = 0;
	m_pDeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	m_pDeviceContext->IASetInputLayout(S_InputLayout.Get());
	m_pDeviceContext->IASetVertexBuffers(0, 1, S_VertexBuffer.GetAddressOf(), &stride, &offset);
	m_pDeviceContext->IASetIndexBuffer(S_IndexBuffer.Get(), DXGI_FORMAT_R32_UINT, 0);

	// 셰이더/상수/자원
	m_pDeviceContext->VSSetShader(S_VertexShader.Get(), nullptr, 0);
	m_pDeviceContext->PSSetShader(S_PixelShader.Get(), nullptr, 0);
	m_pDeviceContext->VSSetConstantBuffers(0, 1, S_ConstantBuffer.GetAddressOf());
	m_pDeviceContext->PSSetConstantBuffers(0, 1, S_ConstantBuffer.GetAddressOf());
	m_pDeviceContext->PSSetShaderResources(0, 1, S_CubeSRV.GetAddressOf());
	m_pDeviceContext->PSSetSamplers(0, 1, S_Sampler.GetAddressOf());


	// 상태
	m_pDeviceContext->OMSetDepthStencilState(S_DepthStencilState.Get(), 0);
	// BlendOff
	m_pDeviceContext->OMSetBlendState(m_pBlendOFF.Get(), nullptr, 0xFFFFFFFF);
	m_pDeviceContext->RSSetState(S_RasterizerState.Get());

	// 드로우
	m_pDeviceContext->DrawIndexed(S_IndexCount, 0, 0);

	// 초기화
	m_pDeviceContext->OMSetDepthStencilState(m_pDepthStateDefault.Get(), 0);
	//m_pDeviceContext->OMSetDepthStencilState(nullptr, 0);
}

void PBR::UninitScene()
{
	SAFE_RELEASE(m_pInputLayout);
	SAFE_RELEASE(m_pVertexShader);
	SAFE_RELEASE(m_pPixelShader);
	SAFE_RELEASE(m_pConstantBuffer);
	SAFE_RELEASE(m_pDepthStencilView);
}

bool PBR::InitImGUI()
{
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();

	// 스타일
	ImGui::StyleColorsDark();
	// ImGui::StyleColorsLight();

	// 플랫폼/렌더러 백엔드 연결
	ImGui_ImplWin32_Init(m_hWnd);
	ImGui_ImplDX11_Init(this->m_pDevice.Get(), this->m_pDeviceContext.Get());
	return true;
}

void PBR::UninitImGUI()
{
	ImGui_ImplDX11_Shutdown();
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext();
}

void PBR::RenderGUI()
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
		ImGui::DragFloat("CamSpeed", &speed, 0.1f, 2.0f, 5000.0f);
		ImGui::Checkbox("LocalTrue", &m_RotateAboutLocal);
		ImGui::DragFloat3("Position", CamPosition, 0.05f, -1000.0f, 1000.0f);
		ImGui::DragFloat3("Rotation", CamRotation, 0.05f, -1000.0f, 1000.0f);

		const float eps_local = 0.001f; // 안전 간격
		ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x * 0.35f);
		ImGui::DragFloat("zNear", &zNear, 0.1f, eps_local, zFar - eps_local);
		ImGui::SameLine();
		ImGui::DragFloat("zFar", &zFar, 0.1f, zNear + eps_local, 1000.0f);
		ImGui::PopItemWidth();

		ImGui::DragFloat("Fov", &CamFovy, 0.05f, 10.0f, 170.0f);


		float dt = GameTimer::m_Instance->DeltaTime();
		//고정 수치가 아닌 카메라 월드행렬 기준 정면으로
		Vector3 Camfwd = m_CWorld.Backward(); Camfwd.Normalize();
		Vector3 Camright = m_CWorld.Right(); Camright.Normalize();
		Vector3 Camup = m_CWorld.Up(); Camup.Normalize();

		Vector3 dpos = Vector3::Zero;
		if (ImGui::IsKeyDown(ImGuiKey_W)) dpos += Camfwd;
		if (ImGui::IsKeyDown(ImGuiKey_S)) dpos -= Camfwd;
		if (ImGui::IsKeyDown(ImGuiKey_D)) dpos += Camright;
		if (ImGui::IsKeyDown(ImGuiKey_A)) dpos -= Camright;
		if (ImGui::IsKeyDown(ImGuiKey_E)) dpos += Camup;
		if (ImGui::IsKeyDown(ImGuiKey_Q)) dpos -= Camup;

		if (dpos.LengthSquared() > 0) dpos.Normalize();
		m_Position += dpos * speed * dt;

		// UI 표시용 배열 동기화(선택)
		CamPosition[0] = m_Position.x;
		CamPosition[1] = m_Position.y;
		CamPosition[2] = m_Position.z;

		Vector3 uiRot = { CamRotation[0], CamRotation[1], CamRotation[2] }; // 라디안
		Vector3 d = uiRot - m_UIRotPrev;
		//이전값 갱신
		m_UIRotPrev = uiRot;
		Vector3 dRad = {
		XMConvertToRadians(d.x),
		XMConvertToRadians(d.y),
		XMConvertToRadians(d.z)
		};

		Quaternion qDelta = Quaternion::CreateFromYawPitchRoll(dRad.y, dRad.x, dRad.z);

		if (m_RotateAboutLocal) {
			// 로컬축 회전: 기존 방향 뒤에 곱함
			m_COri = m_COri * qDelta;
		}
		else {
			// 월드축 회전: 앞에 곱
			m_COri = qDelta * m_COri;
		}


		//초기화
		if (ImGui::Button("Reset##Cam")) {
			CamPosition[0] = 0.0f; CamPosition[1] = 0.0f; CamPosition[2] = -10.0f; // 헤더 기본값
			CamRotation[0] = 0.0f; CamRotation[1] = 0.0f; CamRotation[2] = 0.0f;
			m_UIRotPrev = Vector3(0.0f, 0.0f, 0.0f);
			m_COri = Quaternion::Identity;
			zNear = 0.1f; zFar = 1000.0f;
			CamFovy = 45.0f;
		}



		// 적용
		m_Position = Vector3{ CamPosition[0], CamPosition[1],CamPosition[2] };
		fovy = XMConvertToRadians(CamFovy);
		m_CWorld = Matrix::CreateFromQuaternion(m_COri) * Matrix::CreateTranslation(m_Position);


		ImGui::PopID();
		ImGui::NewLine();

		// 색깔
		const float eps_color = 0.0001f; // 안전 간격
		//빛벡터
		ImGui::PushID(1);
		ImGui::Text("Direction");
		ImGui::DragFloat3("Box1", LightDir1, 0.01f, -1.0f + eps_local, 1.0f - eps_local);
		if (ImGui::Button("Reset##Direction")) {
			LightDir1[0] = 0.0f; LightDir1[1] = -1.0f; LightDir1[2] = 1.0f;
		}
		m_LightDirsEvaluated.x = LightDir1[0]; m_LightDirsEvaluated.y = LightDir1[1]; m_LightDirsEvaluated.z = LightDir1[2];


		ImGui::PopID();
		ImGui::NewLine();
		ImGui::PushID(2);

		ImGui::DragFloat("CamSpeed", &speed, 0.1f, eps_local, 1000.0f - eps_local);
		ImGui::DragFloat("ClipValue", &clipValue, 0.001f, eps_local, 0.5f - eps_local);

		double min_val = 0.0;

		ImGui::PopID();
		ImGui::NewLine();
		ImGui::PushID(3);
		ImGui::DragScalar("Time", ImGuiDataType_Double, &currentTime, 0.01f, &min_val, &DurationAnime);
		ImGui::Checkbox("stop", &stoptime);


		ImGui::PopID();
		ImGui::NewLine();

		// skiingtest
		ImGui::PushID(4);
		ImGui::Text("SkinningModel");
		ImGui::DragFloat3("Position", &P_SkinningTestposition.x, 0.05f, -1000.0f, 1000.0f);
		ImGui::DragFloat3("Rotation", &P_SkinningTestrotation.x, 0.05f, -1000.0f, 1000.0f);
		ImGui::DragFloat3("Scale", &P_SkinningTestScale.x, 0.001f, -1000.0f, 1000.0f);
		if (ImGui::Button("Reset##Skinning")) {
			P_SkinningTestposition = XMFLOAT3(0.0f, 0.0f, 0.0f);
			P_SkinningTestrotation = XMFLOAT3(0.0f, 0.0f, 0.0f);
			P_SkinningTestScale = XMFLOAT3(0.01f, 0.01f, 0.01f);
		}

		ImGui::PopID();
		ImGui::NewLine();

		// char
		ImGui::PushID(5);
		ImGui::Text("CharModel");
		ImGui::DragFloat3("Position", &P_Charposition.x, 0.05f, -1000.0f, 1000.0f);
		ImGui::DragFloat3("Rotation", &P_Charrotation.x, 0.05f, -1000.0f, 1000.0f);
		ImGui::DragFloat3("Scale", &P_CharScale.x, 0.001f, -1000.0f, 1000.0f);
		if (ImGui::Button("Reset##Skinning")) {
			P_Charposition = XMFLOAT3(1.0f, 1.0f, 0.0f);
			P_Charrotation = XMFLOAT3(0.0f, 0.0f, 0.0f);
			P_CharScale = XMFLOAT3(0.01f, 0.01f, 0.01f);
		}

		ImGui::PopID();
		ImGui::NewLine();

		ImGui::PushID(6);
		ImGui::Text("SetLightProjection");
		ImGui::DragFloat("boxsize", &boxSize, 0.05f, 0.0f, 1000.0f);
		ImGui::DragFloat("depthNear", &depthNear, 0.05f, 0.0f, 500.0f);
		ImGui::DragFloat("depthFar", &depthFar, 0.05f, 0.0f, 2000.0f);
		if (ImGui::Button("Reset##Projection"))
		{
			boxSize = 80.0f;
			depthNear = 10.0f;
			depthFar = 200.0f;
		}

		ImGui::PopID();
		ImGui::NewLine();

		ImGui::PushID(7);
		ImGui::Text("Box");
		ImGui::DragFloat3("BoxPosition", &P_Floorposition.x, 0.05f, -500.0f, 500.0f);
		if (ImGui::Button("Reset##Position"))
		{
			P_Floorposition.x = 0.0f;
			P_Floorposition.y = -2.0f;
			P_Floorposition.z = 0.0f;
		}

		ImGui::PopID();
		ImGui::NewLine();

		ImGui::PushID(8);
		ImGui::Text("PBRValue");
		ImGui::Checkbox("UseModelValue", &useModelValues);
		ImGui::DragFloat("Metallic", &m_metalic, 0.005f, 0.0f, 1.0f);
		ImGui::DragFloat("Roughness", &m_roughness, 0.005f, 0.0f, 1.0f);
		ImGui::ColorEdit3("MetalColor", Metalcolor);
		if (ImGui::Button("Reset##PBR"))
		{
			m_metalic = 0.0f;
			m_roughness = 1.0f;
			Metalcolor[0] = 1.0f;
			Metalcolor[1] = 1.0f;
			Metalcolor[2] = 1.0f;
		}


		{
			ImGui::Begin("Shadow Map Viewer"); // <--- Controller 블록 외부에 새 창 시작

			ImTextureID shadowMapID = (ImTextureID)m_pShadowMapSRV.Get();

			ImVec2 imageSize = ImVec2(256, 256);

			// ImGui::Image 호출
			ImGui::Image(
				shadowMapID,
				imageSize,
				ImVec2(0, 0),
				ImVec2(1, 1),
				ImVec4(1, 1, 1, 1),
				ImVec4(0, 0, 0, 0)
			);

			ImGui::Text("Shadow Map Resolution: %.0fx%.0f", imageSize.x, imageSize.y);

			ImGui::End(); // Shadow Map Viewer 창 종료
		}

		{
			ImGui::Begin("Matrix Debugger");

			// ----------------------------------------------------
			// 1. Shadow View 행렬
			// ----------------------------------------------------
			ImGui::Text("--- Shadow View Matrix ---");
			// XMFLOAT4X4 타입의 debugView를 테이블로 출력하는 함수 호출
			DebugMatrix4x4("ShadowView", ShadowVP.ShadowView);

			ImGui::Separator();

			// ----------------------------------------------------
			// 2. Shadow Projection 행렬
			// ----------------------------------------------------
			ImGui::Text("--- Shadow Projection Matrix ---");
			DebugMatrix4x4("ShadowProj", ShadowVP.ShadowProjection);

			ImGui::Separator();

			// ----------------------------------------------------
			// 3. Main Camera View 행렬 (예시: m_CView는 XMFLOAT4X4 타입이라고 가정)
			// ----------------------------------------------------
			ImGui::Text("--- Camera View Matrix ---");
			// 실제 카메라 뷰 행렬 변수를 사용하세요. (예: m_CView)
			Matrix viewMatrix = m_CWorld.Invert();
			DebugMatrix4x4("CameraView", viewMatrix);

			ImGui::Separator();

			// ----------------------------------------------------
			// 4. Main Camera Projection 행렬 (예시: m_CProjection은 XMFLOAT4X4 타입이라고 가정)
			// ----------------------------------------------------
			ImGui::Text("--- Camera Projection Matrix ---");
			// 실제 카메라 프로젝션 행렬 변수를 사용하세요. (예: m_CProjection)
			Matrix projMatrix = Matrix::CreatePerspectiveFieldOfView(fovy, aspect, zNear, zFar);
			DebugMatrix4x4("CameraProj", projMatrix);

			ImGui::End();
		}

		ImGui::PopID();

		ImGui::End();
	}

	ImGui::Render();
	ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
}

bool PBR::InitEffect()
{
	// 2. 파이프라인에 바인딩할 InputLayout 생성
	D3D11_INPUT_ELEMENT_DESC layout[] =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "TANGENT", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "BINORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 }
	};

	D3D11_INPUT_ELEMENT_DESC g_BoneLayout[] =
	{
		// 기존 정점 정보
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "TANGENT", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "BINORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },

		// Bone Index
		{ "BLENDINDICES", 0, DXGI_FORMAT_R32G32B32A32_UINT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		// Bone Weight (가중치)
		{ "BLENDWEIGHT", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 }
	};

	ComPtr<ID3DBlob> vertexShaderBuffer = nullptr;
	HR_T(CompileShaderFromFile(L"BasicVertexShader.hlsl", "main", "vs_4_0", vertexShaderBuffer.GetAddressOf()));
	HR_T(m_pDevice->CreateInputLayout(layout, ARRAYSIZE(layout), vertexShaderBuffer->GetBufferPointer(), vertexShaderBuffer->GetBufferSize(), &m_pInputLayout));

	//HR_T(hr = m_pDevice->CreateInputLayout(layout, ARRAYSIZE(layout),
	//	vertexShaderBuffer->GetBufferPointer(), vertexShaderBuffer->GetBufferSize(), &m_pInputLayout));

	// 3. 파이프 라인에 바인딩할 정점 셰이더 생성
	HR_T(m_pDevice->CreateVertexShader(vertexShaderBuffer->GetBufferPointer(), vertexShaderBuffer->GetBufferSize(), NULL, &m_pVertexShader));

	vertexShaderBuffer.Reset();
	HR_T(CompileShaderFromFile(L"BasicVertexShader.hlsl", "Shadowmain", "vs_4_0", vertexShaderBuffer.GetAddressOf()));
	HR_T(m_pDevice->CreateInputLayout(layout, ARRAYSIZE(layout), vertexShaderBuffer->GetBufferPointer(), vertexShaderBuffer->GetBufferSize(), &m_pInputLayout));
	HR_T(m_pDevice->CreateVertexShader(vertexShaderBuffer->GetBufferPointer(), vertexShaderBuffer->GetBufferSize(), NULL, &m_NormalShadowVertexShader));

	//본 버텍스 셰이더
	vertexShaderBuffer.Reset();
	HR_T(CompileShaderFromFile(L"BasicVertexShader.hlsl", "BoneMain", "vs_5_0", vertexShaderBuffer.GetAddressOf()));
	HR_T(m_pDevice->CreateInputLayout(g_BoneLayout, ARRAYSIZE(g_BoneLayout), vertexShaderBuffer->GetBufferPointer(), vertexShaderBuffer->GetBufferSize(), &m_pBoneInputLayout));
	HR_T(m_pDevice->CreateVertexShader(vertexShaderBuffer->GetBufferPointer(), vertexShaderBuffer->GetBufferSize(), NULL, &m_pBoneVertexShader));

	//그림자 셰이더
	vertexShaderBuffer.Reset();
	HR_T(CompileShaderFromFile(L"BasicVertexShader.hlsl", "Shadow", "vs_5_0", vertexShaderBuffer.GetAddressOf()));
	HR_T(m_pDevice->CreateInputLayout(g_BoneLayout, ARRAYSIZE(g_BoneLayout), vertexShaderBuffer->GetBufferPointer(), vertexShaderBuffer->GetBufferSize(), &m_pBoneInputLayout));
	HR_T(m_pDevice->CreateVertexShader(vertexShaderBuffer->GetBufferPointer(), vertexShaderBuffer->GetBufferSize(), NULL, &m_pShadowVertexShader));


	// 5. 파이프라인에 바인딩할 픽셀 셰이더 생성
	ComPtr<ID3DBlob> pixelShaderBuffer = nullptr;
	HR_T(CompileShaderFromFile(L"BasicPixelShader.hlsl", "main", "ps_4_0", pixelShaderBuffer.GetAddressOf()));
	HR_T(m_pDevice->CreatePixelShader(pixelShaderBuffer->GetBufferPointer(), pixelShaderBuffer->GetBufferSize(), NULL, &m_pPixelShader));

	pixelShaderBuffer.Reset();
	HR_T(CompileShaderFromFile(L"PBRPixelShader.hlsl", "main", "ps_4_0", pixelShaderBuffer.GetAddressOf()));
	HR_T(m_pDevice->CreatePixelShader(pixelShaderBuffer->GetBufferPointer(), pixelShaderBuffer->GetBufferSize(), NULL, m_pPBRShader.GetAddressOf()));
	

	return true;
}

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

LRESULT PBR::WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	if (ImGui_ImplWin32_WndProcHandler(hWnd, message, wParam, lParam))
		return 1; // LRESULT 반환

	return __super::WndProc(hWnd, message, wParam, lParam);
}

void PBR::SetRenderSort()
{
	//for (auto& it : renderlist)
	//{
	//	if (it.second.get() == m_pZelda.get()) { it.first = zeldaDistance; }
	//	else if (it.second.get() == m_pCharacter.get()) { it.first = charDistance; }
	//	else if (it.second.get() == m_pTree.get()) { it.first = treeDistance; }

	//}
	//sort(renderlist.begin(), renderlist.end(), [](const pair<float, shared_ptr<ModelLoader>>& a, pair<float, shared_ptr<ModelLoader>>& b)
	//	{
	//		if (a.first != b.first)
	//		{
	//			return a.first > b.first;
	//		}

	//		return a.second.get()->Getweight() < b.second.get()->Getweight();
	//	});
}

void PBR::SetLightVP()
{
	//기존에 쓰던 lightDir을 그대로 가져온다.
	XMFLOAT3 lightDirWorld(LightDir1[0], LightDir1[1], LightDir1[2]);

	// 혹시 모를 에러를 위해 정규화
	XMVECTOR L = XMLoadFloat3(&lightDirWorld);
	L = XMVector3Normalize(L);
	XMStoreFloat3(&lightDirWorld, L); // 다시 저장하면 이제 정확히 단위 벡터

	//그림자를 찍고 싶은 씬 중심 (카메라 주변을 비추는 광원으로 만들기 위한 부분)
	XMFLOAT3 targetWorld(constandices.camPos.x, constandices.camPos.y, constandices.camPos.z);
	XMVECTOR Target = XMLoadFloat3(&targetWorld);

	//lightDir을 이용해서 가짜 light position(=shadow camera eye) 만들기
	float lightDistance = 50.0f; // 조절 가능 (ImGui로 튀기면 좋음)
	XMVECTOR eye = Target - L * lightDistance;


	//up 벡터 선택
	// lightDir이 거의 -Y축이라 up을 (0,1,0) 쓰면 평행에 가까워져서 꼬일 수 있음.
	// 그래서 Z축을 up으로 쓰는게 안전
	XMVECTOR up = XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f);

	XMMATRIX viewMatrix = XMMatrixLookAtLH(eye, Target, up);

	//원거리/직교 투영(방향성 광원은 보통 직교 사용)

	XMMATRIX projMatrix = XMMatrixOrthographicOffCenterLH(
		-boxSize, boxSize,
		-boxSize, boxSize,
		depthNear, depthFar
	);

	//쉐도우용 View/Proj 저장 (행렬은 HLSL에서 column-major라 Transpose해서 보냄)
	XMStoreFloat4x4(&ShadowVP.ShadowView, XMMatrixTranspose(viewMatrix));
	XMStoreFloat4x4(&ShadowVP.ShadowProjection, XMMatrixTranspose(projMatrix));
}


void PBR::SetHoldLight()
{
	// 1. 기존에 쓰던 lightDir을 그대로 가져온다.
	XMFLOAT3 lightDirWorld(LightDir1[0], LightDir1[1], LightDir1[2]);

	// 혹시 모를 에러를 위해 정규화
	XMVECTOR L = XMLoadFloat3(&lightDirWorld);
	L = XMVector3Normalize(L);
	XMStoreFloat3(&lightDirWorld, L); // 다시 저장하면 이제 정확히 단위 벡터

	// 2. 그림자를 찍고 싶은 씬 중심 (타겟)
	XMFLOAT3 targetWorld(0.0f, 0.0f, 0.0f);
	XMVECTOR Target = XMLoadFloat3(&targetWorld);

	// 3. lightDir을 이용해서 가짜 light position(=shadow camera eye) 만들기
	float lightDistance = 50.0f; // 조절 가능 (ImGui로 튀기면 좋음)
	XMVECTOR eye = Target - L * lightDistance;


	// 4. up 벡터 선택
	// lightDir이 거의 -Y축이라 up을 (0,1,0) 쓰면 평행에 가까워져서 꼬일 수 있음.
	// 그래서 Z축을 up으로 쓰는게 안전
	XMVECTOR up = XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f);

	XMMATRIX viewMatrix = XMMatrixLookAtLH(eye, Target, up);

	// 5. 원거리/직교 투영(방향성 광원은 보통 직교 사용)

	XMMATRIX projMatrix = XMMatrixOrthographicOffCenterLH(
		-boxSize, boxSize,
		-boxSize, boxSize,
		depthNear, depthFar
	);

	// 6. 쉐도우용 View/Proj 저장 (행렬은 HLSL에서 column-major라 Transpose해서 보냄)
	XMStoreFloat4x4(&ShadowVP.ShadowView, XMMatrixTranspose(viewMatrix));
	XMStoreFloat4x4(&ShadowVP.ShadowProjection, XMMatrixTranspose(projMatrix));
}


void PBR::DebugMatrix4x4(const char* label, const Matrix& matrix)
{
	DirectX::XMFLOAT4X4 tempMatrix;
	DirectX::XMStoreFloat4x4(&tempMatrix, matrix);

	if (ImGui::BeginTable(label, 4, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg))
	{
		for (int i = 0; i < 4; i++)
		{
			ImGui::TableNextRow();
			for (int j = 0; j < 4; j++)
			{
				ImGui::TableSetColumnIndex(j);

				// tempMatrix의 요소를 출력합니다.
				float value = tempMatrix.m[i][j];

				// 값을 소수점 4자리까지 출력
				ImGui::Text("%.4f", value);
			}
		}
		ImGui::EndTable();
	}
}

void PBR::SetDebugLightFrustum()
{
	m_states = std::make_unique<CommonStates>(m_pDevice.Get());
	m_batch = std::make_unique < PrimitiveBatch<VertexPositionColor>>(m_pDeviceContext.Get());
	m_effect = std::make_unique<BasicEffect>(m_pDevice.Get());

	XMMATRIX lightView = XMMatrixTranspose(XMLoadFloat4x4(&ShadowVP.ShadowView));
	XMMATRIX lightProj = XMMatrixTranspose(XMLoadFloat4x4(&ShadowVP.ShadowProjection));

	m_effect->SetVertexColorEnabled(true);
	m_effect->SetView(lightView);
	m_effect->SetProjection(lightProj);

	{
		void const* shaderByteCode;
		size_t byteCodeLength;

		m_effect->GetVertexShaderBytecode(&shaderByteCode, &byteCodeLength);

		HR_T(
			m_pDevice.Get()->CreateInputLayout(
				VertexPositionColor::InputElements, VertexPositionColor::InputElementCount,
				shaderByteCode, byteCodeLength,
				S_DebugInputLayout.ReleaseAndGetAddressOf()));
	}
}

void PBR::RenderDebugLightFrustum()
{
	//XMMATRIX lightView = (XMLoadFloat4x4(&ShadowVP.ShadowView));
	//XMMATRIX lightProj = (XMLoadFloat4x4(&ShadowVP.ShadowProjection));
	

	// 저장돼 있는 건 transpose 상태라고 가정
	XMMATRIX lightViewT = XMLoadFloat4x4(&ShadowVP.ShadowView);
	XMMATRIX lightProjT = XMLoadFloat4x4(&ShadowVP.ShadowProjection);

	// 디버그 계산/프러스텀 생성/Effect에는 '원본' 형태로 사용해야 하므로 다시 transpose
	XMMATRIX lightView = XMMatrixTranspose(lightViewT);
	XMMATRIX lightProj = XMMatrixTranspose(lightProjT);


	m_effect->SetView(m_View);
	m_effect->SetProjection(m_Proj);

	BoundingFrustum viewFrustum;
	BoundingFrustum::CreateFromMatrix(viewFrustum, lightProj);

	XMMATRIX invView = XMMatrixInverse(nullptr, lightView);
	viewFrustum.Transform(viewFrustum, invView);

	m_pDeviceContext->OMSetBlendState(m_states->Opaque(), nullptr, 0xFFFFFFFF);
	m_pDeviceContext->OMSetDepthStencilState(m_states->DepthNone(), 0);
	m_pDeviceContext->RSSetState(m_states->CullNone());

	m_effect->Apply(m_pDeviceContext.Get());

	m_pDeviceContext->IASetInputLayout(S_DebugInputLayout.Get());

	m_batch->Begin();

	DX::Draw(m_batch.get(), viewFrustum, Colors::Yellow);

	m_batch->End();
}

void PBR::SetPBRValue(std::shared_ptr<ModelLoader> mesh)
{
	//meshMetallic = mesh.get()->GetMaterial().metallicFactor;
	//meshRoughness = mesh.get()->GetMaterial().roughnessFactor;

	if (useModelValues)
	{
		SetPBR.metalicV = mesh.get()->GetMaterial().metallicFactor;
		SetPBR.roughnessV = mesh.get()->GetMaterial().roughnessFactor;
		SetPBR.UseModelValue = useModelValues;
		int a = 0;
	}
	else
	{
		SetPBR.metalicV = m_metalic;
		SetPBR.roughnessV = m_roughness;
		SetPBR.MetalColor = { Metalcolor[0], Metalcolor[1], Metalcolor[2] };
		SetPBR.UseModelValue = useModelValues;
		int a = 0;
	}
}

