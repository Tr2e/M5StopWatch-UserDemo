#include "model_asset.h"
#include <cassert>
#include <iostream>

int main() {
    const auto& asset=soft3d::assets::fixture_quad::model();
    assert(soft3d::validate(asset)==soft3d::AssetError::None);
    assert(asset.positions.size==4);
    assert(asset.primitives.size==1);
    assert(asset.primitives[0].topology==soft3d::PrimitiveTopology::Quad);
    assert(asset.skeleton.size==1);
    assert(asset.bounds.minimum.x==2.f && asset.bounds.maximum.y==6.f);
    std::cout<<"compiled asset ABI: resident="<<asset.memory.residentBytes
             <<" command="<<asset.memory.maximumCommandBytes<<"\n";
}
