/*
 * Copyright 2015 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef GrPipelineBuilder_DEFINED
#define GrPipelineBuilder_DEFINED

#include "GrBlend.h"
#include "GrClip.h"
#include "GrDrawTargetCaps.h"
#include "GrGpuResourceRef.h"
#include "GrFragmentStage.h"
#include "GrProcOptInfo.h"
#include "GrRenderTarget.h"
#include "GrStencil.h"
#include "GrXferProcessor.h"
#include "SkMatrix.h"
#include "effects/GrCoverageSetOpXP.h"
#include "effects/GrDisableColorXP.h"
#include "effects/GrPorterDuffXferProcessor.h"
#include "effects/GrSimpleTextureEffect.h"

class GrBatch;
class GrDrawTargetCaps;
class GrPaint;
class GrTexture;

class GrPipelineBuilder {
public:
    GrPipelineBuilder();

    GrPipelineBuilder(const GrPipelineBuilder& pipelineBuilder) {
        SkDEBUGCODE(fBlockEffectRemovalCnt = 0;)
        *this = pipelineBuilder;
    }

    virtual ~GrPipelineBuilder();

    /**
     * Initializes the GrPipelineBuilder based on a GrPaint, view matrix and render target. Note
     * that GrPipelineBuilder encompasses more than GrPaint. Aspects of GrPipelineBuilder that have
     * no GrPaint equivalents are set to default values with the exception of vertex attribute state
     * which is unmodified by this function and clipping which will be enabled.
     */
    void setFromPaint(const GrPaint&, GrRenderTarget*, const GrClip&);

    ///////////////////////////////////////////////////////////////////////////
    /// @name Fragment Processors
    ///
    /// GrFragmentProcessors are used to compute per-pixel color and per-pixel fractional coverage.
    /// There are two chains of FPs, one for color and one for coverage. The first FP in each
    /// chain gets the initial color/coverage from the GrPrimitiveProcessor. It computes an output
    /// color/coverage which is fed to the next FP in the chain. The last color and coverage FPs
    /// feed their output to the GrXferProcessor which controls blending.
    ////

    int numColorFragmentStages() const { return fColorStages.count(); }
    int numCoverageFragmentStages() const { return fCoverageStages.count(); }
    int numFragmentStages() const { return this->numColorFragmentStages() +
                                               this->numCoverageFragmentStages(); }

    const GrFragmentStage& getColorFragmentStage(int idx) const { return fColorStages[idx]; }
    const GrFragmentStage& getCoverageFragmentStage(int idx) const { return fCoverageStages[idx]; }

    const GrFragmentProcessor* addColorProcessor(const GrFragmentProcessor* effect) {
        SkASSERT(effect);
        SkNEW_APPEND_TO_TARRAY(&fColorStages, GrFragmentStage, (effect));
        fColorProcInfoValid = false;
        return effect;
    }

    const GrFragmentProcessor* addCoverageProcessor(const GrFragmentProcessor* effect) {
        SkASSERT(effect);
        SkNEW_APPEND_TO_TARRAY(&fCoverageStages, GrFragmentStage, (effect));
        fCoverageProcInfoValid = false;
        return effect;
    }

    /**
     * Creates a GrSimpleTextureEffect that uses local coords as texture coordinates.
     */
    void addColorTextureProcessor(GrTexture* texture, const SkMatrix& matrix) {
        this->addColorProcessor(GrSimpleTextureEffect::Create(texture, matrix))->unref();
    }

    void addCoverageTextureProcessor(GrTexture* texture, const SkMatrix& matrix) {
        this->addCoverageProcessor(GrSimpleTextureEffect::Create(texture, matrix))->unref();
    }

    void addColorTextureProcessor(GrTexture* texture,
                                  const SkMatrix& matrix,
                                  const GrTextureParams& params) {
        this->addColorProcessor(GrSimpleTextureEffect::Create(texture, matrix, params))->unref();
    }

    void addCoverageTextureProcessor(GrTexture* texture,
                                     const SkMatrix& matrix,
                                     const GrTextureParams& params) {
        this->addCoverageProcessor(GrSimpleTextureEffect::Create(texture, matrix, params))->unref();
    }

    /**
     * When this object is destroyed it will remove any color/coverage FPs from the pipeline builder
     * that were added after its constructor.
     */
    class AutoRestoreFragmentProcessors : public ::SkNoncopyable {
    public:
        AutoRestoreFragmentProcessors() 
            : fPipelineBuilder(NULL)
            , fColorEffectCnt(0)
            , fCoverageEffectCnt(0) {}

        AutoRestoreFragmentProcessors(GrPipelineBuilder* ds)
            : fPipelineBuilder(NULL)
            , fColorEffectCnt(0)
            , fCoverageEffectCnt(0) {
            this->set(ds);
        }

        ~AutoRestoreFragmentProcessors() { this->set(NULL); }

        void set(GrPipelineBuilder* ds);

        bool isSet() const { return SkToBool(fPipelineBuilder); }

    private:
        GrPipelineBuilder*    fPipelineBuilder;
        int             fColorEffectCnt;
        int             fCoverageEffectCnt;
    };

    /// @}

    ///////////////////////////////////////////////////////////////////////////
    /// @name Blending
    ////

    /**
     * Determines whether multiplying the computed per-pixel color by the pixel's fractional
     * coverage before the blend will give the correct final destination color. In general it
     * will not as coverage is applied after blending.
     */
    bool canTweakAlphaForCoverage() const;

    /**
     * This function returns true if the render target destination pixel values will be read for
     * blending during draw.
     */
    bool willBlendWithDst(const GrPrimitiveProcessor*) const;

    /**
     * Installs a GrXPFactory. This object controls how src color, fractional pixel coverage,
     * and the dst color are blended.
     */
    const GrXPFactory* setXPFactory(const GrXPFactory* xpFactory) {
        fXPFactory.reset(SkRef(xpFactory));
        return xpFactory;
    }

    /**
     * Sets a GrXPFactory that will ignore src color and perform a set operation between the draws
     * output coverage and the destination. This is useful to render coverage masks as CSG.
     */
    void setCoverageSetOpXPFactory(SkRegion::Op regionOp, bool invertCoverage = false) {
        fXPFactory.reset(GrCoverageSetOpXPFactory::Create(regionOp, invertCoverage));
    }

    /**
     * Sets a GrXPFactory that disables color writes to the destination. This is useful when
     * rendering to the stencil buffer.
     */
    void setDisableColorXPFactory() {
        fXPFactory.reset(GrDisableColorXPFactory::Create());
    }

    const GrXPFactory* getXPFactory() const {
        if (!fXPFactory) {
            fXPFactory.reset(GrPorterDuffXPFactory::Create(SkXfermode::kSrc_Mode));
        }
        return fXPFactory.get();
    }

    /**
     * Checks whether the xp will need a copy of the destination to correctly blend.
     */
    bool willXPNeedDstCopy(const GrDrawTargetCaps& caps, const GrProcOptInfo& colorPOI,
                           const GrProcOptInfo& coveragePOI) const;

    /// @}


    ///////////////////////////////////////////////////////////////////////////
    /// @name Render Target
    ////

    /**
     * Retrieves the currently set render-target.
     *
     * @return    The currently set render target.
     */
    GrRenderTarget* getRenderTarget() const { return fRenderTarget.get(); }

    /**
     * Sets the render-target used at the next drawing call
     *
     * @param target  The render target to set.
     */
    void setRenderTarget(GrRenderTarget* target) { fRenderTarget.reset(SkSafeRef(target)); }

    /// @}

    ///////////////////////////////////////////////////////////////////////////
    /// @name Stencil
    ////

    const GrStencilSettings& getStencil() const { return fStencilSettings; }

    /**
     * Sets the stencil settings to use for the next draw.
     * Changing the clip has the side-effect of possibly zeroing
     * out the client settable stencil bits. So multipass algorithms
     * using stencil should not change the clip between passes.
     * @param settings  the stencil settings to use.
     */
    void setStencil(const GrStencilSettings& settings) { fStencilSettings = settings; }

    /**
     * Shortcut to disable stencil testing and ops.
     */
    void disableStencil() { fStencilSettings.setDisabled(); }

    GrStencilSettings* stencil() { return &fStencilSettings; }

    /**
     * AutoRestoreStencil
     *
     * This simple struct saves and restores the stencil settings
     */
    class AutoRestoreStencil : public ::SkNoncopyable {
    public:
        AutoRestoreStencil() : fPipelineBuilder(NULL) {}

        AutoRestoreStencil(GrPipelineBuilder* ds) : fPipelineBuilder(NULL) { this->set(ds); }

        ~AutoRestoreStencil() { this->set(NULL); }

        void set(GrPipelineBuilder* ds) {
            if (fPipelineBuilder) {
                fPipelineBuilder->setStencil(fStencilSettings);
            }
            fPipelineBuilder = ds;
            if (ds) {
                fStencilSettings = ds->getStencil();
            }
        }

        bool isSet() const { return SkToBool(fPipelineBuilder); }

    private:
        GrPipelineBuilder*  fPipelineBuilder;
        GrStencilSettings   fStencilSettings;
    };


    /// @}

    ///////////////////////////////////////////////////////////////////////////
    /// @name State Flags
    ////

    /**
     *  Flags that affect rendering. Controlled using enable/disableState(). All
     *  default to disabled.
     */
    enum StateBits {
        /**
         * Perform dithering. TODO: Re-evaluate whether we need this bit
         */
        kDither_StateBit        = 0x01,
        /**
         * Perform HW anti-aliasing. This means either HW FSAA, if supported by the render target,
         * or smooth-line rendering if a line primitive is drawn and line smoothing is supported by
         * the 3D API.
         */
        kHWAntialias_StateBit   = 0x02,

        kLast_StateBit = kHWAntialias_StateBit,
    };

    bool isDither() const { return 0 != (fFlagBits & kDither_StateBit); }
    bool isHWAntialias() const { return 0 != (fFlagBits & kHWAntialias_StateBit); }

    /**
     * Enable render state settings.
     *
     * @param stateBits bitfield of StateBits specifying the states to enable
     */
    void enableState(uint32_t stateBits) { fFlagBits |= stateBits; }

    /**
     * Disable render state settings.
     *
     * @param stateBits bitfield of StateBits specifying the states to disable
     */
    void disableState(uint32_t stateBits) { fFlagBits &= ~(stateBits); }

    /**
     * Enable or disable stateBits based on a boolean.
     *
     * @param stateBits bitfield of StateBits to enable or disable
     * @param enable    if true enable stateBits, otherwise disable
     */
    void setState(uint32_t stateBits, bool enable) {
        if (enable) {
            this->enableState(stateBits);
        } else {
            this->disableState(stateBits);
        }
    }

    /// @}

    ///////////////////////////////////////////////////////////////////////////
    /// @name Face Culling
    ////

    enum DrawFace {
        kInvalid_DrawFace = -1,

        kBoth_DrawFace,
        kCCW_DrawFace,
        kCW_DrawFace,
    };

    /**
     * Gets whether the target is drawing clockwise, counterclockwise,
     * or both faces.
     * @return the current draw face(s).
     */
    DrawFace getDrawFace() const { return fDrawFace; }

    /**
     * Controls whether clockwise, counterclockwise, or both faces are drawn.
     * @param face  the face(s) to draw.
     */
    void setDrawFace(DrawFace face) {
        SkASSERT(kInvalid_DrawFace != face);
        fDrawFace = face;
    }

    /// @}

    ///////////////////////////////////////////////////////////////////////////

    GrPipelineBuilder& operator=(const GrPipelineBuilder& that);

    // TODO delete when we have Batch
    const GrProcOptInfo& colorProcInfo(const GrPrimitiveProcessor* pp) const {
        this->calcColorInvariantOutput(pp);
        return fColorProcInfo;
    }

    const GrProcOptInfo& coverageProcInfo(const GrPrimitiveProcessor* pp) const {
        this->calcCoverageInvariantOutput(pp);
        return fCoverageProcInfo;
    }

    const GrProcOptInfo& colorProcInfo(const GrBatch* batch) const {
        this->calcColorInvariantOutput(batch);
        return fColorProcInfo;
    }

    const GrProcOptInfo& coverageProcInfo(const GrBatch* batch) const {
        this->calcCoverageInvariantOutput(batch);
        return fCoverageProcInfo;
    }

    void setClip(const GrClip& clip) { fClip = clip; }
    const GrClip& clip() const { return fClip; }

