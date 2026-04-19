#include "dx11_backend.hpp"
#include "blur/blur.hpp"
#include "glow/glow.hpp"
#include "grayscale/grayscale.hpp"
#include "models/models.hpp"

using alice::c_dx11_backend;

c_dx11_backend::c_dx11_backend( ID3D11Device* dev, ID3D11DeviceContext* context, int w, int h )
    : device( dev )
    , ctx( context )
    , width( w )
    , height( h )
{
}

std::unique_ptr< alice::c_blur > c_dx11_backend::make_blur( )
{
    auto blur = std::make_unique< c_dx11_blur >( );
    if ( !blur->init( device, ctx, width, height ) )
        return nullptr;
    return blur;
}

std::unique_ptr< alice::c_glow > c_dx11_backend::make_glow( )
{
    auto glow = std::make_unique< c_dx11_glow >( );
    if ( !glow->init( device, ctx, width, height ) )
        return nullptr;
    return glow;
}

std::unique_ptr< alice::c_grayscale > c_dx11_backend::make_grayscale( )
{
    auto gs = std::make_unique< c_dx11_grayscale >( );
    if ( !gs->init( device, ctx, width, height ) )
        return nullptr;
    return gs;
}

std::unique_ptr< alice::c_model_engine > c_dx11_backend::make_model_engine( )
{
    return std::make_unique< c_dx11_model_engine >( device, ctx );
}

ImTextureID c_dx11_backend::create_texture_rgba( void const* rgba, int w, int h )
{
    D3D11_TEXTURE2D_DESC td { };
    td.Width = ( UINT ) w;
    td.Height = ( UINT ) h;
    td.MipLevels = 1;
    td.ArraySize = 1;
    td.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    td.SampleDesc = { 1, 0 };
    td.Usage = D3D11_USAGE_DEFAULT;
    td.BindFlags = D3D11_BIND_SHADER_RESOURCE;

    D3D11_SUBRESOURCE_DATA sd { };
    sd.pSysMem = rgba;
    sd.SysMemPitch = ( UINT ) w * sizeof( uint32_t );

    ID3D11Texture2D* tex = nullptr;
    if ( FAILED( device->CreateTexture2D( &td, &sd, &tex ) ) || !tex )
        return 0;

    ID3D11ShaderResourceView* srv = nullptr;
    HRESULT const hr = device->CreateShaderResourceView( tex, nullptr, &srv );
    tex->Release( );

    if ( FAILED( hr ) || !srv )
        return 0;

    return ( ImTextureID ) srv;
}

void c_dx11_backend::destroy_texture( ImTextureID id )
{
    if ( auto* srv = reinterpret_cast< ID3D11ShaderResourceView* >( id ) )
        srv->Release( );
}
