// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gl/gl_surface.h"

#include <dwmapi.h>

#include "base/command_line.h"
#include "base/debug/trace_event.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/win/windows_version.h"
#include "ui/gfx/frame_time.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_implementation.h"
#include "ui/gl/gl_surface_egl.h"
#include "ui/gl/gl_surface_osmesa.h"
#include "ui/gl/gl_surface_stub.h"
#include "ui/gl/gl_surface_wgl.h"

// From ANGLE's egl/eglext.h.
#if !defined(EGL_D3D11_ELSE_D3D9_DISPLAY_ANGLE)
#define EGL_D3D11_ELSE_D3D9_DISPLAY_ANGLE \
  reinterpret_cast<EGLNativeDisplayType>(-2)
#endif
#if !defined(EGL_PLATFORM_ANGLE_ANGLE)
#define EGL_PLATFORM_ANGLE_ANGLE 0x3201
#endif
#if !defined(EGL_PLATFORM_ANGLE_TYPE_ANGLE)
#define EGL_PLATFORM_ANGLE_TYPE_ANGLE 0x3202
#endif
#if !defined(EGL_PLATFORM_ANGLE_TYPE_D3D11_WARP_ANGLE)
#define EGL_PLATFORM_ANGLE_TYPE_D3D11_WARP_ANGLE 0x3206
#endif

namespace gfx {

// This OSMesa GL surface can use GDI to swap the contents of the buffer to a
// view.
class NativeViewGLSurfaceOSMesa : public GLSurfaceOSMesa {
 public:
  explicit NativeViewGLSurfaceOSMesa(gfx::AcceleratedWidget window);
  virtual ~NativeViewGLSurfaceOSMesa();

  // Implement subset of GLSurface.
  virtual bool Initialize() override;
  virtual void Destroy() override;
  virtual bool IsOffscreen() override;
  virtual bool SwapBuffers() override;
  virtual bool SupportsPostSubBuffer() override;
  virtual bool PostSubBuffer(int x, int y, int width, int height) override;

 private:
  gfx::AcceleratedWidget window_;
  HDC device_context_;

  DISALLOW_COPY_AND_ASSIGN(NativeViewGLSurfaceOSMesa);
};

class WinVSyncProvider : public VSyncProvider {
 public:
  explicit WinVSyncProvider(gfx::AcceleratedWidget window) :
    window_(window)
  {
    use_dwm_ = (base::win::GetVersion() >= base::win::VERSION_VISTA);
  }

  virtual ~WinVSyncProvider() {}

