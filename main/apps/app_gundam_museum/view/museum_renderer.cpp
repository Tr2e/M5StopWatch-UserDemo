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
    enum class Work : uint8_t {Raster,Space,Blit,Begin};
    lets_and_go::CarSurfaceRaster<424,424>* raster=nullptr;
    const lets_and_go::TrackCamera* camera=nullptr;
    const lets_and_go::PreparedSolidRasterPanel* panels=nullptr;
    const lets_and_go::PreparedIndexedSolidRasterPanel* indexedPanels=nullptr;
    const lets_and_go::TrackCameraPoint* projected=nullptr;
    const uint16_t* panelIndices=nullptr;
    lgfx::LGFXBase* canvas=nullptr;
    const View* view=nullptr;
    uint8_t* nativeFrameBuffer=nullptr;
    std::size_t nativeStride=0;
    int outputX=0,outputY=0,outputWidth=0,outputHeight=0;
    std::size_t count=0;
    int top=0,bottom=-1;
    bool reverseIndexed=false;
    uint32_t lastUs=0;
    uint32_t stackFree=UINT32_MAX;
    std::array<uint16_t,276> destinationFirst{},destinationLast{};
    std::array<uint16_t,424> sourceY{};
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
            if(worker.work==Work::Begin)
                worker.raster->begin(worker.outputX,worker.outputY,worker.outputWidth,worker.outputHeight);
            else if(worker.work==Work::Space)space::draw(*worker.canvas,*worker.view);
            else if(worker.work==Work::Blit)
                worker.raster->blitScaledNativeSparseRows(worker.nativeFrameBuffer,worker.nativeStride,
                    worker.outputX,worker.outputY,worker.outputWidth,worker.outputHeight,worker.top,worker.bottom,
                    worker.destinationFirst.data(),worker.destinationLast.data(),worker.sourceY.data());
            else if(worker.indexedPanels)
                worker.raster->preparedIndexedSolidPanelBatchRowsTrusted(worker.indexedPanels,worker.projected,
                    worker.panelIndices,worker.count,worker.top,worker.bottom,worker.reverseIndexed);
            else worker.raster->preparedSolidPanelBatchRowsTrusted(*worker.camera,worker.panels,
                    worker.panelIndices,worker.count,worker.top,worker.bottom);
            worker.lastUs=uint32_t(esp_timer_get_time()-started);
            worker.stackFree=std::min<uint32_t>(worker.stackFree,uxTaskGetStackHighWaterMark(nullptr));
            xSemaphoreGive(worker.done);
        }
        xSemaphoreGive(worker.done);
        vTaskDelete(nullptr);
    }
    bool open() {
        start=xSemaphoreCreateBinary();done=xSemaphoreCreateBinary();
        if(!start || !done)return false;
        destinationFirst.fill(424);
        for(int px=0;px<424;++px) {
            const auto sx=uint16_t((2*px+1)*276/(2*424));
            destinationFirst[sx]=std::min(destinationFirst[sx],uint16_t(px));
            destinationLast[sx]=uint16_t(px+1);
        }
        for(int py=0;py<424;++py)sourceY[py]=uint16_t((2*py+1)*276/(2*424));
        return xTaskCreatePinnedToCore(taskMain,"rx_raster",6144,this,tskIDLE_PRIORITY+2,&task,1)==pdPASS;
    }
    void dispatch(lets_and_go::CarSurfaceRaster<424,424>& target,const lets_and_go::TrackCamera& view,
                  const lets_and_go::PreparedSolidRasterPanel* input,const uint16_t* indices,
                  std::size_t size,int first,int last) {
        raster=&target;camera=&view;panels=input;panelIndices=indices;count=size;top=first;bottom=last;
        indexedPanels=nullptr;projected=nullptr;
        work=Work::Raster;
        xSemaphoreGive(start);
    }
    void dispatchIndexed(lets_and_go::CarSurfaceRaster<424,424>& target,
                         const lets_and_go::PreparedIndexedSolidRasterPanel* input,
                         const lets_and_go::TrackCameraPoint* points,const uint16_t* indices,
                         std::size_t size,int first,int last,bool reverse=false) {
        raster=&target;indexedPanels=input;projected=points;panelIndices=indices;
        count=size;top=first;bottom=last;reverseIndexed=reverse;work=Work::Raster;xSemaphoreGive(start);
    }
    void dispatchSpace(lgfx::LGFXBase& target,const View& state) {
        canvas=&target;view=&state;work=Work::Space;xSemaphoreGive(start);
    }
    void dispatchBegin(lets_and_go::CarSurfaceRaster<424,424>& target,
                       int x,int y,int width,int height) {
        raster=&target;outputX=x;outputY=y;outputWidth=width;outputHeight=height;
        work=Work::Begin;xSemaphoreGive(start);
    }
    void dispatchBlit(lets_and_go::CarSurfaceRaster<424,424>& target,uint8_t* frameBuffer,
                      std::size_t stride,int x,int y,int width,int height,int first,int last) {
        raster=&target;nativeFrameBuffer=frameBuffer;nativeStride=stride;
        outputX=x;outputY=y;outputWidth=width;outputHeight=height;top=first;bottom=last;
        work=Work::Blit;xSemaphoreGive(start);
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
#ifdef ESP_PLATFORM
__attribute__((noinline,optimize("O3")))
#endif
uint32_t classifyPlaneFacePasses(const Mesh& mesh,lets_and_go::CarPoint eye,bool cull,int8_t* passes) {
    uint32_t culled=0;
    for(std::size_t i=0;i<mesh.count;++i) {
        passes[i]=-1;
        const float facing=dot(mesh.normals[i],eye)-mesh.planeOffsets[i];
        if(cull && !mesh.twoSided[i] && facing<-.035f){++culled;continue;}
        passes[i]=facing<=0?0:1;
    }
    return culled;
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
        _surface->fastLowerColor.allocate();
        // Build the default exhibit before creating the worker so the exact
        // active projected-point prefix can claim a compact internal block.
        // Smaller ready/task allocations can then use the remaining fragments.
        buildRx78(_surface->mesh,{true,false,false,Pose::Display});
        const auto registration=museumAssetRegistration(ModelId::Rx78);
        const auto assetError=buildMuseumModelAsset(_surface->assetStorage,_surface->mesh,
            registration.name,registration.flags,registration.profile);
        if(assetError==soft3d::AssetError::None) {
            _surface->instance.asset=&_surface->assetStorage.asset;
            _surface->projection.index(*_surface->instance.asset);
        } else _surface->projection.index(_surface->mesh);
        _surface->projection.preferInternalProjected();
        _surface->fastLowerDepth.allocate();
        _surface->projection.preferInternalReady();
        _surface->fastOccupiedDepth.allocate();
        _surface->fastIndexedPanels.allocate(2112);
        auto* occupied=_surface->fastOccupiedDepth.get();
        _surface->raster.setSparseDepthStorage(
            occupied?occupied->data():_surface->occupiedDepth.data(),_surface->occupiedDepth.size());
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
#ifdef ESP_PLATFORM
__attribute__((optimize("O3")))
#endif
void MuseumRenderer::render(lgfx::LGFXBase& canvas,const View& view,int percent,bool cull,bool gray,bool keepBuried,bool partial){
    const auto startUs=micros();
    const uint16_t clearColor=_spaceEnabled?space::background:space::diagnosticBackground;
    const auto requestedAsset=museumAssetRegistration(view.model);
    const bool fastIndexedAsset=requestedAsset.flags&soft3d::AssetFastIndexedCommands;
    soft3d::FrameWorkload routeWork{};
    if(_surface && _surface->instance.asset) {
        routeWork.uniqueVertices=uint32_t(_surface->instance.asset->positions.size);
        routeWork.visiblePrimitives=uint32_t(_stats.submitted?_stats.submitted:
            _surface->instance.asset->primitives.size);
        routeWork.solidPrimitives=(_surface->instance.asset->flags&soft3d::AssetAllSolid)
            ? routeWork.visiblePrimitives:0;
        routeWork.generalPrimitives=routeWork.visiblePrimitives-routeWork.solidPrimitives;
    }
    const bool workloadRequestsDual=soft3d::selectCoreMode(_profile,_capabilities,routeWork)==
                                    soft3d::CoreMode::DualBands;
    const bool parallelFrame=_optimizations && _parallelRasterFastPath &&
                             workloadRequestsDual && fastIndexedAsset && _surface;
#ifdef ESP_PLATFORM
    uint8_t* nativeFrameBuffer=nullptr;std::size_t nativeStride=0;
    if(_optimizations && _nativeFrameBufferFastPath && &canvas==&GetHAL().getDisplay() &&
       canvas.getColorDepth()==16 && canvas.getRotation()==0) {
        auto* first=GetHAL().getDisplayFrameBufferLine(0);
        auto* second=GetHAL().getDisplayFrameBufferLine(1);
        if(first && second && second>first) {nativeFrameBuffer=first;nativeStride=std::size_t(second-first);}
    }
#endif
    const auto clearFrame=[&] {
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
    };
    uint64_t backgroundUs=startUs,spaceUs=startUs;
    uint32_t frameBackgroundUs=0;
#ifdef ESP_PLATFORM
    bool spaceDispatched=false;
    // Let the previous frame's panel DMA overlap command preparation instead
    // of immediately competing with a full framebuffer clear. The room draw
    // will later occupy CPU0 while CPU1 starts the lower raster band.
    const bool lateBackground=parallelFrame && _parallelWorker;
#else
    constexpr bool lateBackground=false;
#endif
    if(!lateBackground) {
        const auto backgroundStart=micros();clearFrame();backgroundUs=micros();
        frameBackgroundUs=uint32_t(backgroundUs-backgroundStart);
#ifdef ESP_PLATFORM
        if(_spaceEnabled && parallelFrame && _parallelWorker) {
            _parallelWorker->dispatchSpace(canvas,view);spaceDispatched=true;
        } else
#endif
        if(_spaceEnabled)space::draw(canvas,view);
        spaceUs=micros();
    }
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
        const auto registration=museumAssetRegistration(view.model);
        const auto assetError=buildMuseumModelAsset(_surface->assetStorage,_surface->mesh,
            registration.name,registration.flags,registration.profile);
        if(assetError==soft3d::AssetError::None) {
            _surface->instance.asset=&_surface->assetStorage.asset;
            _surface->projection.index(*_surface->instance.asset);
        } else {
            _surface->instance.asset=nullptr;
            _surface->projection.index(_surface->mesh);
        }
        if(registration.flags&soft3d::AssetFastIndexedCommands)
            _surface->projection.preferInternalProjected();
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
    const uint32_t assetFlags=_surface->instance.asset?_surface->instance.asset->flags:0;
    const bool fastAsset=assetFlags&soft3d::AssetFastIndexedCommands;
    const bool useSplitColor=_optimizations && _splitColorFastPath && fastAsset &&
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
    raster.setSparseDepthSpanClearFastPath(_optimizations && _sparseDepthSpanClearFastPath && !hiddenLine);
    // At native size the composite already visits every visible source pixel,
    // so build next frame's occupancy map there and remove the write from the
    // raster hot loop. Scaled composite revisits source samples; recording in
    // the raster remains faster for that path on ESP32-S3.
    raster.setDeferredSparseDepthRecord(w==layout::side && h==layout::side);
    uint32_t parallelDepthClearUs=0;
    const auto depthClearStartUs=micros();
#ifdef ESP_PLATFORM
    const bool parallelDepthClear=parallelFrame && _parallelWorker;
    if(parallelDepthClear)_parallelWorker->dispatchBegin(raster,0,0,w,h);
    else raster.begin(0,0,w,h);
#else
    constexpr bool parallelDepthClear=false;
    raster.begin(0,0,w,h);
#endif
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
    const bool trustedNearPlane=(assetFlags&soft3d::AssetNearPlaneEnvelope) &&
                                projection.nearPlaneSafe(transform.pivot);
    projection.setInternalReadyFastPath(_optimizations && _internalProjectionReadyFastPath && fastAsset);
    projection.setInternalProjectedFastPath(_optimizations && _internalProjectedPointFastPath && fastAsset);
    if(projection.usingInternalProjected())_stats.internalProjectedBytes=uint32_t(projection.count*sizeof(lets_and_go::TrackCameraPoint));
    projection.begin();
    auto* facePasses=projection.passesData(_surface->mesh.count,
        _optimizations && _internalFacePassFastPath && fastAsset);
    const bool indexedEntityFastPath=_optimizations && fastAsset && !view.detail;
    if(indexedEntityFastPath)_stats.culled=classifyPlaneFacePasses(_surface->mesh,eye,cull,facePasses);
    else if(_optimizations)for(std::size_t i=0;i<_surface->mesh.count;++i){
        facePasses[i]=-1;
        if(view.detail && _surface->mesh.parts[i]!=Part::Head)continue;
        const float facing=(assetFlags&soft3d::AssetPrecomputedCullPlanes) ?
            dot(_surface->mesh.normals[i],eye)-_surface->mesh.planeOffsets[i] :
            dot(_surface->mesh.normals[i],subtract(eye,_surface->mesh.anchors[i]));
        // Keep the accepted grazing band and Nu backpack mounting rims;
        // performance work must not silently remove these coverage repairs.
        const bool retainMountRim=nu && _surface->mesh.parts[i]==Part::Backpack;
        if(cull && !_surface->mesh.twoSided[i] && !retainMountRim && facing<-.035f){++_stats.culled;continue;}
        facePasses[i]=facing<=0?0:1;
    }
    const auto prepareUs=micros();
    const bool splitRaster=parallelFrame && !hiddenLine;
    bool splitCompatible=splitRaster;
    int split=h/2;
    while(split<h && (std::size_t(w)*split&7))++split;
    auto* bandIndices=projection.bandIndicesData(_surface->mesh.count,
        splitCompatible && _optimizations && _internalBandIndexFastPath && fastAsset);
    auto* upperIndices=bandIndices;
    auto* lowerIndices=bandIndices?bandIndices+_surface->mesh.count:nullptr;
    std::size_t upperCount=0,lowerCount=0;
    const auto recordBands=[&](std::size_t panelIndex,float top,float bottom) {
        if(!bandIndices)return;
        if(top<=split)new(upperIndices+upperCount++) uint16_t(uint16_t(panelIndex));
        if(bottom>=split-1)new(lowerIndices+lowerCount++) uint16_t(uint16_t(panelIndex));
    };
    std::size_t preparedCount=0;
    // The diagnostic path draws backfaces first. Quantized equal depth must
    // not let an invisible reverse face overwrite a visible front face.
    const bool indexedCompactPrepare=indexedEntityFastPath && _compactPanelPrepareFastPath &&
        splitCompatible && trustedNearPlane;
    const bool useIndexedPanels=indexedCompactPrepare && _indexedPanelFastPath &&
        projection.usingInternalProjected();
    auto* indexedPanels=_surface->preparedPanels.indexed;
    if(useIndexedPanels && _surface->fastIndexedPanels.get()) {
        indexedPanels=_surface->fastIndexedPanels.get();
        _stats.internalCommandBytes=uint32_t(_surface->fastIndexedPanels.capacity()*
                                             sizeof(lets_and_go::PreparedIndexedSolidRasterPanel));
    }
    bool directIndexedBands=false;
    const auto* upperIndexedPanels=indexedPanels;
    const auto* lowerIndexedPanels=indexedPanels;
    if(useIndexedPanels && bandIndices && indexedPanels!=_surface->preparedPanels.indexed) {
        const auto capacity=_surface->fastIndexedPanels.capacity();
        bool overflow=false;
        const auto submittedBefore=_stats.submitted,offscreenBefore=_stats.offscreen;
        for(int pass=0;pass<2 && !overflow;++pass)
          for(std::size_t i=0;i<_surface->mesh.count;++i) {
            if(facePasses[i]!=pass)continue;
            const auto& face=_surface->mesh.panels[i];
            lets_and_go::PreparedIndexedSolidRasterPanel prepared{};float top=0,bottom=0;
            projection.solidIndexedPanel(prepared,top,bottom,camera,face,i,project);
            if(bottom<0 || top>=h) {++_stats.offscreen;continue;}
            prepared.visibility|=lets_and_go::kPreparedSolidBandSelected;
            const bool upper=top<=split,lower=bottom>=split-1;
            if(upperCount+lowerCount+std::size_t(upper)+std::size_t(lower)>capacity) {
                overflow=true;break;
            }
            if(upper)new (&indexedPanels[upperCount++])
                lets_and_go::PreparedIndexedSolidRasterPanel(prepared);
            if(lower)new (&indexedPanels[capacity-1-lowerCount++])
                lets_and_go::PreparedIndexedSolidRasterPanel(prepared);
            ++_stats.submitted;
          }
        if(!overflow) {
            // The lower stream already has draw order when walked backward;
            // avoid reversing about 900 compact commands every frame.
            upperIndexedPanels=indexedPanels;lowerIndexedPanels=indexedPanels+capacity-1;
            directIndexedBands=true;preparedCount=_stats.submitted-submittedBefore;
        } else {
            // An unusual pose can exceed the bounded direct-stream workspace.
            // Rebuild the ordinary shared command + index streams; projected
            // points are recomputed so fallback accounting stays coherent.
            projection.begin();upperCount=lowerCount=preparedCount=0;
            _stats.submitted=submittedBefore;_stats.offscreen=offscreenBefore;
        }
    }
    if(indexedCompactPrepare) {
      if(!directIndexedBands) {
      for(int pass=0;pass<2;++pass)for(std::size_t i=0;i<_surface->mesh.count;++i){
        if(facePasses[i]!=pass)continue;
        const auto& face=_surface->mesh.panels[i];
        if(useIndexedPanels) {
            lets_and_go::PreparedIndexedSolidRasterPanel prepared{};float top=0,bottom=0;
            projection.solidIndexedPanel(prepared,top,bottom,camera,face,i,project);
            if(bottom<0 || top>=h) {++_stats.offscreen;continue;}
            if(bandIndices)prepared.visibility|=lets_and_go::kPreparedSolidBandSelected;
            if(indexedPanels!=_surface->preparedPanels.indexed &&
               preparedCount==_surface->fastIndexedPanels.capacity()) {
                std::memcpy(_surface->preparedPanels.indexed,indexedPanels,
                            preparedCount*sizeof(*indexedPanels));
                indexedPanels=_surface->preparedPanels.indexed;_stats.internalCommandBytes=0;
            }
            new (&indexedPanels[preparedCount])
                lets_and_go::PreparedIndexedSolidRasterPanel(prepared);
            recordBands(preparedCount,top,bottom);++preparedCount;++_stats.submitted;continue;
        }
        lets_and_go::PreparedSolidRasterPanel prepared{};float left=0,right=0,top=0,bottom=0;
        projection.solidPanel<true>(prepared,left,right,top,bottom,camera,face,i,project);
        if(!prepared.visibility || right<0 || left>=w || bottom<0 || top>=h) {
            ++_stats.offscreen;continue;
        }
        if(bandIndices)prepared.visibility|=lets_and_go::kPreparedSolidBandSelected;
        new (&_surface->preparedPanels.solid[preparedCount])
            lets_and_go::PreparedSolidRasterPanel(prepared);
        recordBands(preparedCount,top,bottom);++preparedCount;++_stats.submitted;
      }
      }
    } else for(int pass=0;pass<2;++pass)for(std::size_t i=0;i<_surface->mesh.count;++i){
        const auto& face=_surface->mesh.panels[i];
        if(_optimizations){if(facePasses[i]!=pass)continue;}
        else{
          if(view.detail && _surface->mesh.parts[i]!=Part::Head)continue;
          const float facing=dot(_surface->mesh.normals[i],subtract(eye,_surface->mesh.anchors[i]));
          if((facing<=0?0:1)!=pass)continue;
          const bool retainMountRim=nu && _surface->mesh.parts[i]==Part::Backpack;
          if(cull && !_surface->mesh.twoSided[i] && !retainMountRim && facing<-.035f){++_stats.culled;continue;}
        }
        if(_optimizations && _compactPanelPrepareFastPath && splitCompatible &&
           face.paint==lets_and_go::CarPaint::Solid) {
            lets_and_go::PreparedSolidRasterPanel prepared{};float left=0,right=0,top=0,bottom=0;
            if(trustedNearPlane)projection.solidPanel<true>(prepared,left,right,top,bottom,camera,face,i,project);
            else projection.solidPanel(prepared,left,right,top,bottom,camera,face,i,project);
            if(!prepared.visibility || right<0 || left>=w || bottom<0 || top>=h) {
                ++_stats.offscreen;continue;
            }
            if(bandIndices)prepared.visibility|=lets_and_go::kPreparedSolidBandSelected;
            new (&_surface->preparedPanels.solid[preparedCount])
                lets_and_go::PreparedSolidRasterPanel(prepared);
            recordBands(preparedCount,top,bottom);++preparedCount;++_stats.submitted;continue;
        }
        lets_and_go::PreparedCarPanel prepared{};
        if(_optimizations)projection.panel(prepared,camera,face,i,project);
        else {lets_and_go::prepareCarPanel(prepared,camera,face,project);_stats.transformed+=4;}
        if(!prepared.visibility || prepared.right<0 || prepared.left>=w || prepared.bottom<0 || prepared.top>=h){++_stats.offscreen;continue;}
        if(hiddenLine)prepareHiddenLineFill(prepared);
        if(splitCompatible && prepared.paint==lets_and_go::CarPaint::Solid) {
            auto solid=lets_and_go::compactSolidPanel(prepared);
            if(bandIndices)solid.visibility|=lets_and_go::kPreparedSolidBandSelected;
            new (&_surface->preparedPanels.solid[preparedCount]) lets_and_go::PreparedSolidRasterPanel(
                lets_and_go::compactSolidRasterPanel(solid));
            recordBands(preparedCount,solid.top,solid.bottom);++preparedCount;
        }
        else {
            // Preserve the generic material path if a future RX mesh gains a
            // textured panel; flush earlier solids in their original order.
            if(splitCompatible) {
                raster.preparedSolidPanelBatchRowsTrusted(camera,_surface->preparedPanels.solid,nullptr,
                                                           preparedCount,0,split-1);
                raster.preparedSolidPanelBatchRowsTrusted(camera,_surface->preparedPanels.solid,nullptr,
                                                           preparedCount,split,h-1);
                preparedCount=0;splitCompatible=false;
            }
            raster.preparedPanel(camera,prepared);
        }
        ++_stats.submitted;
    }
    if(useIndexedPanels && !directIndexedBands)
        upperIndexedPanels=lowerIndexedPanels=indexedPanels;
    const auto panelPrepareUs=micros();
#ifdef ESP_PLATFORM
    uint32_t parallelSpaceUs=0;
    if(lateBackground) {
        // The display framebuffer and raster depth/color storage are disjoint.
        // Let their clears overlap, but still join depth clear before any
        // room or entity raster access begins.
        const auto backgroundStart=micros();clearFrame();backgroundUs=micros();
        frameBackgroundUs=uint32_t(backgroundUs-backgroundStart);
        if(!splitCompatible) {
            if(_spaceEnabled)space::draw(canvas,view);
            spaceUs=micros();
        }
    }
    if(parallelDepthClear) {
        _parallelWorker->wait();parallelDepthClearUs=_parallelWorker->lastUs;
    }
    if(spaceDispatched){
        const auto waitStart=micros();_parallelWorker->wait();
        _stats.spaceWaitUs=uint32_t(micros()-waitStart);parallelSpaceUs=_parallelWorker->lastUs;
    }
#endif
    if(splitCompatible) {
        // Keep the split on an occupancy-byte boundary so the two cores never
        // update the same sparse-clear byte. Pixel/depth rows are disjoint.
#ifdef ESP_PLATFORM
        if(_parallelWorker) {
            if(useIndexedPanels)
                _parallelWorker->dispatchIndexed(raster,lowerIndexedPanels,projection.projectedData(),
                    directIndexedBands?nullptr:lowerIndices,
                    directIndexedBands?lowerCount:(bandIndices?lowerCount:preparedCount),split,h-1,
                    directIndexedBands);
            else _parallelWorker->dispatch(raster,camera,_surface->preparedPanels.solid,lowerIndices,
                    bandIndices?lowerCount:preparedCount,split,h-1);
            if(lateBackground) {
                if(_spaceEnabled)space::draw(canvas,view);
                spaceUs=micros();
            }
            const auto mainStart=micros();
            const auto mainCount=directIndexedBands?upperCount:(bandIndices?upperCount:preparedCount);
            if(useIndexedPanels)
                raster.preparedIndexedSolidPanelBatchRowsTrusted(upperIndexedPanels,
                    projection.projectedData(),directIndexedBands?nullptr:upperIndices,mainCount,0,split-1);
            else raster.preparedSolidPanelBatchRowsTrusted(camera,_surface->preparedPanels.solid,upperIndices,
                                                            mainCount,0,split-1);
            _stats.mainRasterUs=uint32_t(micros()-mainStart);
            _parallelWorker->wait();
            _stats.workerRasterUs=_parallelWorker->lastUs;
        } else {
            const auto mainStart=micros();
            if(useIndexedPanels) {
                raster.preparedIndexedSolidPanelBatchRowsTrusted(upperIndexedPanels,
                    projection.projectedData(),directIndexedBands?nullptr:upperIndices,
                    directIndexedBands?upperCount:preparedCount,0,split-1);
                raster.preparedIndexedSolidPanelBatchRowsTrusted(lowerIndexedPanels,
                    projection.projectedData(),directIndexedBands?nullptr:lowerIndices,
                    directIndexedBands?lowerCount:preparedCount,split,h-1,directIndexedBands);
            } else {
                raster.preparedSolidPanelBatchRowsTrusted(camera,_surface->preparedPanels.solid,nullptr,
                                                           preparedCount,0,split-1);
                raster.preparedSolidPanelBatchRowsTrusted(camera,_surface->preparedPanels.solid,nullptr,
                                                           preparedCount,split,h-1);
            }
            _stats.mainRasterUs=uint32_t(micros()-mainStart);
        }
#else
        // Host regressions execute both partitions serially and compare the
        // resulting framebuffer to the original unsplit implementation.
        const auto mainStart=micros();
        const auto topCount=directIndexedBands?upperCount:(bandIndices?upperCount:preparedCount);
        if(useIndexedPanels)raster.preparedIndexedSolidPanelBatchRowsTrusted(upperIndexedPanels,
            projection.projectedData(),directIndexedBands?nullptr:upperIndices,topCount,0,split-1);
        else raster.preparedSolidPanelBatchRowsTrusted(camera,_surface->preparedPanels.solid,upperIndices,
                                                        topCount,0,split-1);
        const auto bottomCount=directIndexedBands?lowerCount:(bandIndices?lowerCount:preparedCount);
        if(useIndexedPanels)raster.preparedIndexedSolidPanelBatchRowsTrusted(lowerIndexedPanels,
            projection.projectedData(),directIndexedBands?nullptr:lowerIndices,bottomCount,split,h-1,
            directIndexedBands);
        else raster.preparedSolidPanelBatchRowsTrusted(camera,_surface->preparedPanels.solid,lowerIndices,
                                                        bottomCount,split,h-1);
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
            if(facePasses[i]<0)continue;
            facing=facePasses[i]==1?1.f:-1.f;
        }else{
            if(view.detail && _surface->mesh.parts[i]!=Part::Head)continue;
            facing=dot(_surface->mesh.normals[i],subtract(eye,_surface->mesh.anchors[i]));
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
    if(_parallelWorker && raster.canBlitScaledNativeSparse(layout::side,layout::side,
                                                           nativeFrameBuffer,nativeStride,outputX)) {
        const int blitSplit=220;
        _parallelWorker->dispatchBlit(raster,nativeFrameBuffer,nativeStride,outputX,layout::top,
                                      layout::side,layout::side,blitSplit,layout::side-1);
        raster.blitScaledNativeSparseRows(nativeFrameBuffer,nativeStride,outputX,layout::top,
                                          layout::side,layout::side,0,blitSplit-1,
                                          _parallelWorker->destinationFirst.data(),
                                          _parallelWorker->destinationLast.data(),
                                          _parallelWorker->sourceY.data());
        _parallelWorker->wait();
    } else
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
    _stats.backgroundUs=frameBackgroundUs;
#ifdef ESP_PLATFORM
    _stats.spaceUs=spaceDispatched?parallelSpaceUs:uint32_t(spaceUs-backgroundUs);
    if(_parallelWorker)_stats.workerStackFree=_parallelWorker->stackFree;
#else
    _stats.spaceUs=uint32_t(spaceUs-backgroundUs);
#endif
    _stats.depthClearUs=parallelDepthClear?parallelDepthClearUs:uint32_t(clearUs-depthClearStartUs);
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
