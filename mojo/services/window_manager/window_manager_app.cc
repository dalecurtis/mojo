// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/services/window_manager/window_manager_app.h"

#include "base/message_loop/message_loop.h"
#include "base/stl_util.h"
#include "mojo/aura/aura_init.h"
#include "mojo/converters/input_events/input_events_type_converters.h"
#include "mojo/public/cpp/application/application_connection.h"
#include "mojo/public/cpp/application/application_impl.h"
#include "mojo/public/interfaces/application/shell.mojom.h"
#include "mojo/services/public/cpp/view_manager/view.h"
#include "mojo/services/public/cpp/view_manager/view_manager.h"
#include "mojo/services/window_manager/window_manager_delegate.h"
#include "ui/aura/window.h"
#include "ui/aura/window_delegate.h"
#include "ui/aura/window_property.h"
#include "ui/base/hit_test.h"
#include "ui/wm/core/capture_controller.h"
#include "ui/wm/core/focus_controller.h"
#include "ui/wm/public/activation_client.h"

DECLARE_WINDOW_PROPERTY_TYPE(mojo::View*);

namespace mojo {

// The aura::Windows we use to track Views don't render, so we don't actually
// need to supply a fully functional WindowDelegate. We do need to provide _a_
// delegate however, otherwise the event dispatcher won't dispatch events to
// these windows. (The aura WindowTargeter won't allow a delegate-less window
// to be the target of an event, since the window delegate is considered the
// "target handler").
class DummyDelegate : public aura::WindowDelegate {
 public:
  DummyDelegate() {}
  ~DummyDelegate() override {}

 private:
  // WindowDelegate overrides:
  gfx::Size GetMinimumSize() const override { return gfx::Size(); }
  gfx::Size GetMaximumSize() const override { return gfx::Size(); }
  void OnBoundsChanged(const gfx::Rect& old_bounds,
                       const gfx::Rect& new_bounds) override {}
  gfx::NativeCursor GetCursor(const gfx::Point& point) override {
    return gfx::kNullCursor;
  }
  int GetNonClientComponent(const gfx::Point& point) const override {
    return HTCAPTION;
  }
  bool ShouldDescendIntoChildForEventHandling(
      aura::Window* child,
      const gfx::Point& location) override {
    return true;
  }
  bool CanFocus() override { return true; }
  void OnCaptureLost() override {}
  void OnPaint(gfx::Canvas* canvas) override {}
  void OnDeviceScaleFactorChanged(float device_scale_factor) override {}
  void OnWindowDestroying(aura::Window* window) override {}
  void OnWindowDestroyed(aura::Window* window) override {}
  void OnWindowTargetVisibilityChanged(bool visible) override {}
  bool HasHitTestMask() const override { return false; }
  void GetHitTestMask(gfx::Path* mask) const override {}

