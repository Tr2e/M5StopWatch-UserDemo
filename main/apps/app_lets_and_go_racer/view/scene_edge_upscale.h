#pragma once

namespace lets_and_go {
// Scale2x corner agreement, applied to stored pixel values without color
// conversion. See https://www.scale2x.it/algorithm . Borders repeat the nearest
// source pixel. This refines 2x block contours; it is not geometry coverage AA.
template<class Pixel> void upscaleEdgeRow(const Pixel* above,const Pixel* current,
    const Pixel* below,int width,Pixel* upper,Pixel* lower) {
    for(int x=0;x<width;++x) {
        const auto center=current[x],north=above[x],south=below[x];
        const auto west=current[x ? x-1 : x],east=current[x+1<width ? x+1 : x];
        if(!(north==south) && !(west==east)) {
            upper[2*x]=west==north ? west : center;
            upper[2*x+1]=north==east ? east : center;
            lower[2*x]=west==south ? west : center;
            lower[2*x+1]=south==east ? east : center;
        } else {
            upper[2*x]=upper[2*x+1]=lower[2*x]=lower[2*x+1]=center;
        }
    }
}
} // namespace lets_and_go
