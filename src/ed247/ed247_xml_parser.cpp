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
#include "ed247_xml_parser.h"
#include <expat.h>
#include <fstream>
#include <limits>
#include <string.h>
#include <vector>

namespace ed247 {
  namespace xml {

    namespace dom {

      const Attribute* Node::find_attribute(const std::string& attribute_name) const
      {
        for (const auto& attribute : attributes) {
          if (attribute.name == attribute_name) {
            return &attribute;
          }
        }
        return nullptr;
      }

      bool Node::has_attribute(const std::string& attribute_name) const
      {
        return find_attribute(attribute_name) != nullptr;
      }

      std::string Node::get_attribute(const std::string& attribute_name) const
      {
        const Attribute* attribute = find_attribute(attribute_name);
        return attribute != nullptr ? attribute->value : std::string();
      }

    } // namespace dom

    namespace {

      //
      // Size of the buffer used to read the files (the file is not loaded in memory at once)
      //
      static const size_t FILE_BUFFER_SIZE = 64 * 1024;

      //
      // Return the local name of a qualified name (i.e. the name without its namespace prefix).
      // Expat does not perform any namespace processing: it reports the names as they are
      // written in the document (e.g. "xs:element" or "xsi:noNamespaceSchemaLocation").
      //
      std::string local_name(const char* qualified_name)
      {
        if (qualified_name == nullptr) {
          return std::string();
        }
        const char* separator = strchr(qualified_name, ':');
        return separator != nullptr ? std::string(separator + 1) : std::string(qualified_name);
      }

      //
      // Return true if the qualified name is a namespace declaration (xmlns or xmlns:prefix).
      // Such attributes are not part of the content of the document: they are ignored.
      //
      bool is_namespace_declaration(const char* qualified_name)
      {
        if (qualified_name == nullptr) {
          return false;
        }
        if (strncmp(qualified_name, "xmlns", 5) == 0) {
          return qualified_name[5] == '\0' || qualified_name[5] == ':';
        }
        return false;
      }

      //
      // State of the parsing of a document.
      //
      // The expat parser is a pull parser: it calls the handlers below while the document
      // is fed. The DOM is built on the fly by these handlers.
      //
      class Parser
      {
      public:
        explicit Parser(const std::string& source_name) :
          _parser(XML_ParserCreate(nullptr)),
          _source(new std::string(source_name))
        {
          if (_parser == nullptr) {
            throw exception(std::string("Failed to create the XML parser for [") + source_name + "]");
          }
          // Give the access to this object to the handlers
          XML_SetUserData(_parser, this);
          XML_SetElementHandler(_parser, &Parser::start_element, &Parser::end_element);
          XML_SetCharacterDataHandler(_parser, &Parser::character_data);
        }

        ~Parser()
        {
          if (_parser != nullptr) {
            XML_ParserFree(_parser);
          }
        }

        Parser(const Parser&) = delete;
        Parser& operator=(const Parser&) = delete;

        //
        // Feed the parser with a chunk of the document.
        // The last chunk must be given with final = true.
        //
        void parse(const char* data, int length, bool final)
        {
          if (XML_Parse(_parser, data, length, final ? 1 : 0) == XML_STATUS_ERROR) {
            throw_error();
          }
        }

        //
        // Return the DOM built while parsing the document.
        //
        dom::Document document()
        {
          dom::Document document;
          document.root = std::move(_root);
          return document;
        }

