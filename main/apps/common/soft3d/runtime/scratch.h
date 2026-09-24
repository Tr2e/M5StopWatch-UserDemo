#pragma once
#include <memory>
#include <new>
#include <type_traits>
#ifdef ESP_PLATFORM
#include <esp_heap_caps.h>
#endif

namespace soft3d {
// Optional internal-RAM working storage. Owners keep their original fallback;
// low internal memory must degrade performance without preventing rendering.
template<class T> class Scratch {
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
    bool allocateIfBudget(std::size_t availableBytes,std::size_t reserveBytes=32u*1024u) {
        if(availableBytes<sizeof(T)+reserveBytes)return false;
        return allocate();
    }
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

// Runtime-sized counterpart for compact caches whose useful size is known
// only after indexing. T is intentionally restricted to trivial records so
// the ESP allocation can be released without storing per-element lifetime
// metadata.
template<class T> class ScratchBuffer {
    static_assert(std::is_trivially_destructible_v<T>);
    struct Release {
        void operator()(T* p) const {
#ifdef ESP_PLATFORM
            heap_caps_free(p);
#else
            delete[] p;
#endif
        }
    };
    std::unique_ptr<T[],Release> _value;
    std::size_t _capacity=0;
public:
    bool allocateIfBudget(std::size_t count,std::size_t availableBytes,
                          std::size_t reserveBytes=32u*1024u) {
        if(count>(availableBytes>reserveBytes?(availableBytes-reserveBytes)/sizeof(T):0))return false;
        return allocate(count);
    }
    bool allocate(std::size_t count) {
        if(_value)return _capacity>=count;
        if(!count)return true;
#ifdef ESP_PLATFORM
        const std::size_t bytes=count*sizeof(T);
        if(heap_caps_get_free_size(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT)<bytes+32u*1024u)
            return false;
        auto* memory=static_cast<T*>(heap_caps_malloc(bytes,MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT));
#else
        auto* memory=new(std::nothrow) T[count]{};
#endif
        if(memory){_value.reset(memory);_capacity=count;}
        return bool(_value);
    }
    T* get() const{return _value.get();}
    std::size_t capacity() const{return _capacity;}
    void reset(){_value.reset();_capacity=0;}
};
} // namespace soft3d
