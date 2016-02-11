// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_COMPOSITOR_IMAGE_TRANSPORT_FACTORY_H_
#define CONTENT_BROWSER_COMPOSITOR_IMAGE_TRANSPORT_FACTORY_H_

#include <string>

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "cc/surfaces/surface_id_allocator.h"
#include "content/common/content_export.h"
#include "ui/events/latency_info.h"
#include "ui/gfx/native_widget_types.h"

namespace cc {
class SurfaceManager;
}

namespace gfx {
class Size;
enum class SwapResult;
}

namespace ui {
class Compositor;
class ContextFactory;
class Texture;
}

namespace blink {
class WebGraphicsContext3D;
}

namespace content {
class GLHelper;

// This class provides a way to get notified when surface handles get lost.
class CONTENT_EXPORT ImageTransportFactoryObserver {
 public:
  virtual ~ImageTransportFactoryObserver() {}

  // Notifies that the surface handles generated by ImageTransportFactory were
  // lost.
  // When this is called, the old resources (e.g. shared context, GL helper)
  // still exist, but are about to be destroyed. Getting a reference to those
  // resources from the ImageTransportFactory (e.g. through GetGLHelper) will
  // return newly recreated, valid resources.
  virtual void OnLostResources() = 0;
};

// This class provides the interface for creating the support for the
// cross-process image transport, both for creating the shared surface handle
// (destination surface for the GPU process) and the transport client (logic for
// using that surface as a texture). The factory is a process-wide singleton.
class CONTENT_EXPORT ImageTransportFactory {
 public:
  virtual ~ImageTransportFactory() {}

  // Initializes the global transport factory.
  static void Initialize();

  // Initializes the global transport factory for unit tests using the provided
  // context factory.
  static void InitializeForUnitTests(scoped_ptr<ImageTransportFactory> factory);

  // Terminates the global transport factory.
  static void Terminate();

  // Gets the factory instance.
  static ImageTransportFactory* GetInstance();

  // Gets the image transport factory as a context factory for the compositor.
  virtual ui::ContextFactory* GetContextFactory() = 0;

  virtual cc::SurfaceManager* GetSurfaceManager() = 0;

  // Gets a GLHelper instance, associated with the shared context. This
  // GLHelper will get destroyed whenever the shared context is lost
  // (ImageTransportFactoryObserver::OnLostResources is called).
  virtual GLHelper* GetGLHelper() = 0;

  virtual void AddObserver(ImageTransportFactoryObserver* observer) = 0;
  virtual void RemoveObserver(ImageTransportFactoryObserver* observer) = 0;

#if defined(OS_MACOSX)
  virtual void OnGpuSwapBuffersCompleted(
      int surface_id,
      const std::vector<ui::LatencyInfo>& latency_info,
      gfx::SwapResult result) = 0;

  // Called with |suspended| as true when the ui::Compositor has been
  // disconnected from an NSView and may be attached to another one. Called
  // with |suspended| as false after the ui::Compositor has been connected to
  // a new NSView and the first commit targeted at the new NSView has
  // completed. This ensures that content and frames intended for the old
  // NSView will not flash in the new NSView.
  virtual void SetCompositorSuspendedForRecycle(ui::Compositor* compositor,
                                                bool suspended) = 0;
  // Used by GpuProcessHostUIShim to determine if a frame should not be
  // displayed because it is targetted to an NSView that has been disconnected.
  virtual bool SurfaceShouldNotShowFramesAfterSuspendForRecycle(
      int surface_id) const = 0;
#endif
};

}  // namespace content

#endif  // CONTENT_BROWSER_COMPOSITOR_IMAGE_TRANSPORT_FACTORY_H_
