/*
 * Copyright (C) 2006, 2007, 2008, 2009, 2010, 2013 Apple Inc. All rights reserved.
 * Copyright (C) 2008 Torch Mobile Inc. All rights reserved. (http://www.torchmobile.com/)
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#ifndef Page_h
#define Page_h

#include "core/dom/ViewportDescription.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/SettingsDelegate.h"
#include "core/frame/UseCounter.h"
#include "core/page/PageAnimator.h"
#include "core/page/PageVisibilityState.h"
#include "platform/LifecycleContext.h"
#include "platform/Supplementable.h"
#include "platform/geometry/LayoutRect.h"
#include "platform/geometry/Region.h"
#include "platform/heap/Handle.h"
#include "wtf/Forward.h"
#include "wtf/HashSet.h"
#include "wtf/Noncopyable.h"
#include "wtf/text/WTFString.h"

namespace blink {

class AutoscrollController;
class Chrome;
class ChromeClient;
class ClientRectList;
class ContextMenuClient;
class ContextMenuController;
class Document;
class DragCaretController;
class EditorClient;
class FocusController;
class Frame;
class FrameHost;
class PageLifecycleNotifier;
class PlatformMouseEvent;
class Range;
class RenderBox;
class RenderObject;
class VisibleSelection;
class ScrollableArea;
class ScrollingCoordinator;
class Settings;
class SpellCheckerClient;
class UndoStack;

typedef uint64_t LinkHash;

float deviceScaleFactor(LocalFrame*);

class Page FINAL : public NoBaseWillBeGarbageCollectedFinalized<Page>, public WillBeHeapSupplementable<Page>, public LifecycleContext<Page>, public SettingsDelegate {
    WILL_BE_USING_GARBAGE_COLLECTED_MIXIN(Page);
    WTF_MAKE_NONCOPYABLE(Page);
    friend class Settings;
public:
    static void scheduleForcedStyleRecalcForAllPages();

    // It is up to the platform to ensure that non-null clients are provided where required.
    struct PageClients {
        WTF_MAKE_NONCOPYABLE(PageClients); WTF_MAKE_FAST_ALLOCATED;
    public:
        PageClients();
        ~PageClients();

        ChromeClient* chromeClient;
        ContextMenuClient* contextMenuClient;
        EditorClient* editorClient;
        SpellCheckerClient* spellCheckerClient;
    };

    explicit Page(PageClients&);
    virtual ~Page();

    void makeOrdinary();

    // This method returns all pages, incl. private ones associated with
    // inspector overlay, popups, SVGImage, etc.
    static HashSet<Page*>& allPages();
    // This method returns all ordinary pages.
    static HashSet<Page*>& ordinaryPages();

    FrameHost& frameHost() const { return *m_frameHost; }

    void setNeedsRecalcStyleInAllFrames();
    void updateAcceleratedCompositingSettings();

    ViewportDescription viewportDescription() const;

    EditorClient& editorClient() const { return *m_editorClient; }
    SpellCheckerClient& spellCheckerClient() const { return *m_spellCheckerClient; }
    UndoStack& undoStack() const { return *m_undoStack; }

    void setMainFrame(LocalFrame*);
    LocalFrame* mainFrame() const { return m_mainFrame; }

    void documentDetached(Document*);

    bool openedByDOM() const;
    void setOpenedByDOM();

    PageAnimator& animator() { return m_animator; }
    Chrome& chrome() const { return *m_chrome; }
    AutoscrollController& autoscrollController() const { return *m_autoscrollController; }
    DragCaretController& dragCaretController() const { return *m_dragCaretController; }
    FocusController& focusController() const { return *m_focusController; }
    ContextMenuController& contextMenuController() const { return *m_contextMenuController; }

    ScrollingCoordinator* scrollingCoordinator();

    String mainThreadScrollingReasonsAsText();
    PassRefPtrWillBeRawPtr<ClientRectList> nonFastScrollableRects(const LocalFrame*);

    Settings& settings() const { return *m_settings; }

    UseCounter& useCounter() { return m_useCounter; }

    void setTabKeyCyclesThroughElements(bool b) { m_tabKeyCyclesThroughElements = b; }
    bool tabKeyCyclesThroughElements() const { return m_tabKeyCyclesThroughElements; }

    void unmarkAllTextMatches();

    float deviceScaleFactor() const { return m_deviceScaleFactor; }
    void setDeviceScaleFactor(float);

    PageVisibilityState visibilityState() const;
    void setVisibilityState(PageVisibilityState, bool);

    bool isCursorVisible() const;
    void setIsCursorVisible(bool isVisible) { m_isCursorVisible = isVisible; }

#if ENABLE(ASSERT)
    void setIsPainting(bool painting) { m_isPainting = painting; }
    bool isPainting() const { return m_isPainting; }
#endif

    double timerAlignmentInterval() const;

    class MultisamplingChangedObserver : public WillBeGarbageCollectedMixin {
    public:
        virtual void multisamplingChanged(bool) = 0;
    };

    void addMultisamplingChangedObserver(MultisamplingChangedObserver*);
    void removeMultisamplingChangedObserver(MultisamplingChangedObserver*);

    void didCommitLoad(LocalFrame*);

    void acceptLanguagesChanged();

    static void networkStateChanged(bool online);
    PassOwnPtr<LifecycleNotifier<Page> > createLifecycleNotifier();

    void trace(Visitor*);
    void willBeDestroyed();

protected:
    PageLifecycleNotifier& lifecycleNotifier();

private:
    void initGroup();

    void setTimerAlignmentInterval(double);

    void setNeedsLayoutInAllFrames();

    // SettingsDelegate overrides.
    virtual void settingsChanged(SettingsDelegate::ChangeType) OVERRIDE;

    PageAnimator m_animator;
    const OwnPtr<AutoscrollController> m_autoscrollController;
    const OwnPtr<Chrome> m_chrome;
    const OwnPtrWillBeMember<DragCaretController> m_dragCaretController;
    const OwnPtr<FocusController> m_focusController;
    const OwnPtrWillBeMember<ContextMenuController> m_contextMenuController;
    OwnPtr<ScrollingCoordinator> m_scrollingCoordinator;
    const OwnPtrWillBeMember<UndoStack> m_undoStack;

    // Typically, the main frame and Page should both be owned by the embedder,
    // which must call Page::willBeDestroyed() prior to destroying Page. This
    // call detaches the main frame and clears this pointer, thus ensuring that
    // this field only references a live main frame.
    //
    // However, there are several locations (InspectorOverlay)
    // which don't hold a reference to the main frame at all
    // after creating it. These are still safe because they always create a
    // Frame with a FrameView. FrameView and Frame hold references to each
    // other, thus keeping each other alive. The call to willBeDestroyed()
    // breaks this cycle, so the frame is still properly destroyed once no
    // longer needed.
    LocalFrame* m_mainFrame;

    EditorClient* const m_editorClient;
    SpellCheckerClient* const m_spellCheckerClient;

    UseCounter m_useCounter;

    bool m_openedByDOM;

    bool m_tabKeyCyclesThroughElements;

    float m_deviceScaleFactor;

    double m_timerAlignmentInterval;

    PageVisibilityState m_visibilityState;

    bool m_isCursorVisible;

#if ENABLE(ASSERT)
    bool m_isPainting;
#endif

    WillBeHeapHashSet<RawPtrWillBeWeakMember<MultisamplingChangedObserver> > m_multisamplingChangedObservers;

    // A pointer to all the interfaces provided to in-process Frames for this Page.
    // FIXME: Most of the members of Page should move onto FrameHost.
    OwnPtrWillBeMember<FrameHost> m_frameHost;
};

} // namespace blink

#endif // Page_h