  DISALLOW_COPY_AND_ASSIGN(DummyDelegate);
};

namespace {

DEFINE_WINDOW_PROPERTY_KEY(View*, kViewKey, NULL);

Id GetIdForWindow(aura::Window* window) {
  return window ? WindowManagerApp::GetViewForWindow(window)->id() : 0;
}

}  // namespace

// Used for calls to Embed() that occur before we've connected to the
// ViewManager.
struct WindowManagerApp::PendingEmbed {
  String url;
  InterfaceRequest<mojo::ServiceProvider> service_provider;
};

////////////////////////////////////////////////////////////////////////////////
// WindowManagerApp, public:

WindowManagerApp::WindowManagerApp(
    ViewManagerDelegate* view_manager_delegate,
    WindowManagerDelegate* window_manager_delegate)
    : shell_(nullptr),
      window_manager_service2_factory_(this),
      window_manager_factory_(this),
      window_manager_internal_service_factory_(this),
      wrapped_view_manager_delegate_(view_manager_delegate),
      window_manager_delegate_(window_manager_delegate),
      view_manager_(NULL),
      root_(NULL),
      dummy_delegate_(new DummyDelegate),
      window_manager_client_(nullptr) {
}

WindowManagerApp::~WindowManagerApp() {}

// static
View* WindowManagerApp::GetViewForWindow(aura::Window* window) {
  return window->GetProperty(kViewKey);
}

aura::Window* WindowManagerApp::GetWindowForViewId(Id view) {
  ViewIdToWindowMap::const_iterator it = view_id_to_window_map_.find(view);
  return it != view_id_to_window_map_.end() ? it->second : NULL;
}

void WindowManagerApp::AddConnection(WindowManagerService2Impl* connection) {
  DCHECK(connections_.find(connection) == connections_.end());
  connections_.insert(connection);
}

void WindowManagerApp::RemoveConnection(WindowManagerService2Impl* connection) {
  DCHECK(connections_.find(connection) != connections_.end());
  connections_.erase(connection);
}

void WindowManagerApp::SetCapture(Id view) {
  capture_client_->capture_client()->SetCapture(GetWindowForViewId(view));
  // TODO(beng): notify connected clients that capture has changed, probably
  //             by implementing some capture-client observer.
}

void WindowManagerApp::FocusWindow(Id view) {
  aura::Window* window = GetWindowForViewId(view);
  DCHECK(window);
  focus_client_->FocusWindow(window);
}

void WindowManagerApp::ActivateWindow(Id view) {
  aura::Window* window = GetWindowForViewId(view);
  DCHECK(window);
  activation_client_->ActivateWindow(window);
}

bool WindowManagerApp::IsReady() const {
  return view_manager_ && root_;
}

void WindowManagerApp::InitFocus(wm::FocusRules* rules) {
  wm::FocusController* focus_controller = new wm::FocusController(rules);
  activation_client_ = focus_controller;
  focus_client_.reset(focus_controller);
  aura::client::SetFocusClient(window_tree_host_->window(), focus_controller);
  aura::client::SetActivationClient(window_tree_host_->window(),
                                    focus_controller);

  focus_client_->AddObserver(this);
  activation_client_->AddObserver(this);
}

void WindowManagerApp::Embed(
    const String& url,
    InterfaceRequest<ServiceProvider> service_provider) {
  if (view_manager_) {
    window_manager_delegate_->Embed(url, service_provider.Pass());
    return;
  }
  scoped_ptr<PendingEmbed> pending_embed(new PendingEmbed);
  pending_embed->url = url;
  pending_embed->service_provider = service_provider.Pass();
  pending_embeds_.push_back(pending_embed.release());
}

////////////////////////////////////////////////////////////////////////////////
// WindowManagerApp, ApplicationDelegate implementation:

void WindowManagerApp::Initialize(ApplicationImpl* impl) {
  shell_ = impl->shell();
  aura_init_.reset(new AuraInit);
  LaunchViewManager(impl);
}

bool WindowManagerApp::ConfigureIncomingConnection(
    ApplicationConnection* connection) {
  connection->AddService(&window_manager_service2_factory_);
  connection->AddService(&window_manager_factory_);
  return true;
}

////////////////////////////////////////////////////////////////////////////////
// WindowManagerApp, ViewManagerDelegate implementation:

void WindowManagerApp::OnEmbed(ViewManager* view_manager,
                               View* root,
                               ServiceProviderImpl* exported_services,
                               scoped_ptr<ServiceProvider> imported_services) {
  DCHECK(!view_manager_ && !root_);
  view_manager_ = view_manager;
  root_ = root;

  window_tree_host_.reset(new WindowTreeHostMojo(shell_, root_));
  window_tree_host_->window()->SetBounds(root->bounds());
  window_tree_host_->window()->Show();

  RegisterSubtree(root_, window_tree_host_->window());

  capture_client_.reset(
      new wm::ScopedCaptureClient(window_tree_host_->window()));

  if (wrapped_view_manager_delegate_) {
    wrapped_view_manager_delegate_->OnEmbed(
        view_manager, root, exported_services, imported_services.Pass());
  }

  for (Connections::const_iterator it = connections_.begin();
       it != connections_.end(); ++it) {
    (*it)->NotifyReady();
  }

  for (PendingEmbed* pending_embed : pending_embeds_)
    Embed(pending_embed->url, pending_embed->service_provider.Pass());
  pending_embeds_.clear();
}

void WindowManagerApp::OnViewManagerDisconnected(
    ViewManager* view_manager) {
  DCHECK_EQ(view_manager_, view_manager);
  if (wrapped_view_manager_delegate_)
    wrapped_view_manager_delegate_->OnViewManagerDisconnected(view_manager);
  view_manager_ = NULL;
  base::MessageLoop::current()->Quit();
}

////////////////////////////////////////////////////////////////////////////////
// WindowManagerApp, ViewObserver implementation:

void WindowManagerApp::OnTreeChanged(
    const ViewObserver::TreeChangeParams& params) {
  if (params.receiver != root_)
    return;
  DCHECK(params.old_parent || params.new_parent);
  if (!params.target)
    return;

  if (params.new_parent) {
    if (view_id_to_window_map_.find(params.target->id()) ==
        view_id_to_window_map_.end()) {
      ViewIdToWindowMap::const_iterator it =
          view_id_to_window_map_.find(params.new_parent->id());
      DCHECK(it != view_id_to_window_map_.end());
      RegisterSubtree(params.target, it->second);
    }
  } else if (params.old_parent) {
    UnregisterSubtree(params.target);
  }
}

void WindowManagerApp::OnViewDestroying(View* view) {
  if (view != root_) {
    Unregister(view);
    return;
  }
  aura::Window* window = GetWindowForViewId(view->id());
  window->RemovePreTargetHandler(this);
  root_ = NULL;
  STLDeleteValues(&view_id_to_window_map_);
  if (focus_client_.get())
    focus_client_->RemoveObserver(this);
  if (activation_client_)
    activation_client_->RemoveObserver(this);
  window_tree_host_.reset();
}

void WindowManagerApp::OnViewBoundsChanged(View* view,
                                           const gfx::Rect& old_bounds,
                                           const gfx::Rect& new_bounds) {
  aura::Window* window = GetWindowForViewId(view->id());
  window->SetBounds(new_bounds);
}

////////////////////////////////////////////////////////////////////////////////
// WindowManagerApp, ui::EventHandler implementation:

void WindowManagerApp::OnEvent(ui::Event* event) {
  if (!window_manager_client_)
    return;

  View* view = GetViewForWindow(static_cast<aura::Window*>(event->target()));
  if (!view)
    return;

  window_manager_client_->DispatchInputEventToView(view->id(),
                                                   Event::From(*event));
}

////////////////////////////////////////////////////////////////////////////////
// WindowManagerApp, aura::client::FocusChangeObserver implementation:

void WindowManagerApp::OnWindowFocused(aura::Window* gained_focus,
                                       aura::Window* lost_focus) {
  for (Connections::const_iterator it = connections_.begin();
       it != connections_.end(); ++it) {
    (*it)->NotifyViewFocused(GetIdForWindow(gained_focus),
                             GetIdForWindow(lost_focus));
  }
}

////////////////////////////////////////////////////////////////////////////////
// WindowManagerApp, aura::client::ActivationChangeObserver implementation:

void WindowManagerApp::OnWindowActivated(aura::Window* gained_active,
                                         aura::Window* lost_active) {
  for (Connections::const_iterator it = connections_.begin();
       it != connections_.end(); ++it) {
    (*it)->NotifyWindowActivated(GetIdForWindow(gained_active),
                                 GetIdForWindow(lost_active));
  }
  if (gained_active) {
    View* view = GetViewForWindow(gained_active);
    view->MoveToFront();
  }
}

////////////////////////////////////////////////////////////////////////////////
// WindowManagerApp, private:

void WindowManagerApp::RegisterSubtree(View* view, aura::Window* parent) {
  view->AddObserver(this);
  DCHECK(view_id_to_window_map_.find(view->id()) ==
         view_id_to_window_map_.end());
  aura::Window* window = new aura::Window(dummy_delegate_.get());
  window->set_id(view->id());
  window->SetProperty(kViewKey, view);
  // All events pass through the root during dispatch, so we only need a handler
  // installed there.
  if (view == root_)
    window->AddPreTargetHandler(this);
  parent->AddChild(window);
  window->SetBounds(view->bounds());
  window->Show();
  view_id_to_window_map_[view->id()] = window;
  View::Children::const_iterator it = view->children().begin();
  for (; it != view->children().end(); ++it)
    RegisterSubtree(*it, window);
}

void WindowManagerApp::UnregisterSubtree(View* view) {
  for (View* child : view->children())
    UnregisterSubtree(child);
  Unregister(view);
}

void WindowManagerApp::Unregister(View* view) {
  ViewIdToWindowMap::iterator it = view_id_to_window_map_.find(view->id());
  if (it == view_id_to_window_map_.end()) {
    // Because we unregister in OnViewDestroying() we can still get a subsequent
    // OnTreeChanged for the same view. Ignore this one.
    return;
  }
  view->RemoveObserver(this);
  DCHECK(it != view_id_to_window_map_.end());
  // Delete before we remove from map as destruction may want to look up view
  // for window.
  delete it->second;
  view_id_to_window_map_.erase(it);
}

void WindowManagerApp::LaunchViewManager(ApplicationImpl* app) {
  // TODO(sky): figure out logic if this connection goes away.
  view_manager_client_factory_.reset(
      new ViewManagerClientFactory(shell_, this));

  MessagePipe pipe;
  ApplicationConnection* view_manager_app =
      app->ConnectToApplication("mojo:view_manager");
  ServiceProvider* view_manager_service_provider =
      view_manager_app->GetServiceProvider();
  view_manager_service_provider->ConnectToService(ViewManagerService::Name_,
                                                  pipe.handle1.Pass());
  view_manager_client_ = ViewManagerClientFactory::WeakBindViewManagerToPipe(
                             pipe.handle0.Pass(), shell_, this).Pass();

  view_manager_app->AddService(&window_manager_internal_service_factory_);
}

}  // namespace mojo
