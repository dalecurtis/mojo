/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2003, 2008, 2010 Apple Inc. All rights reserved.
 * Copyright (C) 2011 Google Inc. All rights reserved.
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
 *
 */

#ifndef HTMLLinkElement_h
#define HTMLLinkElement_h

#include "core/css/CSSStyleSheet.h"
#include "core/dom/DOMSettableTokenList.h"
#include "core/fetch/ResourceOwner.h"
#include "core/html/HTMLElement.h"
#include "core/html/LinkRelAttribute.h"
#include "core/html/LinkResource.h"

namespace blink {

class DocumentFragment;
class HTMLLinkElement;
class KURL;
class LinkImport;

template<typename T> class EventSender;
typedef EventSender<HTMLLinkElement> LinkEventSender;

class HTMLLinkElement FINAL : public HTMLElement {
    DEFINE_WRAPPERTYPEINFO();
public:
    static PassRefPtrWillBeRawPtr<HTMLLinkElement> create(Document&, bool createdByParser);
    virtual ~HTMLLinkElement();

    KURL href() const;
    const AtomicString& rel() const;
    String media() const { return m_media; }
    String typeValue() const { return m_type; }
    const LinkRelAttribute& relAttribute() const { return m_relAttribute; }

    const AtomicString& type() const;

    // the icon sizes as parsed from the HTML attribute
    const Vector<IntSize>& iconSizes() const;

    bool async() const;
    String as() const;

    CSSStyleSheet* sheet() const { return 0; }
    Document* import() const;

    bool isImport() const { return linkImport(); }
    bool isDisabled() const { return false; }

    DOMSettableTokenList* sizes() const;

    void dispatchPendingEvent(LinkEventSender*);
    void scheduleEvent();
    void dispatchEventImmediately();
    static void dispatchPendingLoadEvents();

    // For LinkStyle
    bool shouldProcessStyle() { return false; }
    bool isCreatedByParser() const { return m_createdByParser; }

    // Parse the icon size attribute into |iconSizes|, make this method public
    // visible for testing purpose.
    static void parseSizesAttribute(const AtomicString& value, Vector<IntSize>& iconSizes);

    virtual void trace(Visitor*) OVERRIDE;

private:
    virtual void parseAttribute(const QualifiedName&, const AtomicString&) OVERRIDE;

    LinkImport* linkImport() const;
    LinkResource* linkResourceToProcess();

    void process();
    static void processCallback(Node*);

    // From Node and subclassses
    virtual InsertionNotificationRequest insertedInto(ContainerNode*) OVERRIDE;
    virtual void removedFrom(ContainerNode*) OVERRIDE;
    virtual bool isURLAttribute(const Attribute&) const OVERRIDE;
    virtual bool hasLegalLinkAttribute(const QualifiedName&) const OVERRIDE;
    virtual const QualifiedName& subResourceAttributeName() const OVERRIDE;
    virtual void finishParsingChildren() OVERRIDE;

private:
    HTMLLinkElement(Document&, bool createdByParser);

    OwnPtrWillBeMember<LinkResource> m_link;

    String m_type;
    String m_media;
    RefPtrWillBeMember<DOMSettableTokenList> m_sizes;
    Vector<IntSize> m_iconSizes;
    LinkRelAttribute m_relAttribute;

    bool m_createdByParser;
    bool m_isInShadowTree;
};

} // namespace blink

#endif // HTMLLinkElement_h
