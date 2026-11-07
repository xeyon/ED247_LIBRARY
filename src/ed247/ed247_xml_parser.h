/* -*- mode: c++; c-basic-offset: 2 -*-  */
/******************************************************************************
 * The MIT Licence
 *
 * Copyright (c) 2021 Airbus Operations S.A.S
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 *****************************************************************************/
#ifndef _ED247_XML_PARSER_H_
#define _ED247_XML_PARSER_H_
/*
 * XML parsing layer of the library.
 *
 * This module replaces the DOM provided by libXML2 by a small DOM built on top
 * of expat (a lightweight, dependency free XML pull parser).
 *
 * Contrary to libXML2:
 *  - expat is a streaming (SAX like) parser: it does not provide any DOM.
 *    The DOM used by the library is therefore built by this module while the
 *    document is parsed (see ed247_xml_parser.cpp).
 *  - expat does not support XSD/XMLSchema validation. The schema validation
 *    that was previously performed by libXML2 (ED247A_ECIC.xsd) has been
 *    removed: the validity of an ECIC file is now checked by the semantic
 *    analysis performed in ed247_xml.cpp only (unknown attributes, unknown
 *    nodes, invalid enumerated values, invalid numbers, ...).
 *  - expat does not perform any namespace processing by default: the qualified
 *    names are reported as they are written in the document. To keep the code
 *    of ed247_xml.cpp independent from that choice, this module stores the
 *    *local* name of the elements and the attributes (i.e. the name without
 *    its namespace prefix). Namespace declarations (xmlns / xmlns:prefix) are
 *    not stored as attributes.
 */
#include "ed247_logs.h" // ed247::exception
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace ed247 {
  namespace xml {

    //
    // Exception raised when an XML document cannot be parsed.
    //
    class exception : public ed247::exception
    {
    public:
      explicit exception(const std::string& message) : ed247::exception(std::string("xml parse exception: ") + message) {};
      virtual ~exception() throw () override {}
    };

    namespace dom {

      struct Node;

      //
      // An attribute of an element.
      //
      struct Attribute
      {
        std::string name;    // Local name of the attribute (i.e. without the namespace prefix)
        std::string value;   // Value of the attribute (XML entities are already decoded)
        uint32_t    line;    // Line of the attribute in the source document
        const Node* parent;  // Element owning this attribute (nullptr if unknown)
      };

      //
      // An element of the document.
      //
      // Only the elements are stored as children: the character data of an
      // element is gathered in the 'text' member and the comments/processing
      // instructions are dropped.
      //
      struct Node
      {
        std::string                        name;        // Local name of the element
        std::vector<Attribute>             attributes;  // Attributes of the element
        std::vector<std::unique_ptr<Node>> children;    // Child elements
        std::string                        text;        // Character data of the element
        uint32_t                           line;        // Line of the element in the source document
        std::shared_ptr<const std::string> source;      // Origin of the document (file path or pseudo name)

        // Return the attribute with the given local name, nullptr if it does not exist
        const Attribute* find_attribute(const std::string& attribute_name) const;
        // Return true if the element has an attribute with the given local name
        bool has_attribute(const std::string& attribute_name) const;
        // Return the value of the attribute with the given local name (empty string if it does not exist)
        std::string get_attribute(const std::string& attribute_name) const;
      };

      //
      // A parsed document.
      //
      struct Document
      {
        std::unique_ptr<Node> root; // Root element of the document
      };

    } // namespace dom

    //
    // Parse an XML file and build its DOM.
    // Throws ed247::xml::exception if the file cannot be read or is not a well formed XML document.
    //
    dom::Document parse_file(const std::string& filepath);

    //
    // Parse an XML content and build its DOM.
    // Throws ed247::xml::exception if the content is not a well formed XML document.
    // The source_name is used to locate the errors in the messages (e.g. "<content>").
    //
    dom::Document parse_content(const std::string& content, const std::string& source_name = "<content>");

  } // namespace xml
} // namespace ed247

#endif