private:
    // Calculating invariant color / coverage information is expensive, so we partially cache the
    // results.
    //
    // canUseFracCoveragePrimProc() - Called in regular skia draw, caches results but only for a
    //                                specific color and coverage.  May be called multiple times
    // willBlendWithDst() - only called by Nvpr, does not cache results
    // GrOptDrawState constructor - never caches results

    /**
     * Primproc variants of the calc functions
     * TODO remove these when batch is everywhere
     */
    void calcColorInvariantOutput(const GrPrimitiveProcessor*) const;
    void calcCoverageInvariantOutput(const GrPrimitiveProcessor*) const;

    /**
     * GrBatch provides the initial seed for these loops based off of its initial geometry data
     */
    void calcColorInvariantOutput(const GrBatch*) const;
    void calcCoverageInvariantOutput(const GrBatch*) const;

    /**
     * If fColorProcInfoValid is false, function calculates the invariant output for the color
     * processors and results are stored in fColorProcInfo.
     */
    void calcColorInvariantOutput(GrColor) const;

    /**
     * If fCoverageProcInfoValid is false, function calculates the invariant output for the coverage
     * processors and results are stored in fCoverageProcInfo.
     */
    void calcCoverageInvariantOutput(GrColor) const;

    // Some of the auto restore objects assume that no effects are removed during their lifetime.
    // This is used to assert that this condition holds.
    SkDEBUGCODE(int fBlockEffectRemovalCnt;)

    typedef SkSTArray<4, GrFragmentStage> FragmentStageArray;

    SkAutoTUnref<GrRenderTarget>            fRenderTarget;
    uint32_t                                fFlagBits;
    GrStencilSettings                       fStencilSettings;
    DrawFace                                fDrawFace;
    mutable SkAutoTUnref<const GrXPFactory> fXPFactory;
    FragmentStageArray                      fColorStages;
    FragmentStageArray                      fCoverageStages;
    GrClip                                  fClip;

    mutable GrProcOptInfo fColorProcInfo;
    mutable GrProcOptInfo fCoverageProcInfo;
    mutable bool fColorProcInfoValid;
    mutable bool fCoverageProcInfoValid;
    mutable GrColor fColorCache;
    mutable GrColor fCoverageCache;

    friend class GrPipeline;
};

#endif