  virtual void GetVSyncParameters(const UpdateVSyncCallback& callback) {
    TRACE_EVENT0("gpu", "WinVSyncProvider::GetVSyncParameters");

    base::TimeTicks timebase;
    base::TimeDelta interval;
    bool dwm_active = false;

    // Query the DWM timing info first if available. This will provide the most
    // precise values.
    if (use_dwm_) {
      DWM_TIMING_INFO timing_info;
      timing_info.cbSize = sizeof(timing_info);
      HRESULT result = DwmGetCompositionTimingInfo(NULL, &timing_info);
      if (result == S_OK) {
        dwm_active = true;
        // If FrameTime is not high resolution, we do not want to translate the
        // QPC value provided by DWM into the low-resolution timebase, which
        // would be error prone and jittery. As a fallback, we assume the
        // timebase is zero.
        if (gfx::FrameTime::TimestampsAreHighRes()) {
          timebase = gfx::FrameTime::FromQPCValue(
              static_cast<LONGLONG>(timing_info.qpcVBlank));
        }

        // Swap the numerator/denominator to convert frequency to period.
        if (timing_info.rateRefresh.uiDenominator > 0 &&
            timing_info.rateRefresh.uiNumerator > 0) {
          interval = base::TimeDelta::FromMicroseconds(
              timing_info.rateRefresh.uiDenominator *
              base::Time::kMicrosecondsPerSecond /
              timing_info.rateRefresh.uiNumerator);
        }
      }
    }

    // Double check DWM values against per-display refresh rates.
    // When DWM compositing is active all displays are normalized to the
    // refresh rate of the primary display, and won't composite any faster.
    // If the display refresh rate is higher than the DWM reported value we will
    // favor the DWM value because any additional frames produced will be
    // discarded by the OS. If the display refresh rate is lower, however, we
    // can use that to limit the frames we produce more intelligently.
    // If DWM compositing is not active we will always use the display refresh.
    HMONITOR monitor = MonitorFromWindow(window_, MONITOR_DEFAULTTONEAREST);
    MONITORINFOEX monitor_info;
    monitor_info.cbSize = sizeof(MONITORINFOEX);
    BOOL result = GetMonitorInfo(monitor, &monitor_info);
    if (result) {
      DEVMODE display_info;
      display_info.dmSize = sizeof(DEVMODE);
      display_info.dmDriverExtra = 0;
      result = EnumDisplaySettings(monitor_info.szDevice, ENUM_CURRENT_SETTINGS,
          &display_info);
      if (result && display_info.dmDisplayFrequency > 1) {
        base::TimeDelta display_interval = base::TimeDelta::FromMicroseconds(
            (1.0 / static_cast<double>(display_info.dmDisplayFrequency)) *
            base::Time::kMicrosecondsPerSecond);

        if (!dwm_active || display_interval > interval) {
          interval = display_interval;
        }
      }
    }

    if (interval.ToInternalValue() != 0) {
      callback.Run(timebase, interval);
    }
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(WinVSyncProvider);

  gfx::AcceleratedWidget window_;
  bool use_dwm_;
};

// Helper routine that does one-off initialization like determining the
// pixel format.
bool GLSurface::InitializeOneOffInternal() {
  switch (GetGLImplementation()) {
    case kGLImplementationDesktopGL:
      if (!GLSurfaceWGL::InitializeOneOff()) {
        LOG(ERROR) << "GLSurfaceWGL::InitializeOneOff failed.";
        return false;
      }
      break;
    case kGLImplementationEGLGLES2:
      if (!GLSurfaceEGL::InitializeOneOff()) {
        LOG(ERROR) << "GLSurfaceEGL::InitializeOneOff failed.";
        return false;
      }
      break;
  }
  return true;
}

NativeViewGLSurfaceOSMesa::NativeViewGLSurfaceOSMesa(
    gfx::AcceleratedWidget window)
    : GLSurfaceOSMesa(OSMesaSurfaceFormatRGBA, gfx::Size(1, 1)),
      window_(window),
      device_context_(NULL) {
  DCHECK(window);
}

NativeViewGLSurfaceOSMesa::~NativeViewGLSurfaceOSMesa() {
  Destroy();
}

bool NativeViewGLSurfaceOSMesa::Initialize() {
  if (!GLSurfaceOSMesa::Initialize())
    return false;

  device_context_ = GetDC(window_);
  return true;
}

void NativeViewGLSurfaceOSMesa::Destroy() {
  if (window_ && device_context_)
    ReleaseDC(window_, device_context_);

  device_context_ = NULL;

  GLSurfaceOSMesa::Destroy();
}

bool NativeViewGLSurfaceOSMesa::IsOffscreen() {
  return false;
}

bool NativeViewGLSurfaceOSMesa::SwapBuffers() {
  DCHECK(device_context_);

  gfx::Size size = GetSize();

  // Note: negating the height below causes GDI to treat the bitmap data as row
  // 0 being at the top.
  BITMAPV4HEADER info = { sizeof(BITMAPV4HEADER) };
  info.bV4Width = size.width();
  info.bV4Height = -size.height();
  info.bV4Planes = 1;
  info.bV4BitCount = 32;
  info.bV4V4Compression = BI_BITFIELDS;
  info.bV4RedMask = 0x000000FF;
  info.bV4GreenMask = 0x0000FF00;
  info.bV4BlueMask = 0x00FF0000;
  info.bV4AlphaMask = 0xFF000000;

  // Copy the back buffer to the window's device context. Do not check whether
  // StretchDIBits succeeds or not. It will fail if the window has been
  // destroyed but it is preferable to allow rendering to silently fail if the
  // window is destroyed. This is because the primary application of this
  // class of GLContext is for testing and we do not want every GL related ui /
  // browser test to become flaky if there is a race condition between GL
  // context destruction and window destruction.
  StretchDIBits(device_context_,
                0, 0, size.width(), size.height(),
                0, 0, size.width(), size.height(),
                GetHandle(),
                reinterpret_cast<BITMAPINFO*>(&info),
                DIB_RGB_COLORS,
                SRCCOPY);

  return true;
}

bool NativeViewGLSurfaceOSMesa::SupportsPostSubBuffer() {
  return true;
}

bool NativeViewGLSurfaceOSMesa::PostSubBuffer(
    int x, int y, int width, int height) {
  DCHECK(device_context_);

  gfx::Size size = GetSize();

  // Note: negating the height below causes GDI to treat the bitmap data as row
  // 0 being at the top.
  BITMAPV4HEADER info = { sizeof(BITMAPV4HEADER) };
  info.bV4Width = size.width();
  info.bV4Height = -size.height();
  info.bV4Planes = 1;
  info.bV4BitCount = 32;
  info.bV4V4Compression = BI_BITFIELDS;
  info.bV4RedMask = 0x000000FF;
  info.bV4GreenMask = 0x0000FF00;
  info.bV4BlueMask = 0x00FF0000;
  info.bV4AlphaMask = 0xFF000000;

  // Copy the back buffer to the window's device context. Do not check whether
  // StretchDIBits succeeds or not. It will fail if the window has been
  // destroyed but it is preferable to allow rendering to silently fail if the
  // window is destroyed. This is because the primary application of this
  // class of GLContext is for testing and we do not want every GL related ui /
  // browser test to become flaky if there is a race condition between GL
  // context destruction and window destruction.
  StretchDIBits(device_context_,
                x, size.height() - y - height, width, height,
                x, y, width, height,
                GetHandle(),
                reinterpret_cast<BITMAPINFO*>(&info),
                DIB_RGB_COLORS,
                SRCCOPY);

  return true;
}

scoped_refptr<GLSurface> GLSurface::CreateViewGLSurface(
    gfx::AcceleratedWidget window) {
  TRACE_EVENT0("gpu", "GLSurface::CreateViewGLSurface");
  switch (GetGLImplementation()) {
    case kGLImplementationOSMesaGL: {
      scoped_refptr<GLSurface> surface(
          new NativeViewGLSurfaceOSMesa(window));
      if (!surface->Initialize())
        return NULL;

      return surface;
    }
    case kGLImplementationEGLGLES2: {
      DCHECK(window != gfx::kNullAcceleratedWidget);
      scoped_refptr<NativeViewGLSurfaceEGL> surface(
          new NativeViewGLSurfaceEGL(window));
      scoped_ptr<VSyncProvider> sync_provider;
      sync_provider.reset(new WinVSyncProvider(window));
      if (!surface->Initialize(sync_provider.Pass()))
        return NULL;

      return surface;
    }
    case kGLImplementationDesktopGL: {
      scoped_refptr<GLSurface> surface(new NativeViewGLSurfaceWGL(
          window));
      if (!surface->Initialize())
        return NULL;

      return surface;
    }
    case kGLImplementationMockGL:
      return new GLSurfaceStub;
    default:
      NOTREACHED();
      return NULL;
  }
}

scoped_refptr<GLSurface> GLSurface::CreateOffscreenGLSurface(
    const gfx::Size& size) {
  TRACE_EVENT0("gpu", "GLSurface::CreateOffscreenGLSurface");
  switch (GetGLImplementation()) {
    case kGLImplementationOSMesaGL: {
      scoped_refptr<GLSurface> surface(
          new GLSurfaceOSMesa(OSMesaSurfaceFormatRGBA, size));
      if (!surface->Initialize())
        return NULL;

      return surface;
    }
    case kGLImplementationEGLGLES2: {
      scoped_refptr<GLSurface> surface(new PbufferGLSurfaceEGL(size));
      if (!surface->Initialize())
        return NULL;

      return surface;
    }
    case kGLImplementationDesktopGL: {
      scoped_refptr<GLSurface> surface(new PbufferGLSurfaceWGL(size));
      if (!surface->Initialize())
        return NULL;

      return surface;
    }
    case kGLImplementationMockGL:
      return new GLSurfaceStub;
    default:
      NOTREACHED();
      return NULL;
  }
}

EGLNativeDisplayType GetPlatformDefaultEGLNativeDisplay() {
  if (CommandLine::ForCurrentProcess()->HasSwitch(switches::kDisableD3D11) ||
      CommandLine::ForCurrentProcess()->HasSwitch(switches::kUseWarp))
    return GetDC(NULL);
  return EGL_D3D11_ELSE_D3D9_DISPLAY_ANGLE;
}

}  // namespace gfx
