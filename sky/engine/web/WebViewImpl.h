/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef WebViewImpl_h
#define WebViewImpl_h

#include "core/html/ime/InputMethodContext.h"
#include "core/page/ContextMenuProvider.h"
#include "platform/geometry/IntPoint.h"
#include "platform/geometry/IntRect.h"
#include "platform/graphics/GraphicsLayer.h"
#include "public/platform/WebGestureCurveTarget.h"
#include "public/platform/WebLayer.h"
#include "public/platform/WebPoint.h"
#include "public/platform/WebRect.h"
#include "public/platform/WebSize.h"
#include "public/platform/WebString.h"
#include "public/web/WebInputEvent.h"
#include "public/web/WebNavigationPolicy.h"
#include "public/web/WebView.h"
#include "web/ChromeClientImpl.h"
#include "web/ContextMenuClientImpl.h"
#include "web/EditorClientImpl.h"
#include "web/PageOverlayList.h"
#include "web/PageWidgetDelegate.h"
#include "web/SpellCheckerClientImpl.h"
#include "wtf/OwnPtr.h"
#include "wtf/RefCounted.h"
#include "wtf/Vector.h"

namespace blink {

class Frame;
class LinkHighlight;
class RenderLayerCompositor;
class UserGestureToken;
class WebActiveGestureAnimation;
class WebLocalFrameImpl;
class WebImage;
class WebSettingsImpl;

class WebViewImpl FINAL : public WebView
    , public RefCounted<WebViewImpl>
    , public WebGestureCurveTarget
    , public PageWidgetEventHandler {
public:
    static WebViewImpl* create(WebViewClient*);

    // WebWidget methods:
    virtual void close() OVERRIDE;
    virtual WebSize size() OVERRIDE;
    virtual void willStartLiveResize() OVERRIDE;
    virtual void resize(const WebSize&) OVERRIDE;
    virtual void resizePinchViewport(const WebSize&) OVERRIDE;
    virtual void willEndLiveResize() OVERRIDE;

    virtual void beginFrame(const WebBeginFrameArgs&) OVERRIDE;
    virtual void didCommitFrameToCompositor() OVERRIDE;

    virtual void layout() OVERRIDE;
    virtual void paint(WebCanvas*, const WebRect&) OVERRIDE;
#if OS(ANDROID)
    virtual void paintCompositedDeprecated(WebCanvas*, const WebRect&) OVERRIDE;
#endif
    virtual void compositeAndReadbackAsync(WebCompositeAndReadbackAsyncCallback*) OVERRIDE;
    virtual bool isTrackingRepaints() const OVERRIDE;
    virtual void themeChanged() OVERRIDE;
    virtual bool handleInputEvent(const WebInputEvent&) OVERRIDE;
    virtual void setCursorVisibilityState(bool isVisible) OVERRIDE;
    virtual bool hasTouchEventHandlersAt(const WebPoint&) OVERRIDE;
    virtual void applyScrollAndScale(const WebSize&, float) OVERRIDE;
    virtual void mouseCaptureLost() OVERRIDE;
    virtual void setFocus(bool enable) OVERRIDE;
    virtual bool setComposition(
        const WebString& text,
        const WebVector<WebCompositionUnderline>& underlines,
        int selectionStart,
        int selectionEnd) OVERRIDE;
    virtual bool confirmComposition() OVERRIDE;
    virtual bool confirmComposition(ConfirmCompositionBehavior selectionBehavior) OVERRIDE;
    virtual bool confirmComposition(const WebString& text) OVERRIDE;
    virtual bool compositionRange(size_t* location, size_t* length) OVERRIDE;
    virtual WebTextInputInfo textInputInfo() OVERRIDE;
    virtual WebColor backgroundColor() const OVERRIDE;
    virtual bool selectionBounds(WebRect& anchor, WebRect& focus) const OVERRIDE;
    virtual void didShowCandidateWindow() OVERRIDE;
    virtual void didUpdateCandidateWindow() OVERRIDE;
    virtual void didHideCandidateWindow() OVERRIDE;
    virtual bool selectionTextDirection(WebTextDirection& start, WebTextDirection& end) const OVERRIDE;
    virtual bool isSelectionAnchorFirst() const OVERRIDE;
    virtual bool caretOrSelectionRange(size_t* location, size_t* length) OVERRIDE;
    virtual void setTextDirection(WebTextDirection) OVERRIDE;
    virtual bool isAcceleratedCompositingActive() const OVERRIDE;
    virtual void willCloseLayerTreeView() OVERRIDE;

    // WebView methods:
    virtual void setMainFrame(WebFrame*) OVERRIDE;
    virtual void setSpellCheckClient(WebSpellCheckClient*) OVERRIDE;
    virtual WebSettings* settings() OVERRIDE;
    virtual WebString pageEncoding() const OVERRIDE;
    virtual void setPageEncoding(const WebString&) OVERRIDE;
    virtual bool isTransparent() const OVERRIDE;
    virtual void setIsTransparent(bool value) OVERRIDE;
    virtual void setBaseBackgroundColor(WebColor) OVERRIDE;
    virtual bool tabsToLinks() const OVERRIDE;
    virtual void setTabsToLinks(bool value) OVERRIDE;
    virtual bool tabKeyCyclesThroughElements() const OVERRIDE;
    virtual void setTabKeyCyclesThroughElements(bool value) OVERRIDE;
    virtual bool isActive() const OVERRIDE;
    virtual void setIsActive(bool value) OVERRIDE;
    virtual void setDomainRelaxationForbidden(bool, const WebString& scheme) OVERRIDE;
    virtual void setOpenedByDOM() OVERRIDE;
    virtual WebFrame* mainFrame() OVERRIDE;
    virtual WebFrame* focusedFrame() OVERRIDE;
    virtual void setFocusedFrame(WebFrame*) OVERRIDE;
    virtual void setInitialFocus(bool reverse) OVERRIDE;
    virtual void clearFocusedElement() OVERRIDE;
    virtual void scrollFocusedNodeIntoRect(const WebRect&) OVERRIDE;
    virtual void zoomToFindInPageRect(const WebRect&) OVERRIDE;
    virtual void advanceFocus(bool reverse) OVERRIDE;
    virtual double zoomLevel() OVERRIDE;
    virtual double setZoomLevel(double) OVERRIDE;
    virtual void zoomLimitsChanged(double minimumZoomLevel, double maximumZoomLevel) OVERRIDE;
    virtual float textZoomFactor() OVERRIDE;
    virtual float setTextZoomFactor(float) OVERRIDE;
    virtual bool zoomToMultipleTargetsRect(const WebRect&) OVERRIDE;
    virtual void setMainFrameScrollOffset(const WebPoint&) OVERRIDE;
    virtual void setPinchViewportOffset(const WebFloatPoint&) OVERRIDE;
    virtual WebFloatPoint pinchViewportOffset() const OVERRIDE;
    virtual void resetScrollAndScaleState() OVERRIDE;
    virtual WebSize contentsPreferredMinimumSize() OVERRIDE;

    virtual float deviceScaleFactor() const OVERRIDE;
    virtual void setDeviceScaleFactor(float) OVERRIDE;

    virtual void setFixedLayoutSize(const WebSize&) OVERRIDE;

    virtual void performMediaPlayerAction(
        const WebMediaPlayerAction& action,
        const WebPoint& location) OVERRIDE;
    virtual WebHitTestResult hitTestResultAt(const WebPoint&) OVERRIDE;
    virtual void copyImageAt(const WebPoint&) OVERRIDE;
    virtual void saveImageAt(const WebPoint&) OVERRIDE;
    virtual void dragSourceSystemDragEnded() OVERRIDE;
    virtual void spellingMarkers(WebVector<uint32_t>* markers) OVERRIDE;
    virtual void removeSpellingMarkersUnderWords(const WebVector<WebString>& words) OVERRIDE;
    virtual void setCompositorDeviceScaleFactorOverride(float) OVERRIDE;
    virtual void setRootLayerTransform(const WebSize& offset, float scale) OVERRIDE;
    virtual void setSelectionColors(unsigned activeBackgroundColor,
                                    unsigned activeForegroundColor,
                                    unsigned inactiveBackgroundColor,
                                    unsigned inactiveForegroundColor) OVERRIDE;
    virtual void performCustomContextMenuAction(unsigned action) OVERRIDE;
    virtual void showContextMenu() OVERRIDE;
    virtual void extractSmartClipData(WebRect, WebString&, WebString&, WebRect&) OVERRIDE;
    virtual void addPageOverlay(WebPageOverlay*, int /* zOrder */) OVERRIDE;
    virtual void removePageOverlay(WebPageOverlay*) OVERRIDE;
    virtual void transferActiveWheelFlingAnimation(const WebActiveWheelFlingParameters&) OVERRIDE;
    virtual bool endActiveFlingAnimation() OVERRIDE;
    virtual void setShowPaintRects(bool) OVERRIDE;
    void setShowDebugBorders(bool);
    virtual void setShowFPSCounter(bool) OVERRIDE;
    virtual void setContinuousPaintingEnabled(bool) OVERRIDE;
    virtual void setShowScrollBottleneckRects(bool) OVERRIDE;
    virtual void getSelectionRootBounds(WebRect& bounds) const OVERRIDE;
    virtual void acceptLanguagesChanged() OVERRIDE;

    // WebViewImpl

    HitTestResult coreHitTestResultAt(const WebPoint&);
    void suppressInvalidations(bool enable);
    void invalidateRect(const IntRect&);

    void setIgnoreInputEvents(bool newValue);
    void setBackgroundColorOverride(WebColor);
    void setZoomFactorOverride(float);

    Color baseBackgroundColor() const { return m_baseBackgroundColor; }

    PageOverlayList* pageOverlays() const { return m_pageOverlays.get(); }

    void setOverlayLayer(GraphicsLayer*);

    const WebPoint& lastMouseDownPoint() const
    {
        return m_lastMouseDownPoint;
    }

    LocalFrame* focusedCoreFrame() const;

    // Returns the currently focused Element or null if no element has focus.
    Element* focusedElement() const;

    static WebViewImpl* fromPage(Page*);

    WebViewClient* client()
    {
        return m_client;
    }

    WebSpellCheckClient* spellCheckClient()
    {
        return m_spellCheckClient;
    }

    // Returns the page object associated with this view. This may be null when
    // the page is shutting down, but will be valid at all other times.
    Page* page() const
    {
        return m_page.get();
    }

    // Returns the main frame associated with this view. This may be null when
    // the page is shutting down, but will be valid at all other times.
    WebLocalFrameImpl* mainFrameImpl();

    // FIXME: Temporary method to accommodate out-of-process frame ancestors;
    // will be removed when there can be multiple WebWidgets for a single page.
    WebLocalFrameImpl* localFrameRootTemporary() const;

    // Event related methods:
    void mouseContextMenu(const WebMouseEvent&);
    void mouseDoubleClick(const WebMouseEvent&);

    bool detectContentOnTouch(const WebPoint&);

    void hasTouchEventHandlers(bool);

    // WebGestureCurveTarget implementation for fling.
    virtual bool scrollBy(const WebFloatSize& delta, const WebFloatSize& velocity) OVERRIDE;

    // Handles context menu events orignated via the the keyboard. These
    // include the VK_APPS virtual key and the Shift+F10 combine. Code is
    // based on the Webkit function bool WebView::handleContextMenuEvent(WPARAM
    // wParam, LPARAM lParam) in webkit\webkit\win\WebView.cpp. The only
    // significant change in this function is the code to convert from a
    // Keyboard event to the Right Mouse button down event.
    bool sendContextMenuEvent(const WebKeyboardEvent&);

    void showContextMenuAtPoint(float x, float y, PassRefPtr<ContextMenuProvider>);

    // Notifies the WebView that a load has been committed. isNewNavigation
    // will be true if a new session history item should be created for that
    // load. isNavigationWithinPage will be true if the navigation does
    // not take the user away from the current page.
    void didCommitLoad(bool isNewNavigation, bool isNavigationWithinPage);

    // Indicates two things:
    //   1) This view may have a new layout now.
    //   2) Calling layout() is a no-op.
    // After calling WebWidget::layout(), expect to get this notification
    // unless the view did not need a layout.
    void layoutUpdated(WebLocalFrameImpl*);

    void didRemoveAllPendingStylesheet(WebLocalFrameImpl*);

    bool contextMenuAllowed() const
    {
        return m_contextMenuAllowed;
    }

    void updateMainFrameLayoutSize();
    void updatePageDefinedViewportConstraints(const ViewportDescription&);

    // Returns the input event we're currently processing. This is used in some
    // cases where the WebCore DOM event doesn't have the information we need.
    static const WebInputEvent* currentInputEvent()
    {
        return m_currentInputEvent;
    }

    GraphicsLayer* rootGraphicsLayer();
    void setRootGraphicsLayer(GraphicsLayer*);
    void scheduleCompositingLayerSync();
    GraphicsLayerFactory* graphicsLayerFactory() const;
    RenderLayerCompositor* compositor() const;
    void registerForAnimations(WebLayer*);
    void scheduleAnimation();

    virtual void setVisibilityState(WebPageVisibilityState, bool) OVERRIDE;

    // Returns true if the event leads to scrolling.
    static bool mapKeyCodeForScroll(
        int keyCode,
        ScrollDirection*,
        ScrollGranularity*);

    void computeScaleAndScrollForBlockRect(const WebPoint& hitPoint, const WebRect& blockRect, float padding, float defaultScaleWhenAlreadyLegible, float& scale, WebPoint& scroll);
    Node* bestTapNode(const PlatformGestureEvent& tapEvent);
    void enableTapHighlightAtPoint(const PlatformGestureEvent& tapEvent);
    void enableTapHighlights(WillBeHeapVector<RawPtrWillBeMember<Node> >&);
    void computeScaleAndScrollForFocusedNode(Node* focusedNode, float& scale, IntPoint& scroll, bool& needAnimation);

    void animateDoubleTapZoom(const IntPoint&);

    void clearCompositedSelectionBounds();

    // Exposed for the purpose of overriding device metrics.
    void sendResizeEventAndRepaint();

    // Exposed for testing purposes.
    bool hasHorizontalScrollbar();
    bool hasVerticalScrollbar();

    // Heuristic-based function for determining if we should disable workarounds
    // for viewing websites that are not optimized for mobile devices.
    bool shouldDisableDesktopWorkarounds();

    // Exposed for tests.
    unsigned numLinkHighlights() { return m_linkHighlights.size(); }
    LinkHighlight* linkHighlight(int i) { return m_linkHighlights[i].get(); }

    WebSettingsImpl* settingsImpl();

    // Returns the bounding box of the block type node touched by the WebRect.
    WebRect computeBlockBounds(const WebRect&, bool ignoreClipping);

    IntPoint clampOffsetAtScale(const IntPoint& offset, float scale);

    // Exposed for tests.
    WebVector<WebCompositionUnderline> compositionUnderlines() const;

    bool pinchVirtualViewportEnabled() const;

    bool matchesHeuristicsForGpuRasterizationForTesting() const { return m_matchesHeuristicsForGpuRasterization; }

private:
    float legibleScale() const;
    void resumeTreeViewCommits();
    IntSize contentsSize() const;

    void resetSavedScrollAndScaleState();

    void updateMainFrameScrollPosition(const IntPoint& scrollPosition, bool programmaticScroll);

    void performResize();

    friend class WebView;  // So WebView::Create can call our constructor
    friend class WTF::RefCounted<WebViewImpl>;
    friend void setCurrentInputEventForTest(const WebInputEvent*);

    enum DragAction {
      DragEnter,
      DragOver
    };

    explicit WebViewImpl(WebViewClient*);
    virtual ~WebViewImpl();

    WebTextInputType textInputType();
    int textInputFlags();

    WebString inputModeOfFocusedElement();

    // Returns true if the event was actually processed.
    bool keyEventDefault(const WebKeyboardEvent&);

    bool confirmComposition(const WebString& text, ConfirmCompositionBehavior);

    // Returns true if the view was scrolled.
    bool scrollViewWithKeyboard(int keyCode, int modifiers);

    // Converts |pos| from window coordinates to contents coordinates and gets
    // the HitTestResult for it.
    HitTestResult hitTestResultForWindowPos(const IntPoint&);

    void setIsAcceleratedCompositingActive(bool);
    void doComposite();
    void reallocateRenderer();
    void updateLayerTreeBackgroundColor();
    void updateRootLayerTransform();
    void updateLayerTreeDeviceScaleFactor();

    // Helper function: Widens the width of |source| by the specified margins
    // while keeping it smaller than page width.
    WebRect widenRectWithinPageBounds(const WebRect& source, int targetMargin, int minimumMargin);

    // PageWidgetEventHandler functions
    virtual void handleMouseLeave(LocalFrame&, const WebMouseEvent&) OVERRIDE;
    virtual void handleMouseDown(LocalFrame&, const WebMouseEvent&) OVERRIDE;
    virtual void handleMouseUp(LocalFrame&, const WebMouseEvent&) OVERRIDE;
    virtual bool handleMouseWheel(LocalFrame&, const WebMouseWheelEvent&) OVERRIDE;
    virtual bool handleGestureEvent(const WebGestureEvent&) OVERRIDE;
    virtual bool handleKeyEvent(const WebKeyboardEvent&) OVERRIDE;
    virtual bool handleCharEvent(const WebKeyboardEvent&) OVERRIDE;

    InputMethodContext* inputMethodContext();

    WebViewClient* m_client; // Can be 0 (e.g. unittests, shared workers, etc.)
    WebSpellCheckClient* m_spellCheckClient;

    ChromeClientImpl m_chromeClientImpl;
    ContextMenuClientImpl m_contextMenuClientImpl;
    EditorClientImpl m_editorClientImpl;
    SpellCheckerClientImpl m_spellCheckerClientImpl;

    WebSize m_size;
    bool m_fixedLayoutSizeLock;

    OwnPtrWillBePersistent<Page> m_page;

    // An object that can be used to manipulate m_page->settings() without linking
    // against WebCore. This is lazily allocated the first time GetWebSettings()
    // is called.
    OwnPtr<WebSettingsImpl> m_webSettings;

    // The point relative to the client area where the mouse was last pressed
    // down. This is used by the drag client to determine what was under the
    // mouse when the drag was initiated. We need to track this here in
    // WebViewImpl since DragClient::startDrag does not pass the position the
    // mouse was at when the drag was initiated, only the current point, which
    // can be misleading as it is usually not over the element the user actually
    // dragged by the time a drag is initiated.
    WebPoint m_lastMouseDownPoint;

    // Keeps track of the current zoom level. 0 means no zoom, positive numbers
    // mean zoom in, negative numbers mean zoom out.
    double m_zoomLevel;

    double m_minimumZoomLevel;

    double m_maximumZoomLevel;

    bool m_contextMenuAllowed;

    bool m_doingDragAndDrop;

    bool m_ignoreInputEvents;

    float m_compositorDeviceScaleFactorOverride;
    WebSize m_rootLayerOffset;
    float m_rootLayerScale;

    // Webkit expects keyPress events to be suppressed if the associated keyDown
    // event was handled. Safari implements this behavior by peeking out the
    // associated WM_CHAR event if the keydown was handled. We emulate
    // this behavior by setting this flag if the keyDown was handled.
    bool m_suppressNextKeypressEvent;

    // Represents whether or not this object should process incoming IME events.
    bool m_imeAcceptEvents;

    // The available drag operations (copy, move link...) allowed by the source.
    WebDragOperation m_operationsAllowed;

    // The current drag operation as negotiated by the source and destination.
    // When not equal to DragOperationNone, the drag data can be dropped onto the
    // current drop target in this WebView (the drop target can accept the drop).
    WebDragOperation m_dragOperation;

    OwnPtr<PageOverlayList> m_pageOverlays;

    // Whether the webview is rendering transparently.
    bool m_isTransparent;

    // Whether the user can press tab to focus links.
    bool m_tabsToLinks;

    // If set, the (plugin) node which has mouse capture.
    RefPtrWillBePersistent<Node> m_mouseCaptureNode;
    RefPtr<UserGestureToken> m_mouseCaptureGestureToken;

    IntRect m_rootLayerScrollDamage;
    WebLayerTreeView* m_layerTreeView;
    WebLayer* m_rootLayer;
    GraphicsLayer* m_rootGraphicsLayer;
    GraphicsLayer* m_rootTransformLayer;
    OwnPtr<GraphicsLayerFactory> m_graphicsLayerFactory;
    bool m_isAcceleratedCompositingActive;
    bool m_layerTreeViewCommitsDeferred;
    bool m_layerTreeViewClosed;
    bool m_matchesHeuristicsForGpuRasterization;
    // If true, the graphics context is being restored.
    bool m_recreatingGraphicsContext;
    static const WebInputEvent* m_currentInputEvent;

    OwnPtr<WebActiveGestureAnimation> m_gestureAnimation;
    WebPoint m_positionOnFlingStart;
    WebPoint m_globalPositionOnFlingStart;
    int m_flingModifier;
    bool m_flingSourceDevice;
    Vector<OwnPtr<LinkHighlight> > m_linkHighlights;

    bool m_showFPSCounter;
    bool m_showPaintRects;
    bool m_showDebugBorders;
    bool m_continuousPaintingEnabled;
    bool m_showScrollBottleneckRects;
    WebColor m_baseBackgroundColor;
    WebColor m_backgroundColorOverride;
    float m_zoomFactorOverride;

    bool m_userGestureObserved;
};

// We have no ways to check if the specified WebView is an instance of
// WebViewImpl because WebViewImpl is the only implementation of WebView.
DEFINE_TYPE_CASTS(WebViewImpl, WebView, webView, true, true);

} // namespace blink

#endif
