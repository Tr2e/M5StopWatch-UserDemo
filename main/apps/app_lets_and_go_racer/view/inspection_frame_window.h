#pragma once
#include "../model/car_catalog.h"
#include <algorithm>
#include <array>
#include <cstdint>

namespace lets_and_go {
// Recent rendered frames only. Idle time is not a slow frame, and a change of
// car or resolution starts a fresh window instead of mixing unlike workloads.
class InspectionFrameWindow {
public:
    struct Summary {
        unsigned count=0;
        uint32_t drawUs=0,presentUs=0,p95Us=0,maxUs=0;
    };
    void reset() { _count=_next=0;_car=CarId::Count;_percent=0; }
    void record(CarId car,int percent,uint32_t drawUs,uint32_t presentUs) {
        if(car!=_car || percent!=_percent) {
            reset();_car=car;_percent=percent;
        }
        _frames[_next]={drawUs,presentUs};_next=(_next+1)%_frames.size();
        _count=std::min<unsigned>(_count+1,_frames.size());
    }
    int percent() const { return _percent; }
    Summary summary() const {
        Summary result;result.count=_count;
        if(!_count)return result;
        uint64_t draw=0,present=0;
        std::array<uint32_t,64> times{};
        for(unsigned i=0;i<_count;++i) {
            draw+=_frames[i].draw;present+=_frames[i].present;
            times[i]=_frames[i].draw+_frames[i].present;
        }
        std::sort(times.begin(),times.begin()+_count);
        result.drawUs=uint32_t(draw/_count);result.presentUs=uint32_t(present/_count);
        result.p95Us=times[(_count*95+99)/100-1];result.maxUs=times[_count-1];
        return result;
    }
private:
    struct Frame { uint32_t draw=0,present=0; };
    std::array<Frame,64> _frames{};
    CarId _car=CarId::Count;
    int _percent=0;
    unsigned _count=0,_next=0;
};
} // namespace lets_and_go
