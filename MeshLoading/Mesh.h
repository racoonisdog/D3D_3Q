#pragma once
#ifndef MESH_H
#define MESH_H

#include <string>
#include <fstream>
#include <sstream>
#include <iostream>
#include <vector>
#include <stdexcept>
#include <d3d11_1.h>
#include <directxtk/SimpleMath.h> // directXmath 대신 사용
#include <DirectXMath.h>


using namespace DirectX;
using namespace DirectX::SimpleMath;

//윈도우 스마트 포인터용
#include <wrl/client.h>
using Microsoft::WRL::ComPtr;


enum class Transparency {

	Opaque,             //불투명
	Cutout,             //투명   ( 나뭇잎 , 불꽃 등 -> clip())
	AlphaBlend          //알파블랜딩 ( 유리 , 연기 등 ) 

};

struct VERTEX {
    Vector3 position;
    Vector2 tex;

    Vector3 tangent;
    Vector3 bitangent;
    Vector3 normal;
};

struct Texture {
    std::string type;
    std::string path;
    ComPtr<ID3D11ShaderResourceView> texture; // 데이터 없는듯?

    void Release() {
        texture = nullptr;
    }
};

struct Material
{
    Vector4 ambient;
    Vector4 diffuse;
    Vector4 specular;

    int UseClip = false;
    Vector3 padding;
};

class Mesh {
public:
    std::vector<VERTEX> vertices_;
    std::vector<UINT> indices_;
    std::vector<Texture> textures_;
    ComPtr<ID3D11Device> dev_;
    Transparency TransMode = Transparency::Opaque;

    Mesh(ComPtr<ID3D11Device> dev, const std::vector<VERTEX>& vertices, const std::vector<UINT>& indices, const std::vector<Texture>& textures, Transparency tmod) :
        vertices_(vertices),
        indices_(indices),
        textures_(textures),
        dev_(dev),
        TransMode(tmod),
        VertexBuffer{},
        IndexBuffer() {
        this->setupMesh(this->dev_.Get());
    }

    void Draw(ComPtr<ID3D11DeviceContext>& devcon) {
        UINT stride = sizeof(VERTEX);
        UINT offset = 0;

        //DIFFUSE, EMISSIVE, NORMALS, SPECULAR 리소스 4개 사용을 위해 비워두는 작업
        ID3D11ShaderResourceView* Ps_sr[4] = { nullptr };
        devcon->PSSetShaderResources(0, 4, Ps_sr);

        devcon->IASetVertexBuffers(0, 1, VertexBuffer.GetAddressOf(), &stride, &offset);
        devcon->IASetIndexBuffer(IndexBuffer.Get(), DXGI_FORMAT_R32_UINT, 0);

        int textureCount = textures_.size();
        for (int i = 0; i < textureCount; ++i)
        {
            SetTextureType(devcon, i);
        }

        devcon->DrawIndexed(static_cast<UINT>(indices_.size()), 0, 0);
    }

    Material& GetMaterial()
    {
        return material;
    }

    void Close() {

        VertexBuffer = nullptr;
        IndexBuffer = nullptr;
    }
private:
    // Render data
    ComPtr<ID3D11Buffer> VertexBuffer;
    ComPtr<ID3D11Buffer> IndexBuffer;
    Material material;

    // Functions
    // Initializes all the buffer objects/arrays
    void setupMesh(ID3D11Device* dev) {
        HRESULT hr;

        D3D11_BUFFER_DESC vbd;
        vbd.Usage = D3D11_USAGE_IMMUTABLE;
        vbd.ByteWidth = static_cast<UINT>(sizeof(VERTEX) * vertices_.size());
        vbd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
        vbd.CPUAccessFlags = 0;
        vbd.MiscFlags = 0;

        D3D11_SUBRESOURCE_DATA initData;
        initData.pSysMem = &vertices_[0];

        hr = dev->CreateBuffer(&vbd, &initData, VertexBuffer.GetAddressOf());
        if (FAILED(hr)) {
            Close();
            throw std::runtime_error("Failed to create vertex buffer.");
        }

        D3D11_BUFFER_DESC ibd;
        ibd.Usage = D3D11_USAGE_IMMUTABLE;
        ibd.ByteWidth = static_cast<UINT>(sizeof(UINT) * indices_.size());
        ibd.BindFlags = D3D11_BIND_INDEX_BUFFER;
        ibd.CPUAccessFlags = 0;
        ibd.MiscFlags = 0;

        initData.pSysMem = &indices_[0];

        hr = dev->CreateBuffer(&ibd, &initData, IndexBuffer.GetAddressOf());
        if (FAILED(hr)) {
            Close();
            throw std::runtime_error("Failed to create index buffer.");
        }
    }

    void SetTextureType(ComPtr<ID3D11DeviceContext>& dev, int _index)
    {
        std::string typeN = textures_[_index].type;

        if (typeN == "DIFFUSE")
        {
            dev->PSSetShaderResources(0, 1, textures_[_index].texture.GetAddressOf());
        }
        else if (typeN == "EMISSIVE")
        {
            dev->PSSetShaderResources(1, 1, textures_[_index].texture.GetAddressOf());
        }
        else if (typeN == "NORMAL")
        {
            dev->PSSetShaderResources(2, 1, textures_[_index].texture.GetAddressOf());
        }
        else if (typeN == "SPECULAR")
        {
            dev->PSSetShaderResources(3, 1, textures_[_index].texture.GetAddressOf());
        }
    }
};

#endif