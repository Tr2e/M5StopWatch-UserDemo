#pragma once
#include <memory>
#include <new>
#ifdef ESP_PLATFORM
#include <esp_heap_caps.h>
#endif

namespace lets_and_go {
// Optional internal-RAM working storage. Owners keep their original fallback;
// low internal memory must degrade performance without preventing rendering.
template<class T> class RenderScratch {
    struct Release {
        void operator()(T* p) const {
#ifdef ESP_PLATFORM
            p->~T();heap_caps_free(p);
#else
            delete p;
#endif
        }
    };
    std::unique_ptr<T,Release> _value;
public:
    bool allocate() {
        if(_value)return true;
#ifdef ESP_PLATFORM
        // Leave room for later driver/task allocations during the app session.
        if(heap_caps_get_free_size(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT)<sizeof(T)+32u*1024u)
            return false;
        auto* memory=heap_caps_malloc(sizeof(T),MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT);
        if(memory)_value.reset(new(memory) T{});
#else
        _value.reset(new(std::nothrow) T{});
#endif
        return bool(_value);
    }
    T* get() const {return _value.get();}
};
} // namespace lets_and_go
