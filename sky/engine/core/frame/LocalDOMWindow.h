/*
 * Copyright (C) 2006, 2007, 2009, 2010 Apple Inc. All rights reserved.
 * Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies)
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef LocalDOMWindow_h
#define LocalDOMWindow_h

#include "bindings/core/v8/Dictionary.h"
#include "core/events/EventTarget.h"
#include "core/frame/DOMWindowBase64.h"
#include "core/frame/FrameDestructionObserver.h"
#include "platform/LifecycleContext.h"
#include "platform/Supplementable.h"
#include "platform/heap/Handle.h"
#include "platform/scroll/ScrollableArea.h"

#include "wtf/Forward.h"

namespace blink {

class CSSRuleList;
class CSSStyleDeclaration;
class Console;
class DOMSelection;
class DOMURL;
class DOMWindowCSS;
class DOMWindowEventQueue;
class DOMWindowLifecycleNotifier;
class DOMWindowProperty;
class Database;
class DatabaseCallback;
class Document;
class DocumentInit;
class Element;
class EventListener;
class EventQueue;
class ExceptionState;
class FloatRect;
class FrameConsole;
class History;
class IDBFactory;
class LocalFrame;
class Location;
class MediaQueryList;
class MessageEvent;
class Node;
class Page;
class RequestAnimationFrameCallback;
class ScheduledAction;
class Screen;
class ScriptCallStack;
class SerializedScriptValue;
class StyleMedia;

typedef WillBeHeapVector<RefPtrWillBeMember<MessagePort>, 1> MessagePortArray;

enum PageshowEventPersistence {
    PageshowEventNotPersisted = 0,
    PageshowEventPersisted = 1
};

enum SetLocationLocking { LockHistoryBasedOnGestureState, LockHistoryAndBackForwardList };

class LocalDOMWindow FINAL : public RefCountedWillBeGarbageCollectedFinalized<LocalDOMWindow>, public EventTargetWithInlineData, public DOMWindowBase64, public FrameDestructionObserver, public WillBeHeapSupplementable<LocalDOMWindow>, public LifecycleContext<LocalDOMWindow> {
    DEFINE_WRAPPERTYPEINFO();
    REFCOUNTED_EVENT_TARGET(LocalDOMWindow);
    WILL_BE_USING_GARBAGE_COLLECTED_MIXIN(LocalDOMWindow);
public:
    static PassRefPtrWillBeRawPtr<LocalDOMWindow> create(LocalFrame& frame)
    {
        return adoptRefWillBeNoop(new LocalDOMWindow(frame));
    }
    virtual ~LocalDOMWindow();

    PassRefPtrWillBeRawPtr<Document> installNewDocument(const DocumentInit&);

    virtual const AtomicString& interfaceName() const OVERRIDE;
    virtual ExecutionContext* executionContext() const OVERRIDE;

    virtual LocalDOMWindow* toDOMWindow() OVERRIDE;

    void registerProperty(DOMWindowProperty*);
    void unregisterProperty(DOMWindowProperty*);

    void reset();

    PassRefPtrWillBeRawPtr<MediaQueryList> matchMedia(const String&);

    unsigned pendingUnloadEventListeners() const;

    static FloatRect adjustWindowRect(LocalFrame&, const FloatRect& pendingChanges);

    // DOM Level 0

    Screen& screen() const;
    History& history() const;

    Location& location() const;
    void setLocation(const String& location, LocalDOMWindow* callingWindow, LocalDOMWindow* enteredWindow,
        SetLocationLocking = LockHistoryBasedOnGestureState);

    DOMSelection* getSelection();

    void focus(ExecutionContext* = 0);

    bool find(const String&, bool caseSensitive, bool backwards, bool wrap, bool wholeWord, bool searchInFrames, bool showDialog) const;

    int outerHeight() const;
    int outerWidth() const;
    int innerHeight() const;
    int innerWidth() const;
    int screenX() const;
    int screenY() const;
    int screenLeft() const { return screenX(); }
    int screenTop() const { return screenY(); }
    int scrollX() const;
    int scrollY() const;
    int pageXOffset() const { return scrollX(); }
    int pageYOffset() const { return scrollY(); }

    // FIXME(sky): keeping self for now since js-test.html uses it.
    LocalDOMWindow* window() const;
    LocalDOMWindow* self() const { return window(); }

    // DOM Level 2 AbstractView Interface

    Document* document() const;

    // CSSOM View Module

    StyleMedia& styleMedia() const;

    // DOM Level 2 Style Interface

    PassRefPtrWillBeRawPtr<CSSStyleDeclaration> getComputedStyle(Element*, const String& pseudoElt) const;

    // WebKit extensions

    PassRefPtrWillBeRawPtr<CSSRuleList> getMatchedCSSRules(Element*, const String& pseudoElt) const;
    double devicePixelRatio() const;

    Console& console() const;
    FrameConsole* frameConsole() const;

    void printErrorMessage(const String&);

    void scrollBy(int x, int y, ScrollBehavior = ScrollBehaviorAuto) const;
    void scrollBy(int x, int y, const Dictionary& scrollOptions, ExceptionState&) const;
    void scrollTo(int x, int y, ScrollBehavior = ScrollBehaviorAuto) const;
    void scrollTo(int x, int y, const Dictionary& scrollOptions, ExceptionState&) const;
    void scroll(int x, int y) const { scrollTo(x, y); }
    void scroll(int x, int y, const Dictionary& scrollOptions, ExceptionState& exceptionState) const { scrollTo(x, y, scrollOptions, exceptionState); }

    void moveBy(float x, float y) const;
    void moveTo(float x, float y) const;

    void resizeBy(float x, float y) const;
    void resizeTo(float width, float height) const;

    // WebKit animation extensions
    int requestAnimationFrame(PassOwnPtrWillBeRawPtr<RequestAnimationFrameCallback>);
    void cancelAnimationFrame(int id);

    DOMWindowCSS& css() const;

    // Events
    // EventTarget API
    virtual bool addEventListener(const AtomicString& eventType, PassRefPtr<EventListener>, bool useCapture = false) OVERRIDE;
    virtual bool removeEventListener(const AtomicString& eventType, PassRefPtr<EventListener>, bool useCapture = false) OVERRIDE;
    virtual void removeAllEventListeners() OVERRIDE;

    using EventTarget::dispatchEvent;
    bool dispatchEvent(PassRefPtrWillBeRawPtr<Event> prpEvent, PassRefPtrWillBeRawPtr<EventTarget> prpTarget);

    void dispatchLoadEvent();

    DEFINE_ATTRIBUTE_EVENT_LISTENER(animationend);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(animationiteration);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(animationstart);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(search);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(transitionend);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(wheel);

    // This is the interface orientation in degrees. Some examples are:
    //  0 is straight up; -90 is when the device is rotated 90 clockwise;
    //  90 is when rotated counter clockwise.
    int orientation() const;

    DEFINE_ATTRIBUTE_EVENT_LISTENER(orientationchange);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(touchstart);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(touchmove);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(touchend);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(touchcancel);

    void willDetachDocumentFromFrame();

    bool isInsecureScriptAccess(LocalDOMWindow& callingWindow, const String& urlString);

    PassOwnPtr<LifecycleNotifier<LocalDOMWindow> > createLifecycleNotifier();

    EventQueue* eventQueue() const;
    void enqueueWindowEvent(PassRefPtrWillBeRawPtr<Event>);
    void enqueueDocumentEvent(PassRefPtrWillBeRawPtr<Event>);
    void enqueuePageshowEvent(PageshowEventPersistence);
    void enqueueHashchangeEvent(const String& oldURL, const String& newURL);
    void enqueuePopstateEvent(PassRefPtr<SerializedScriptValue>);
    void dispatchWindowLoadEvent();
    void documentWasClosed();
    void statePopped(PassRefPtr<SerializedScriptValue>);

    // FIXME: This shouldn't be public once LocalDOMWindow becomes ExecutionContext.
    void clearEventQueue();

    void acceptLanguagesChanged();

    virtual void trace(Visitor*) OVERRIDE;

protected:
    DOMWindowLifecycleNotifier& lifecycleNotifier();

private:
    explicit LocalDOMWindow(LocalFrame&);

    Page* page();

    virtual void frameDestroyed() OVERRIDE;
    virtual void willDetachFrameHost() OVERRIDE;

    void clearDocument();
    void resetDOMWindowProperties();
    void willDestroyDocumentInFrame();

    // FIXME: Oilpan: the need for this internal method will fall
    // away when EventTargets are no longer using refcounts and
    // window properties are also on the heap. Inline the minimal
    // do-not-broadcast handling then and remove the enum +
    // removeAllEventListenersInternal().
    enum BroadcastListenerRemoval {
        DoNotBroadcastListenerRemoval,
        DoBroadcastListenerRemoval
    };

    void removeAllEventListenersInternal(BroadcastListenerRemoval);

    RefPtrWillBeMember<Document> m_document;

#if ENABLE(ASSERT)
    bool m_hasBeenReset;
#endif

    HashSet<DOMWindowProperty*> m_properties;

    mutable RefPtrWillBeMember<Screen> m_screen;
    mutable RefPtrWillBeMember<History> m_history;
    mutable RefPtrWillBeMember<Console> m_console;
    mutable RefPtrWillBeMember<Location> m_location;
    mutable RefPtrWillBeMember<StyleMedia> m_media;

    mutable RefPtrWillBeMember<DOMWindowCSS> m_css;

    RefPtrWillBeMember<DOMWindowEventQueue> m_eventQueue;
    RefPtr<SerializedScriptValue> m_pendingStateObject;
};

} // namespace blink

#endif // LocalDOMWindow_h
