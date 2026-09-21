#include "museum_renderer.h"
#include "../model/char_zaku.h"
#include "../model/nu_gundam.h"
#include "../model/sazabi.h"
#include "../model/strike_gundam.h"
#include "../model/destiny_gundam.h"
#include "museum_layout.h"
#include "museum_space.h"
#include "museum_wireframe.h"
#include <algorithm>
#include <cmath>
#include <new>
#ifdef ESP_PLATFORM
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>
#else
#include <chrono>
#endif

namespace gundam_museum {
#ifdef ESP_PLATFORM
struct MuseumParallelWorker {
    enum class Work : uint8_t {Raster,Space};
    lets_and_go::CarSurfaceRaster<424,424>* raster=nullptr;
    const lets_and_go::TrackCamera* camera=nullptr;
    const lets_and_go::PreparedSolidPanel* panels=nullptr;
    lgfx::LGFXBase* canvas=nullptr;
    const View* view=nullptr;
    std::size_t count=0;
    int top=0,bottom=-1;
    uint32_t lastUs=0;
    Work work=Work::Raster;
    SemaphoreHandle_t start=nullptr,done=nullptr;
    TaskHandle_t task=nullptr;
    bool stopping=false;

    static void taskMain(void* argument) {
        auto& worker=*static_cast<MuseumParallelWorker*>(argument);
        for(;;) {
            xSemaphoreTake(worker.start,portMAX_DELAY);
            if(worker.stopping)break;
            const auto started=esp_timer_get_time();
            if(worker.work==Work::Space)space::draw(*worker.canvas,*worker.view);
            else for(std::size_t i=0;i<worker.count;++i)
                worker.raster->preparedSolidPanelRows(*worker.camera,worker.panels[i],worker.top,worker.bottom);
            worker.lastUs=uint32_t(esp_timer_get_time()-started);
            xSemaphoreGive(worker.done);
        }
        xSemaphoreGive(worker.done);
        vTaskDelete(nullptr);
    }
    bool open() {
        start=xSemaphoreCreateBinary();done=xSemaphoreCreateBinary();
        if(!start || !done)return false;
        return xTaskCreatePinnedToCore(taskMain,"rx_raster",6144,this,tskIDLE_PRIORITY+2,&task,1)==pdPASS;
    }
    void dispatch(lets_and_go::CarSurfaceRaster<424,424>& target,const lets_and_go::TrackCamera& view,
                  const lets_and_go::PreparedSolidPanel* input,std::size_t size,int first,int last) {
        raster=&target;camera=&view;panels=input;count=size;top=first;bottom=last;
        work=Work::Raster;
        xSemaphoreGive(start);
    }
    void dispatchSpace(lgfx::LGFXBase& target,const View& state) {
        canvas=&target;view=&state;work=Work::Space;xSemaphoreGive(start);
    }
    void wait(){xSemaphoreTake(done,portMAX_DELAY);}
    ~MuseumParallelWorker() {
        if(task){stopping=true;xSemaphoreGive(start);wait();task=nullptr;}
        if(start)vSemaphoreDelete(start);
        if(done)vSemaphoreDelete(done);
    }
};
#endif
static_assert(hiddenLinePaper==space::background && hiddenLineInk==space::navigation);
namespace {
uint64_t micros(){
#ifdef ESP_PLATFORM
    return esp_timer_get_time();
#else
    return uint64_t(std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count());
#endif
}
constexpr uint16_t background=space::background,white=space::navigation;
void label(lgfx::LGFXBase& c,const char* text,int x,int y,int size,uint16_t color=white){
    c.setTextDatum(textdatum_t::middle_center);c.setTextColor(color,background);c.setTextSize(size);c.drawString(text,x,y);
}
}
MuseumRenderer::MuseumRenderer()=default;
MuseumRenderer::~MuseumRenderer(){close();}
bool MuseumRenderer::open(){
    close();_surface.reset(new(std::nothrow) Surface{});
    if(_surface){
        _surface->raster.setSparseDepthStorage(_surface->occupiedDepth.data(),_surface->occupiedDepth.size());
        _surface->fastLowerColor.allocate();
        // Build the default exhibit before creating the worker so the exact
        // active projected-point prefix can claim a compact internal block.
        // Smaller ready/task allocations can then use the remaining fragments.
        buildRx78(_surface->mesh,{true,false,false,Pose::Display});
        _surface->projection.index(_surface->mesh);
        _surface->projection.preferInternalProjected();
        _surface->fastLowerDepth.allocate();
        _surface->projection.preferInternalReady();
        _model=ModelId::Rx78;_pose=Pose::Display;_equipment=true;
        _gray=false;_buried=false;_cached=true;
    }
#ifdef ESP_PLATFORM
    if(_surface){
        _parallelWorker.reset(new(std::nothrow) MuseumParallelWorker{});
        if(!_parallelWorker || !_parallelWorker->open())_parallelWorker.reset();
    }
#endif
    return bool(_surface);
}
void MuseumRenderer::close(){
#ifdef ESP_PLATFORM
    _parallelWorker.reset();
#endif
    _surface.reset();_cached=false;_stats={};
}
std::size_t MuseumRenderer::workingBytes(){return sizeof(Surface);}
void MuseumRenderer::render(lgfx::LGFXBase& canvas,const View& view,int percent,bool cull,bool gray,bool keepBuried,bool partial){
    const auto startUs=micros();
    const uint16_t clearColor=_spaceEnabled?space::background:space::diagnosticBackground;
#ifdef ESP_PLATFORM
    uint8_t* nativeFrameBuffer=nullptr;std::size_t nativeStride=0;
    if(_optimizations && _nativeFrameBufferFastPath && &canvas==&GetHAL().getDisplay() &&
       canvas.getColorDepth()==16 && canvas.getRotation()==0) {
        auto* first=GetHAL().getDisplayFrameBufferLine(0);
        auto* second=GetHAL().getDisplayFrameBufferLine(1);
        if(first && second && second>first) {nativeFrameBuffer=first;nativeStride=std::size_t(second-first);}
    }
#endif
    // The rotating room reaches the screen poles, so its dirty area is the
    // entire screen. Retain the old bounded clear for model-only diagnostics.
    if(partial && !_spaceEnabled)canvas.fillRect(0,layout::top,canvas.width(),layout::side,clearColor);
#ifdef ESP_PLATFORM
    else if(_nativeClearFastPath && nativeFrameBuffer && !(canvas.width()&1) && !(nativeStride&3) &&
            !(reinterpret_cast<std::uintptr_t>(nativeFrameBuffer)&3) &&
            nativeStride>=std::size_t(canvas.width())*2) {
        const uint16_t native=uint16_t((clearColor<<8)|(clearColor>>8));
        const uint32_t pair=uint32_t(native)|(uint32_t(native)<<16);
        for(int y=0;y<canvas.height();++y)
            std::fill_n(reinterpret_cast<uint32_t*>(nativeFrameBuffer+std::size_t(y)*nativeStride),canvas.width()/2,pair);
        GetHAL().markDisplayFrameBufferModified(0,0,canvas.width(),canvas.height());
    }
#endif
    else canvas.fillScreen(clearColor);
    const auto backgroundUs=micros();
    const bool parallelFrame=_optimizations && _parallelRasterFastPath &&
                             view.model==ModelId::Rx78 && _surface;
#ifdef ESP_PLATFORM
    bool spaceDispatched=false;
    if(_spaceEnabled && parallelFrame && _parallelWorker) {
        _parallelWorker->dispatchSpace(canvas,view);spaceDispatched=true;
    } else
#endif
    if(_spaceEnabled)space::draw(canvas,view);
    const auto spaceUs=micros();
    if(!_surface){label(canvas,"MODEL MEMORY UNAVAILABLE",canvas.width()/2,220,1);return;}
    const bool nu=view.model==ModelId::NuGundam,strike=view.model==ModelId::StrikeGundam,destiny=view.model==ModelId::DestinyGundam,zaku=view.model==ModelId::CharZaku,sazabi=view.model==ModelId::Sazabi;
    const bool hiddenLine=view.model==ModelId::NuGundam;
    if(!_cached || _model!=view.model || _equipment!=view.equipment || _gray!=gray || _buried!=keepBuried || _pose!=view.pose){
        if(destiny)buildDestinyGundam(_surface->mesh,{view.equipment,keepBuried,gray,view.pose});
        else if(strike)buildStrikeGundam(_surface->mesh,{view.equipment,keepBuried,gray,view.pose});
        else if(sazabi)buildSazabi(_surface->mesh,{view.equipment,keepBuried,gray,view.pose});
        else if(nu)buildNuGundam(_surface->mesh,{view.equipment,keepBuried,gray,view.pose});
        else if(zaku)buildCharZaku(_surface->mesh,{view.equipment,keepBuried,gray,view.pose});
        else buildRx78(_surface->mesh,{view.equipment,keepBuried,gray,view.pose});
        _surface->projection.index(_surface->mesh);
        if(view.model==ModelId::Rx78)_surface->projection.preferInternalProjected();
        _model=view.model;
        _pose=view.pose;
        _equipment=view.equipment;_gray=gray;_buried=keepBuried;_cached=true;
    }
    // One fixed envelope per exhibit, no angle-dependent auto-fit breathing.
    const MuseumCamera transform(view);const auto eye=transform.eye();
    // Hidden-line keeps the Z-buffer while orbiting. Drag fills a cheaper
    // paper pass, then nearest-expands so ink is still 1px native.
    const bool drag=hiddenLine && percent<100;
    const int p=std::clamp(percent,50,100),w=(424*p+50)/100,h=(424*p+50)/100;
    auto& raster=_surface->raster;
    constexpr int fastColorWidth=276,fastColorSplit=138;
    const bool useSplitColor=_optimizations && _splitColorFastPath && view.model==ModelId::Rx78 &&
        w==fastColorWidth && h==fastColorWidth && _surface->fastLowerColor.get();
    raster.setSplitColorStorage(useSplitColor?_surface->fastLowerColor.get()->data():nullptr,
                                fastColorWidth,fastColorSplit);
    const bool useSplitDepth=useSplitColor && _splitDepthFastPath && _surface->fastLowerDepth.get();
    raster.setSplitDepthStorage(useSplitDepth?_surface->fastLowerDepth.get()->data():nullptr,
                                fastColorWidth,fastColorSplit);
    // Keep exact barycentric interpolation. View Car's incremental mode
    // changes thin SD panels at some continuous angles (see performance log).
    raster.setSolidFastPath(_optimizations);
    raster.setSolidSpanFastPath(_optimizations && _solidSpanFastPath);
    raster.setTrustedSolidDepthFastPath(_optimizations && _trustedSolidDepthFastPath);
    raster.setSolidQuadFastPath(_optimizations && _solidQuadFastPath);
    raster.setDirectSpanFastPath(_optimizations && _directSpanFastPath);
    raster.setNativeFrameBufferFastPath(_optimizations && _nativeFrameBufferFastPath);
    raster.setSparseCompositeFastPath(_optimizations && _sparseCompositeFastPath);
    // Nu's drag path expands the compact depth plane in place after fill, so
    // its next clear cannot use the source-resolution occupancy map.
    raster.setSparseDepthClearFastPath(_optimizations && _sparseDepthClearFastPath && !hiddenLine);
    // At native size the composite already visits every visible source pixel,
    // so build next frame's occupancy map there and remove the write from the
    // raster hot loop. Scaled composite revisits source samples; recording in
    // the raster remains faster for that path on ESP32-S3.
    raster.setDeferredSparseDepthRecord(w==layout::side && h==layout::side);
    const auto depthClearStartUs=micros();
    raster.begin(0,0,w,h);
    if(hiddenLine)_surface->edges.begin();
    const auto clearUs=micros();
    lets_and_go::TrackCamera camera{};
    camera.principalX=w*.5f;camera.principalY=h*.5f;
    const float scale=MuseumCamera::scale(view);
    camera.focalLength=scale*7.f*float(h)/layout::side;
    const float correction=float(w)/h;
    const auto project=[&](Point point,uint8_t tag){auto v=transform(point,tag);v.x*=correction;return v;};
    _stats={};_stats.total=_surface->mesh.count;
    if(useSplitDepth)_stats.internalDepthBytes=uint32_t(fastColorWidth*fastColorSplit*sizeof(uint16_t));
    auto& projection=_surface->projection;
    projection.setInternalReadyFastPath(_optimizations && _internalProjectionReadyFastPath && view.model==ModelId::Rx78);
    projection.setInternalProjectedFastPath(_optimizations && _internalProjectedPointFastPath && view.model==ModelId::Rx78);
    if(projection.usingInternalProjected())_stats.internalProjectedBytes=uint32_t(projection.count*sizeof(lets_and_go::TrackCameraPoint));
    projection.begin();
    if(_optimizations)for(std::size_t i=0;i<_surface->mesh.count;++i){
        projection.passes[i]=-1;
        if(view.detail && _surface->mesh.parts[i]!=Part::Head)continue;
        const auto& face=_surface->mesh.panels[i];
        const float facing=dot(_surface->mesh.normals[i],subtract(eye,face.point[0]));
        // Keep the accepted grazing band and Nu backpack mounting rims;
        // performance work must not silently remove these coverage repairs.
        const bool retainMountRim=nu && _surface->mesh.parts[i]==Part::Backpack;
        if(cull && !_surface->mesh.twoSided[i] && !retainMountRim && facing<-.035f){++_stats.culled;continue;}
        projection.passes[i]=facing<=0?0:1;
    }
    const auto prepareUs=micros();
    const bool splitRaster=parallelFrame && !hiddenLine;
    bool splitCompatible=splitRaster;
    std::size_t preparedCount=0;
    // The diagnostic path draws backfaces first. Quantized equal depth must
    // not let an invisible reverse face overwrite a visible front face.
    for(int pass=0;pass<2;++pass)for(std::size_t i=0;i<_surface->mesh.count;++i){
        const auto& face=_surface->mesh.panels[i];
        if(_optimizations){
            if(projection.passes[i]!=pass)continue;
        }else{
            if(view.detail && _surface->mesh.parts[i]!=Part::Head)continue;
            const float facing=dot(_surface->mesh.normals[i],subtract(eye,face.point[0]));
            if((facing<=0?0:1)!=pass)continue;
            const bool retainMountRim=nu && _surface->mesh.parts[i]==Part::Backpack;
            if(cull && !_surface->mesh.twoSided[i] && !retainMountRim && facing<-.035f){++_stats.culled;continue;}
        }
        if(_optimizations && _compactPanelPrepareFastPath && splitCompatible &&
           face.paint==lets_and_go::CarPaint::Solid) {
            lets_and_go::PreparedSolidPanel prepared{};float left=0,right=0;
            projection.solidPanel(prepared,left,right,camera,face,i,project);
            if(!prepared.visibility || right<0 || left>=w || prepared.bottom<0 || prepared.top>=h) {
                ++_stats.offscreen;continue;
            }
            _surface->preparedPanels[preparedCount++]=prepared;++_stats.submitted;continue;
        }
        lets_and_go::PreparedCarPanel prepared{};
        if(_optimizations)projection.panel(prepared,camera,face,i,project);
        else {lets_and_go::prepareCarPanel(prepared,camera,face,project);_stats.transformed+=4;}
        if(!prepared.visibility || prepared.right<0 || prepared.left>=w || prepared.bottom<0 || prepared.top>=h){++_stats.offscreen;continue;}
        if(hiddenLine)prepareHiddenLineFill(prepared);
        if(splitCompatible && prepared.paint==lets_and_go::CarPaint::Solid)
            _surface->preparedPanels[preparedCount++]=lets_and_go::compactSolidPanel(prepared);
        else {
            // Preserve the generic material path if a future RX mesh gains a
            // textured panel; flush earlier solids in their original order.
            if(splitCompatible) {
                for(std::size_t j=0;j<preparedCount;++j)
                    raster.preparedSolidPanelRows(camera,_surface->preparedPanels[j],0,h-1);
                preparedCount=0;splitCompatible=false;
            }
            raster.preparedPanel(camera,prepared);
        }
        ++_stats.submitted;
    }
    const auto panelPrepareUs=micros();
#ifdef ESP_PLATFORM
    uint32_t parallelSpaceUs=0;
    if(spaceDispatched){
        const auto waitStart=micros();_parallelWorker->wait();
        _stats.spaceWaitUs=uint32_t(micros()-waitStart);parallelSpaceUs=_parallelWorker->lastUs;
    }
#endif
    if(splitCompatible) {
        // Keep the split on an occupancy-byte boundary so the two cores never
        // update the same sparse-clear byte. Pixel/depth rows are disjoint.
        int split=h/2;
        while(split<h && (std::size_t(w)*split&7))++split;
#ifdef ESP_PLATFORM
        if(_parallelWorker) {
            _parallelWorker->dispatch(raster,camera,_surface->preparedPanels.data(),preparedCount,split,h-1);
            const auto mainStart=micros();
            for(std::size_t i=0;i<preparedCount;++i)
                raster.preparedSolidPanelRows(camera,_surface->preparedPanels[i],0,split-1);
            _stats.mainRasterUs=uint32_t(micros()-mainStart);
            _parallelWorker->wait();
            _stats.workerRasterUs=_parallelWorker->lastUs;
        } else {
            const auto mainStart=micros();
            for(std::size_t i=0;i<preparedCount;++i)
                raster.preparedSolidPanelRows(camera,_surface->preparedPanels[i],0,h-1);
            _stats.mainRasterUs=uint32_t(micros()-mainStart);
        }
#else
        // Host regressions execute both partitions serially and compare the
        // resulting framebuffer to the original unsplit implementation.
        const auto mainStart=micros();
        for(std::size_t i=0;i<preparedCount;++i)
            raster.preparedSolidPanelRows(camera,_surface->preparedPanels[i],0,split-1);
        for(std::size_t i=0;i<preparedCount;++i)
            raster.preparedSolidPanelRows(camera,_surface->preparedPanels[i],split,h-1);
        _stats.mainRasterUs=uint32_t(micros()-mainStart);
#endif
    }
    if(hiddenLine && drag){
        raster.upsampleNearestToFull();
        // Reproject strokes at native resolution. Scaling cached low-resolution
        // coordinates changes floating-point rounding at depth-test boundaries
        // and can erase isolated ink pixels on grazing Nu edges.
        std::fill_n(projection.ready.begin(),projection.count,false);
        camera.principalX=raster.width()*.5f;camera.principalY=raster.height()*.5f;
        camera.focalLength=scale*7.f;
    }
    const int strokeW=raster.width(),strokeH=raster.height();
    if(hiddenLine)for(std::size_t i=0;i<_surface->mesh.count;++i){
        const auto& face=_surface->mesh.panels[i];
        float facing=0;
        if(_optimizations){
            if(projection.passes[i]<0)continue;
            facing=projection.passes[i]==1?1.f:-1.f;
        }else{
            if(view.detail && _surface->mesh.parts[i]!=Part::Head)continue;
            facing=dot(_surface->mesh.normals[i],subtract(eye,face.point[0]));
            if(cull && !_surface->mesh.twoSided[i] && facing<-.035f)continue;
        }
        // Far-side panels still fill depth when culling is off; stroking them
        // would draw the reverse mesh. Keep two-sided lips that were filled.
        if(facing<=0 && !_surface->mesh.twoSided[i])continue;
        lets_and_go::PreparedCarPanel prepared{};
        if(_optimizations)projection.panel(prepared,camera,face,i,project);
        else lets_and_go::prepareCarPanel(prepared,camera,face,project);
        if(!prepared.visibility || prepared.right<0 || prepared.left>=strokeW || prepared.bottom<0 || prepared.top>=strokeH)continue;
        strokeHiddenLinePanel(raster,camera,prepared,&_surface->edges,&projection.indices[i*4]);
    }
    const auto rasterUs=micros();
    const int outputX=(canvas.width()-layout::side)/2;
#ifdef ESP_PLATFORM
    raster.blitScaled(canvas,outputX,layout::top,layout::side,layout::side,nativeFrameBuffer,nativeStride);
    if(nativeFrameBuffer)GetHAL().markDisplayFrameBufferModified(outputX,layout::top,layout::side,layout::side);
#else
    raster.blitScaled(canvas,outputX,layout::top,layout::side,layout::side);
#endif
    const auto blitUs=micros();
    _stats.vertices=projection.count;
    if(_optimizations)_stats.transformed=projection.transformed;
    _stats.clearUs=uint32_t(clearUs-startUs);_stats.prepareUs=uint32_t(prepareUs-clearUs);
    _stats.rasterUs=uint32_t(rasterUs-prepareUs);_stats.blitUs=uint32_t(blitUs-rasterUs);
    _stats.backgroundUs=uint32_t(backgroundUs-startUs);
#ifdef ESP_PLATFORM
    _stats.spaceUs=spaceDispatched?parallelSpaceUs:uint32_t(spaceUs-backgroundUs);
#else
    _stats.spaceUs=uint32_t(spaceUs-backgroundUs);
#endif
    _stats.depthClearUs=uint32_t(clearUs-depthClearStartUs);
    _stats.panelPrepareUs=uint32_t(panelPrepareUs-prepareUs);
    // Keep navigation above both the room and the exhibit.
    for(const auto& button:{layout::previous,layout::next}){
        const int x=button.x+button.width/2,y=button.y+button.height/2;
        const int sign=button.x<canvas.width()/2?-1:1;
        for(int d=0;d<2;++d){
            canvas.drawLine(x-sign*4+d,y-9,x+sign*4+d,y,white);
            canvas.drawLine(x+sign*4+d,y,x-sign*4+d,y+9,white);
        }
    }
    _stats.overlayUs=uint32_t(micros()-blitUs);
}
} // namespace gundam_museum
