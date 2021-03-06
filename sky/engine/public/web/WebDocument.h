/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
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

#ifndef WebDocument_h
#define WebDocument_h

#include "../platform/WebReferrerPolicy.h"
#include "../platform/WebVector.h"
#include "WebExceptionCode.h"
#include "WebFrame.h"
#include "WebNode.h"

#if BLINK_IMPLEMENTATION
namespace WTF { template <typename T> class PassRefPtr; }
#endif

namespace v8 {
class Value;
template <class T> class Handle;
}

namespace blink {

class Document;
class DocumentType;
class WebAXObject;
class WebDocumentType;
class WebElement;
class WebString;
class WebURL;

// Provides readonly access to some properties of a DOM document.
class WebDocument : public WebNode {
public:
    WebDocument() { }
    WebDocument(const WebDocument& e) : WebNode(e) { }

    WebDocument& operator=(const WebDocument& e)
    {
        WebNode::assign(e);
        return *this;
    }
    void assign(const WebDocument& e) { WebNode::assign(e); }

    BLINK_EXPORT WebURL url() const;

    BLINK_EXPORT WebString encoding() const;
    BLINK_EXPORT WebString contentLanguage() const;
    BLINK_EXPORT WebString referrer() const;
    // The url of the OpenSearch Desription Document (if any).
    BLINK_EXPORT WebURL openSearchDescriptionURL() const;

    // Returns the frame the document belongs to or 0 if the document is frameless.
    BLINK_EXPORT WebLocalFrame* frame() const;
    BLINK_EXPORT bool isHTMLDocument() const;

    BLINK_EXPORT WebElement documentElement() const;
    BLINK_EXPORT WebString title() const;
    BLINK_EXPORT WebURL completeURL(const WebString&) const;
    BLINK_EXPORT WebElement getElementById(const WebString&) const;
    BLINK_EXPORT WebElement focusedElement() const;
    BLINK_EXPORT WebDocumentType doctype() const;
    BLINK_EXPORT WebDOMEvent createEvent(const WebString& eventType);
    BLINK_EXPORT WebReferrerPolicy referrerPolicy() const;
    BLINK_EXPORT WebElement createElement(const WebString& tagName);
    // Shorthand of frame()->scrollOffset().
    BLINK_EXPORT WebSize scrollOffset() const;
    // Shorthand of frame()->minimumScrollOffset().
    BLINK_EXPORT WebSize minimumScrollOffset() const;
    // Shorthand of frame()->maximumScrollOffset().
    BLINK_EXPORT WebSize maximumScrollOffset() const;

    BLINK_EXPORT v8::Handle<v8::Value> registerEmbedderCustomElement(const WebString& name, v8::Handle<v8::Value> options, WebExceptionCode&);

#if BLINK_IMPLEMENTATION
    WebDocument(const PassRefPtrWillBeRawPtr<Document>&);
    WebDocument& operator=(const PassRefPtrWillBeRawPtr<Document>&);
    operator PassRefPtrWillBeRawPtr<Document>() const;
#endif
};

} // namespace blink

#endif
