// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/application/application_runner_chromium.h"
#include "mojo/public/c/system/main.h"
#include "mojo/public/cpp/application/application_delegate.h"
#include "mojo/public/cpp/application/application_impl.h"
#include "mojo/public/cpp/application/connect.h"
#include "mojo/public/cpp/application/service_provider_impl.h"
#include "mojo/services/public/cpp/view_manager/view_manager.h"
#include "mojo/services/public/cpp/view_manager/view_manager_delegate.h"
#include "mojo/services/public/cpp/view_manager/view_observer.h"
#include "mojo/services/public/interfaces/input_events/input_events.mojom.h"
#include "mojo/services/window_manager/window_manager_app.h"
#include "mojo/services/window_manager/window_manager_delegate.h"
#include "sky/tools/debugger/focus_rules.h"
#include "sky/tools/debugger/debugger.mojom.h"

namespace sky {

class SkyDebugger : public mojo::ApplicationDelegate,
                    public mojo::ViewManagerDelegate,
                    public mojo::ViewObserver,
                    public mojo::InterfaceFactory<Debugger>,
                    public mojo::InterfaceImpl<Debugger> {
 public:
  SkyDebugger()
      : window_manager_app_(new mojo::WindowManagerApp(this, nullptr)),
        view_manager_(nullptr),
        root_(nullptr),
        content_(nullptr) {}
  virtual ~SkyDebugger() {}

 private:
  // Overridden from mojo::ApplicationDelegate:
  virtual void Initialize(mojo::ApplicationImpl* app) override {
    window_manager_app_->Initialize(app);
    app->ConnectToApplication("mojo:sky_debugger_prompt");
  }

  virtual bool ConfigureIncomingConnection(
      mojo::ApplicationConnection* connection) override {
    window_manager_app_->ConfigureIncomingConnection(connection);
    connection->AddService(this);
    return true;
  }

  virtual bool ConfigureOutgoingConnection(
      mojo::ApplicationConnection* connection) override {
    window_manager_app_->ConfigureOutgoingConnection(connection);
    connection->AddService(this);
    return true;
  }

  // Overridden from mojo::ViewManagerDelegate:
  virtual void OnEmbed(
      mojo::ViewManager* view_manager,
      mojo::View* root,
      mojo::ServiceProviderImpl* exported_services,
      scoped_ptr<mojo::ServiceProvider> remote_service_provider) override {
    view_manager_ = view_manager;

    root_ = root;
    root_->AddObserver(this);

    content_ = mojo::View::Create(view_manager_);
    content_->SetBounds(root_->bounds());
    root_->AddChild(content_);

    window_manager_app_->InitFocus(
        new FocusRules(window_manager_app_.get(), content_));
  }

  virtual void OnViewManagerDisconnected(
      mojo::ViewManager* view_manager) override {
    view_manager_ = nullptr;
    root_ = nullptr;
  }

  virtual void OnViewDestroyed(mojo::View* view) override {
    view->RemoveObserver(this);
  }

  virtual void OnViewBoundsChanged(mojo::View* view,
                                   const gfx::Rect& old_bounds,
                                   const gfx::Rect& new_bounds) override {
    content_->SetBounds(new_bounds);
  }

  // Overridden from InterfaceFactory<Debugger>
  virtual void Create(mojo::ApplicationConnection* connection,
                      mojo::InterfaceRequest<Debugger> request) override {
    mojo::WeakBindToRequest(this, &request);
  }

  // Overridden from Debugger
  virtual void NavigateToURL(const mojo::String& url) override {
    content_->Embed(url);
  }

  scoped_ptr<mojo::WindowManagerApp> window_manager_app_;

  mojo::ViewManager* view_manager_;
  mojo::View* root_;
  mojo::View* content_;

  DISALLOW_COPY_AND_ASSIGN(SkyDebugger);
};

}  // namespace sky

MojoResult MojoMain(MojoHandle shell_handle) {
  mojo::ApplicationRunnerChromium runner(new sky::SkyDebugger);
  return runner.Run(shell_handle);
}