      private:
        //
        // Called by expat when an element is opened.
        //
        static void XMLCALL start_element(void* user_data, const XML_Char* name, const XML_Char** attributes)
        {
          Parser* parser = static_cast<Parser*>(user_data);

          std::unique_ptr<dom::Node> node(new dom::Node());
          node->name  = local_name(name);
          node->line  = static_cast<uint32_t>(XML_GetCurrentLineNumber(parser->_parser));
          node->source = parser->_source;

          // Copy the attributes of the element, ignoring the namespace declarations
          for (int attribute_id = 0; attributes[attribute_id] != nullptr; attribute_id += 2) {
            if (is_namespace_declaration(attributes[attribute_id])) {
              continue;
            }
            dom::Attribute attribute;
            attribute.name   = local_name(attributes[attribute_id]);
            attribute.value  = attributes[attribute_id + 1] != nullptr ? attributes[attribute_id + 1] : "";
            attribute.line   = node->line;
            attribute.parent = node.get();
            node->attributes.push_back(attribute);
          }

          // Attach the element to its parent (or make it the root of the document)
          dom::Node* node_ptr = nullptr;
          if (parser->_stack.empty()) {
            parser->_root = std::move(node);
            node_ptr = parser->_root.get();
          } else {
            parser->_stack.back()->children.push_back(std::move(node));
            node_ptr = parser->_stack.back()->children.back().get();
          }

          // Push the element on the stack of the opened elements (the tree owns the elements)
          parser->_stack.push_back(node_ptr);
        }

        //
        // Called by expat when an element is closed.
        //
        static void XMLCALL end_element(void* user_data, const XML_Char* /*name*/)
        {
          Parser* parser = static_cast<Parser*>(user_data);
          if (!parser->_stack.empty()) {
            parser->_stack.pop_back();
          }
        }

        //
        // Called by expat for the character data (text) of an element.
        //
        static void XMLCALL character_data(void* user_data, const XML_Char* text, int length)
        {
          Parser* parser = static_cast<Parser*>(user_data);
          if (parser->_stack.empty()) {
            // Character data outside of the root element (e.g. spaces between the XML declaration and the root element)
            return;
          }
          parser->_stack.back()->text.append(text, length);
        }

        //
        // Build the error message of the last error reported by expat and throw it.
        //
        [[noreturn]] void throw_error() const
        {
          const XML_Error code = XML_GetErrorCode(_parser);
          const long     line = XML_GetCurrentLineNumber(_parser);
          const long     column = XML_GetCurrentColumnNumber(_parser);
          std::string message = "Failed to parse XML document [";
          message += *_source;
          message += "] at line ";
          message += strize() << line;
          message += ", column ";
          message += strize() << column;
          message += ": ";
          message += XML_ErrorString(code);
          PRINT_ERROR(message);
          throw exception(message);
        }

        XML_Parser                      _parser; // expat parser
        std::shared_ptr<std::string>    _source; // Origin of the document (shared by all the nodes)
        std::unique_ptr<dom::Node>      _root;   // Root element of the document
        std::vector<dom::Node*>         _stack;  // Elements currently opened (not owned: owned by the tree)
      };

    } // anonymous namespace

    //
    // Parse an XML file and build its DOM.
    //
    dom::Document parse_file(const std::string& filepath)
    {
      std::ifstream file(filepath.c_str(), std::ios::in | std::ios::binary);
      if (!file.is_open()) {
        std::string message = "Failed to open XML file [" + filepath + "]";
        PRINT_ERROR(message);
        throw exception(message);
      }

      Parser parser(filepath);

      // Feed the parser chunk by chunk to avoid loading the whole file in memory
      std::vector<char> buffer(FILE_BUFFER_SIZE);
      while (file.good()) {
        file.read(&buffer[0], static_cast<std::streamsize>(buffer.size()));
        const std::streamsize count = file.gcount();
        if (count > 0) {
          parser.parse(&buffer[0], static_cast<int>(count), false);
        }
        if (count == 0) {
          break; // End of file
        }
      }

      // Tell expat that the document is complete
      parser.parse("", 0, true);

      return parser.document();
    }

    //
    // Parse an XML content and build its DOM.
    //
    dom::Document parse_content(const std::string& content, const std::string& source_name)
    {
      if (content.size() > static_cast<size_t>(std::numeric_limits<int>::max())) {
        throw exception("XML content [" + source_name + "] is too big to be parsed");
      }
      Parser parser(source_name);
      parser.parse(content.c_str(), static_cast<int>(content.size()), true);
      return parser.document();
    }

  } // namespace xml
} // namespace ed247
