/*
 * Copyright (C) 2006, 2007, 2008, 2009 Apple Inc. All rights reserved.
 * Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies).
 * Copyright (C) 2012, Samsung Electronics. All rights reserved.
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

#ifndef Chrome_h
#define Chrome_h

#include "core/loader/NavigationPolicy.h"
#include "core/page/FocusType.h"
#include "platform/Cursor.h"
#include "platform/HostWindow.h"
#include "wtf/Forward.h"

namespace blink {

class ChromeClient;
class ColorChooser;
class ColorChooserClient;
class DateTimeChooser;
class DateTimeChooserClient;
class FloatRect;
class LocalFrame;
class HitTestResult;
class IntRect;
class Node;
class Page;

struct DateTimeChooserParameters;
struct ViewportDescription;

class Chrome FINAL : public HostWindow {
public:
    virtual ~Chrome();

    static PassOwnPtr<Chrome> create(Page*, ChromeClient*);

    ChromeClient& client() { return *m_client; }

    // HostWindow methods.
    virtual void invalidateContentsAndRootView(const IntRect&) OVERRIDE;
    virtual void invalidateContentsForSlowScroll(const IntRect&) OVERRIDE;
    virtual IntRect rootViewToScreen(const IntRect&) const OVERRIDE;
    virtual blink::WebScreenInfo screenInfo() const OVERRIDE;

    virtual void scheduleAnimation() OVERRIDE;

    void setCursor(const Cursor&);

    void setWindowRect(const FloatRect&) const;
    FloatRect windowRect() const;

    FloatRect pageRect() const;

    void focus() const;

    bool canTakeFocus(FocusType) const;
    void takeFocus(FocusType) const;

    void focusedNodeChanged(Node*) const;

    void show(NavigationPolicy = NavigationPolicyIgnore) const;

    void mouseDidMoveOverElement(const HitTestResult&, unsigned modifierFlags);

    void setToolTip(const HitTestResult&);

    void dispatchViewportPropertiesDidChange(const ViewportDescription&) const;

    void willBeDestroyed();

private:
    Chrome(Page*, ChromeClient*);

    Page* m_page;
    ChromeClient* m_client;
};

}

#endif // Chrome_h
