#pragma once
#include <cstddef>
#include <cstdint>
#include <array>
#include <mutex>

// Host contract double, not an emulator of I2S or the ESP-IDF audio task.
class Hal {
public:
    using Callback=void(*)(void*,int16_t*,std::size_t);
    bool audioStartStream(void* owner,Callback callback) {
        std::lock_guard<std::mutex> guard(mutex);
        ++starts;
        if(reject || (active && active!=owner))return false;
        active=owner;fill=callback;return true;
    }
    void audioStopStream(void* owner) {
        std::lock_guard<std::mutex> guard(mutex);
        ++stops;
        if(owner==active){active=nullptr;fill=nullptr;}
    }
    void pump() {
        std::lock_guard<std::mutex> guard(mutex);
        std::array<int16_t,512> pcm{};
        if(fill){fill(active,pcm.data(),pcm.size());++callbacks;}
    }
    bool reject=false;
    unsigned starts=0,stops=0,callbacks=0;
    void* active=nullptr;
private:
    Callback fill=nullptr;
    std::mutex mutex;
};
inline Hal& GetHAL(){static Hal hal;return hal;}
