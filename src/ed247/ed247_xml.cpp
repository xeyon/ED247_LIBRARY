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
#include "ed247_xml.h"
#include "ed247_logs.h"
#include <algorithm>
#include <sstream>

/*
 * ECIC Nodes and attributes
 */
namespace ed247 {
  namespace xml {
    namespace node {
      static const std::string DataTimestamp { "DataTimestamp" };
      static const std::string FileProducer { "FileProducer" };
      static const std::string UdpSockets { "UDP_Sockets" };
      static const std::string UdpSocket { "UDP_Socket" };
      static const std::string FrameFormat { "FrameFormat" };
      static const std::string Frame { "Frame" };
      static const std::string FrameSize { "FrameSize" };
      static const std::string ComInterface { "ComInterface" };
      static const std::string Header { "Header" };
      static const std::string SampleTimestampOffset { "SampleTimestampOffset" };
      static const std::string Errors { "Errors" };
      static const std::string Signal { "Signal" };
      static const std::string Signals { "Signals" };
      static const std::string A429_Stream { "A429_Stream" };
      static const std::string A664_Stream { "A664_Stream" };
      static const std::string MessageSize { "MessageSize" };
      static const std::string A825_Stream { "A825_Stream" };
      static const std::string SERIAL_Stream { "SERIAL_Stream" };
      static const std::string ETH_Stream { "ETH_Stream" };
      static const std::string DIS_Stream { "DIS_Stream" };
      static const std::string ANA_Stream { "ANA_Stream" };
      static const std::string NAD_Stream { "NAD_Stream" };
      static const std::string VNAD_Stream { "VNAD_Stream" };
      static const std::string Streams { "Streams" };
      static const std::string Stream { "Stream" };
      static const std::string MultiChannel { "MultiChannel" };
      static const std::string Channel { "Channel" };
      static const std::string Channels { "Channels" };
      static const std::string ED247ComponentInstanceConfiguration { "ED247ComponentInstanceConfiguration" };
    }

    namespace attr {
      static const std::string Name { "Name" };
      static const std::string Direction { "Direction" };
      static const std::string ComponentVersion { "ComponentVersion" };
      static const std::string Comment { "Comment" };
      static const std::string Identifier { "Identifier" };
      static const std::string DstIP { "DstIP" };
      static const std::string DstPort { "DstPort" };
      static const std::string SrcIP { "SrcIP" };
      static const std::string SrcPort { "SrcPort" };
      static const std::string MulticastInterfaceIP { "MulticastInterfaceIP" };
      static const std::string MulticastTTL { "MulticastTTL" };
      static const std::string TransportTimestamp { "TransportTimestamp" };
      static const std::string SampleMaxNumber { "SampleMaxNumber" };
      static const std::string SampleMaxSizeBytes { "SampleMaxSizeBytes" };
      static const std::string Position { "Position" };
      static const std::string Enable { "Enable" };
      static const std::string UID { "UID" };
      static const std::string ICD { "ICD" };
      static const std::string FifoSize { "FifoSize" };
      static const std::string ByteOffset { "ByteOffset" };
      static const std::string ComponentType { "ComponentType" };
      static const std::string StandardRevision { "StandardRevision" };
      static const std::string SamplingPeriodUs { "SamplingPeriodUs" };
      static const std::string Type { "Type" };
      static const std::string MaxNumber { "MaxNumber" };
      static const std::string MaxLength { "MaxLength" };  // Backward compatibility
      static const std::string Unit { "Unit" };
      static const std::string SampleDataTimestampOffset { "SampleDataTimestampOffset" };
      static const std::string ElectricalUnit { "ElectricalUnit" };
      static const std::string Dimensions { "Dimensions" };
      static const std::string Layer { "Layer" };
    }
  }
}


/*
 * XML helpers
 *
 * Note: these helpers used to wrap the libXML2 API (xmlGetProp, xmlGetProp...).
 * They now work on the DOM built by the expat based parser (see ed247_xml_parser.h):
 * the elements and their attributes are directly available through the node.
 */
namespace ed247 {
  namespace xml {

    //
    // Return the location of a node in the source document (e.g. "my_ecic.xml:42")
    //
    std::string xmlNode_get_fileline(node_ptr_t node)
    {
      if (node == nullptr) {
        return std::string();
      }
      const std::string source = node->source ? *node->source : std::string("<xml>");
      return strize() << source << ":" << node->line;
    }

    //
    // Return the value of an attribute of a node (empty string if the attribute does not exist)
    //
    std::string xml_get_prop(node_ptr_t node, const std::string& name)
    {
      if (node == nullptr) {
        return std::string();
      }
      return node->get_attribute(name);
    }

    //
    // Throw an exception about an attribute holding an invalid value.
    //
    [[noreturn]] void xmlAttr_throw_error(const dom::Attribute& attribute, const std::string& reason)
    {
      std::string message = strize() << "Invalid value [" << attribute.value << "] for attribute [" << attribute.name << "]";
      if (attribute.parent != nullptr) {
        message += strize() << " of element [" << attribute.parent->name << "]";
      }
      message += ": " + reason;
      if (attribute.parent != nullptr) {
        message += std::string(". Near ") + xmlNode_get_fileline(attribute.parent);
      }
      PRINT_ERROR(message);
      throw exception(message);
    }

    //
    // Extract the value of an attribute into a numeric variable.
    //
    // Note: the validity of the value is checked here. This check replaces the one that was
    // previously performed by the XSD validation of the ECIC file (performed by libXML2).
    //
    template<typename T>
    void xmlAttr_get_value(const dom::Attribute& attribute, T& variable)
    {
      std::istringstream stream(attribute.value);
      stream >> variable;
      if (stream.fail()) {
        xmlAttr_throw_error(attribute, "is not a valid number");
      }
      // Ensure the whole value has been consumed (e.g. reject "12abc")
      char remaining = 0;
      if (stream >> remaining) {
        xmlAttr_throw_error(attribute, "is not a valid number");
      }
    }

    template<>
    void xmlAttr_get_value(const dom::Attribute& attribute, std::string& variable)
    {
      variable = attribute.value;
    }

    //
    // Extract the value of an attribute holding an enumerated value.
    // The value is checked here (an invalid value is rejected).
    //
    template<>
    void xmlAttr_get_value(const dom::Attribute& attribute, ed247_component_type_t& variable)
    {
      variable = ed247_component_type_from_string(attribute.value.c_str());
      if (variable == ED247_COMPONENT_TYPE__INVALID) {
        xmlAttr_throw_error(attribute, "is not a valid component type (expected Virtual or Bridge)");
      }
    }

    template<>
    void xmlAttr_get_value(const dom::Attribute& attribute, ed247_standard_t& variable)
    {
      variable = ed247_standard_from_string(attribute.value.c_str());
      if (variable == ED247_STANDARD__INVALID) {
        xmlAttr_throw_error(attribute, "is not a valid standard revision");
      }
    }

    template<>
    void xmlAttr_get_value(const dom::Attribute& attribute, ed247_yesno_t& variable)
    {
      variable = ed247_yesno_from_string(attribute.value.c_str());
      if (variable == ED247_YESNO__INVALID) {
        xmlAttr_throw_error(attribute, "is not a valid yes/no value (expected Yes or No)");
      }
    }

    template<>
    void xmlAttr_get_value(const dom::Attribute& attribute, ed247_direction_t& variable)
    {
      variable = ed247_direction_from_string(attribute.value.c_str());
      if (variable == ED247_DIRECTION__INVALID) {
        xmlAttr_throw_error(attribute, "is not a valid direction (expected In, Out or InOut)");
      }
    }

    template<>
    void xmlAttr_get_value(const dom::Attribute& attribute, ed247_nad_type_t& variable)
    {
      variable = ed247_nad_type_from_string(attribute.value.c_str());
      if (variable == ED247_NAD_TYPE__INVALID) {
        xmlAttr_throw_error(attribute, "is not a valid NAD type");
      }
    }

    template<> void xmlAttr_get_value(const dom::Attribute& attribute, const char * & variable) = delete;
    template<> void xmlAttr_get_value(const dom::Attribute& attribute, char * & variable) = delete;

    //
    // Return true if the given string is a valid IPv4 address (e.g. "192.168.0.1")
    //
    // Note: the format of the IP addresses used to be checked by the XSD validation of the
    // ECIC file (performed by libXML2). This check replaces it.
    //
    bool xml_is_ip_address(const std::string& address)
    {
      size_t begin = 0;
      for (int octet_id = 0 ; octet_id < 4 ; octet_id++) {
        const size_t end = address.find('.', begin);
        const std::string octet = address.substr(begin, end == std::string::npos ? std::string::npos : end - begin);
        if (octet.empty() || octet.size() > 3) {
          return false;
        }
        for (char digit : octet) {
          if (digit < '0' || digit > '9') {
            return false;
          }
        }
        if (std::stoi(octet) > 255) {
          return false;
        }
        if (end == std::string::npos) {
          return octet_id == 3; // The last octet shall be the fourth one
        }
        begin = end + 1;
      }
      return false; // Too many octets
    }

  }
}

/*
 * Error handling
 *
 * Note: the exception raised when an XML document cannot be parsed is defined
 * in ed247_xml_parser.h (it is shared by the parser and the semantic analysis below).
 */


#define PARSER_WARNING(closest_node, msg)                                                                        \
  do {                                                                                                           \
    std::string message = strize() << msg;                                                                       \
    if (closest_node != nullptr) {                                                                               \
      message += std::string(". Near ") + ::ed247::xml::xmlNode_get_fileline(closest_node);                      \
    }                                                                                                            \
    PRINT_WARNING(message);                                                                                      \
  } while (0)

#define THROW_PARSER_ERROR(closest_node, msg)                                                                    \
  do {                                                                                                           \
    std::string message = strize() << msg;                                                                       \
    if (closest_node != nullptr) {                                                                               \
      message += std::string(". Near ") + ::ed247::xml::xmlNode_get_fileline(closest_node);                      \
    }                                                                                                            \
    PRINT_ERROR(message);                                                                                        \
    throw ed247::xml::exception(message);                                                                        \
  } while (0)

//
// DataTimestamp
//
ed247::xml::DataTimestamp::DataTimestamp() :
  _enable(ED247_YESNO_NO),
  _enable_sample_offset(ED247_YESNO_NO)
{
}

void ed247::xml::DataTimestamp::load(node_ptr_t xml_node)
{
  for(const auto& xml_attr : xml_node->attributes){
    if(xml_attr.name == attr::Enable){
      ::ed247::xml::xmlAttr_get_value(xml_attr, _enable);
    }else if(xml_attr.name == attr::SampleDataTimestampOffset){
      ::ed247::xml::xmlAttr_get_value(xml_attr, _enable_sample_offset);
    }else{
      THROW_PARSER_ERROR(xml_node, "Unknown attribute [" << xml_attr.name << "] in tag [" << node::DataTimestamp << "]");
    }
  }
}

//
// Errors
//
ed247::xml::Errors::Errors() :
  _enable(ED247_YESNO_NO)
{
}

void ed247::xml::Errors::load(node_ptr_t xml_node)
{
  for(const auto& xml_attr : xml_node->attributes){
    if(xml_attr.name == attr::Enable){
      ::ed247::xml::xmlAttr_get_value(xml_attr, _enable);
    }else{
      THROW_PARSER_ERROR(xml_node, "Unknown attribute [" << xml_attr.name << "] in tag [" << node::Errors << "]");
    }
  }
}

//
// UdpSocket
//
ed247::xml::UdpSocket::UdpSocket() :
  _dst_ip_port(0),
  _src_ip_port(0),
  _mc_ttl(1),
  _direction(ED247_DIRECTION__INVALID)
{
}

void ed247::xml::UdpSocket::load(node_ptr_t xml_node)
{
  for(const auto& xml_attr : xml_node->attributes){
    if(xml_attr.name == attr::DstIP){
      ::ed247::xml::xmlAttr_get_value(xml_attr, _dst_ip_address);
    }else if(xml_attr.name == attr::DstPort){
      ::ed247::xml::xmlAttr_get_value(xml_attr, _dst_ip_port);
    }else if(xml_attr.name == attr::SrcIP){
      ::ed247::xml::xmlAttr_get_value(xml_attr, _src_ip_address);
    }else if(xml_attr.name == attr::SrcPort){
      ::ed247::xml::xmlAttr_get_value(xml_attr, _src_ip_port);
    }else if(xml_attr.name == attr::MulticastInterfaceIP){
      ::ed247::xml::xmlAttr_get_value(xml_attr, _mc_ip_address);
    }else if(xml_attr.name == attr::MulticastTTL){
      ::ed247::xml::xmlAttr_get_value(xml_attr, _mc_ttl);
    }else if(xml_attr.name == attr::Direction){
      ::ed247::xml::xmlAttr_get_value(xml_attr, _direction);
    }else{
      THROW_PARSER_ERROR(xml_node, "Unknown attribute [" << xml_attr.name << "] in tag [" << node::UdpSocket << "]");
    }
  }
  if (_direction == ED247_DIRECTION_INOUT) {
    THROW_PARSER_ERROR(xml_node, "Bidirectional UDP_Socket is not supported.");
  }

  // Check the values that used to be checked by the XSD validation of the ECIC file
  // (the schema is not applied anymore since the migration from libXML2 to expat).
  // Note: as in the schema, the source address (SrcIP/SrcPort) is not checked: an empty
  // source port means that the source port is chosen by the system.
  if (_dst_ip_address.empty()) {
    THROW_PARSER_ERROR(xml_node, "Missing attribute [" << attr::DstIP << "] in tag [" << node::UdpSocket << "]");
  }
  if (!::ed247::xml::xml_is_ip_address(_dst_ip_address)) {
    THROW_PARSER_ERROR(xml_node, "Invalid IP address [" << _dst_ip_address << "] for attribute [" << attr::DstIP << "]");
  }
  if (!_mc_ip_address.empty() && !::ed247::xml::xml_is_ip_address(_mc_ip_address)) {
    THROW_PARSER_ERROR(xml_node, "Invalid IP address [" << _mc_ip_address << "] for attribute [" << attr::MulticastInterfaceIP << "]");
  }
  if (_dst_ip_port == 0) {
    THROW_PARSER_ERROR(xml_node, "Invalid value [0] for attribute [" << attr::DstPort << "]: it shall be greater than 0");
  }
  if (_mc_ttl == 0) {
    THROW_PARSER_ERROR(xml_node, "Invalid value [0] for attribute [" << attr::MulticastTTL << "]: it shall be greater than 0");
  }
}

std::ostream& operator<<(std::ostream& stream, const ed247::xml::UdpSocket& socket)
{
  return stream << "UdpSocket - "
    "DstIP[" << socket._dst_ip_address << "] DstPort[" << socket._dst_ip_port<< "] "
    "SrcIP[" << socket._src_ip_address << "] SrcPort[" << socket._src_ip_port << "] "
    "MulticastInterfaceIP[" << socket._mc_ip_address << "] MulticastTTL[" << socket._mc_ttl << "]";
}

//
// ComInterface
//
void ed247::xml::ComInterface::load(node_ptr_t xml_node)
{
  for(const auto& xml_node_iter : xml_node->children){
    const std::string& node_name = xml_node_iter->name;
    if(node_name == node::UdpSockets){
      for(const auto& xml_node_child_iter : xml_node_iter->children){
        if(xml_node_child_iter->name == node::UdpSocket){
          _udp_sockets.emplace_back();
          _udp_sockets.back().load(xml_node_child_iter.get());
        }else{
          THROW_PARSER_ERROR(xml_node_child_iter.get(), "Unknown node [" << xml_node_child_iter->name << "] in tag [" << node::UdpSockets << "]");
        }
      }
    }else{
      THROW_PARSER_ERROR(xml_node_iter.get(), "Unknown node [" << node_name << "] in tag [" << node::ComInterface << "]");
    }
  }
}

//
// Stream base
//
ed247::xml::Stream::Stream(ed247_stream_type_t type, uint32_t sample_max_size_bytes, bool sample_size_fixed) :
  _direction(ED247_DIRECTION__INVALID),
  _type(type),
  _uid(0),
  _sample_max_number(1),
  _sample_max_size_bytes(sample_max_size_bytes),
  _sample_size_fixed(sample_size_fixed)
{
}

ed247::xml::StreamProtocoled::StreamProtocoled(ed247_stream_type_t type, uint32_t sample_max_size_bytes, bool sample_size_fixed) :
  Stream(type, sample_max_size_bytes, sample_size_fixed)
{}

ed247::xml::StreamSignals::StreamSignals(ed247_stream_type_t type, uint32_t sample_max_size_bytes, bool sample_size_fixed) :
  Stream(type, sample_max_size_bytes, sample_size_fixed),
  _sampling_period_us(0)
{}

//
// A429Stream
//
ed247::xml::A429Stream::A429Stream() :
  StreamProtocoled(ED247_STREAM_TYPE_A429, 4, true)
{}

void ed247::xml::A429Stream::load(node_ptr_t xml_node)
{
  for(const auto& xml_attr : xml_node->attributes){
    if(xml_attr.name == attr::Name){
      ::ed247::xml::xmlAttr_get_value(xml_attr, _name);
    }else if(xml_attr.name == attr::Direction){
      ::ed247::xml::xmlAttr_get_value(xml_attr, _direction);
    }else if(xml_attr.name == attr::Comment){
      ::ed247::xml::xmlAttr_get_value(xml_attr, _comment);
    }else if(xml_attr.name == attr::ICD){
      ::ed247::xml::xmlAttr_get_value(xml_attr, _icd);
    }else if(xml_attr.name == attr::SampleMaxNumber){
      ::ed247::xml::xmlAttr_get_value(xml_attr, _sample_max_number);
    }else if(xml_attr.name == attr::UID){
      ::ed247::xml::xmlAttr_get_value(xml_attr, _uid);
    }else{
      THROW_PARSER_ERROR(xml_node, "Unknown attribute [" << xml_attr.name << "] in tag [" << node::A429_Stream << "]");
    }
  }

  for(const auto& xml_node_iter : xml_node->children){
    const std::string& node_name = xml_node_iter->name;
    if(node_name == node::DataTimestamp){
      _data_timestamp.load(xml_node_iter.get());
    }else if(node_name == node::Errors){
      _errors.load(xml_node_iter.get());
    }else{
      THROW_PARSER_ERROR(xml_node_iter.get(), "Unknown node [" << node_name << "] in tag [" << node::A429_Stream << "]");
    }
  }
}

void ed247::xml::A429Stream::validate(node_ptr_t closest_node)
{
  if (_sample_max_size_bytes != 4) {
    THROW_PARSER_ERROR(closest_node, node::A429_Stream << ": SampleMaxSizeByte shall be 4.");
  }
}


//
// A664Stream
//
ed247::xml::A664Stream::A664Stream() :
  StreamProtocoled(ED247_STREAM_TYPE_A664, 1471),
  _enable_message_size(ED247_YESNO_YES)
{
}

void ed247::xml::A664Stream::load(node_ptr_t xml_node)
{
  for(const auto& xml_attr : xml_node->attributes){
    if(xml_attr.name == attr::Name){
      ::ed247::xml::xmlAttr_get_value(xml_attr, _name);
    }else if(xml_attr.name == attr::Direction){
      ::ed247::xml::xmlAttr_get_value(xml_attr, _direction);
    }else if(xml_attr.name == attr::Comment){
      ::ed247::xml::xmlAttr_get_value(xml_attr, _comment);
    }else if(xml_attr.name == attr::ICD){
      ::ed247::xml::xmlAttr_get_value(xml_attr, _icd);
    }else if(xml_attr.name == attr::SampleMaxNumber){
      ::ed247::xml::xmlAttr_get_value(xml_attr, _sample_max_number);
    }else if(xml_attr.name == attr::SampleMaxSizeBytes){
      ::ed247::xml::xmlAttr_get_value(xml_attr, _sample_max_size_bytes);
    }else if(xml_attr.name == attr::UID){
      ::ed247::xml::xmlAttr_get_value(xml_attr, _uid);
    }else{
      THROW_PARSER_ERROR(xml_node, "Unknown attribute [" << xml_attr.name << "] in tag [" << node::A664_Stream << "]");
    }
  }

  for(const auto& xml_node_iter : xml_node->children){
    const std::string& node_name = xml_node_iter->name;
    if(node_name == node::DataTimestamp){
      _data_timestamp.load(xml_node_iter.get());
    }else if(node_name == node::Errors){
      _errors.load(xml_node_iter.get());
    }else if(node_name == node::MessageSize){
      for(const auto& xml_attr : xml_node_iter->attributes){
        if(xml_attr.name == attr::Enable){
          ::ed247::xml::xmlAttr_get_value(xml_attr, _enable_message_size);
        }else{
          THROW_PARSER_ERROR(xml_node_iter.get(), "Unknown attribute [" << xml_attr.name << "] in tag [" << node::MessageSize << "]");
        }
      }
    }else{
      THROW_PARSER_ERROR(xml_node_iter.get(), "Unknown node [" << node_name << "] in tag [" << node::A664_Stream << "]");
    }
  }
}

void ed247::xml::A664Stream::validate(node_ptr_t closest_node)
{
  if (_enable_message_size == ED247_YESNO_NO) {
    if (_sample_max_number > 1) {
      // Hack ED247LIB-27
      // THROW_PARSER_ERROR(closest_node, "A664: Cannot encode several samples (SampleMaxNumber=" << _sample_max_number << ") "
      // "in a stream without sample size (MessageSize disabled)");
      //
      // Curently the library is able to handle this case by sending one packet by sample. But this is not part of the NORM.
    }
  }

  if (_sample_max_size_bytes > std::numeric_limits<uint16_t>::max()) {
    THROW_PARSER_ERROR(closest_node, node::A664_Stream << ": SampleMaxSizeByte shall be lower than " << std::numeric_limits<uint16_t>::max() <<
                       " (A greater size cannot be encoded in an ED247 frame)");
  }
}


//
// A825Stream
//
ed247::xml::A825Stream::A825Stream() :
  StreamProtocoled(ED247_STREAM_TYPE_A825, 69)
{
}

void ed247::xml::A825Stream::load(node_ptr_t xml_node)
{
  for(const auto& xml_attr : xml_node->attributes){
    if(xml_attr.name == attr::Name){
      ::ed247::xml::xmlAttr_get_value(xml_attr, _name);
    }else if(xml_attr.name == attr::Direction){
      ::ed247::xml::xmlAttr_get_value(xml_attr, _direction);
    }else if(xml_attr.name == attr::Comment){
      ::ed247::xml::xmlAttr_get_value(xml_attr, _comment);
    }else if(xml_attr.name == attr::ICD){
      ::ed247::xml::xmlAttr_get_value(xml_attr, _icd);
    }else if(xml_attr.name == attr::SampleMaxNumber){
      ::ed247::xml::xmlAttr_get_value(xml_attr, _sample_max_number);
    }else if(xml_attr.name == attr::UID){
      ::ed247::xml::xmlAttr_get_value(xml_attr, _uid);
    }else{
      THROW_PARSER_ERROR(xml_node, "Unknown attribute [" << xml_attr.name << "] in tag [" << node::A825_Stream << "]");
    }
  }

  for(const auto& xml_node_iter : xml_node->children){
    const std::string& node_name = xml_node_iter->name;
    if(node_name == node::DataTimestamp){
      _data_timestamp.load(xml_node_iter.get());
    }else if(node_name == node::Errors){
      _errors.load(xml_node_iter.get());
    }else{
      THROW_PARSER_ERROR(xml_node_iter.get(), "Unknown node [" << node_name << "] in tag [" << node::A825_Stream << "]");
    }
  }
}

void ed247::xml::A825Stream::validate(node_ptr_t closest_node)
{
  if (_sample_max_size_bytes > std::numeric_limits<uint8_t>::max()) {
    THROW_PARSER_ERROR(closest_node, node::A825_Stream << ": SampleMaxSizeByte shall be lower than " << std::numeric_limits<uint8_t>::max() <<
                       " (A greater size cannot be encoded in an ED247 frame)");
  }
}

//
// SERIALStream
//
ed247::xml::SERIALStream::SERIALStream() :
  StreamProtocoled(ED247_STREAM_TYPE_SERIAL, 1)
{
}

void ed247::xml::SERIALStream::load(node_ptr_t xml_node)
{
  for(const auto& xml_attr : xml_node->attributes){
    if(xml_attr.name == attr::Name){
      ::ed247::xml::xmlAttr_get_value(xml_attr, _name);
    }else if(xml_attr.name == attr::Direction){
      ::ed247::xml::xmlAttr_get_value(xml_attr, _direction);
    }else if(xml_attr.name == attr::Comment){
      ::ed247::xml::xmlAttr_get_value(xml_attr, _comment);
    }else if(xml_attr.name == attr::ICD){
      ::ed247::xml::xmlAttr_get_value(xml_attr, _icd);
    }else if(xml_attr.name == attr::SampleMaxNumber){
      ::ed247::xml::xmlAttr_get_value(xml_attr, _sample_max_number);
    }else if(xml_attr.name == attr::SampleMaxSizeBytes){
      ::ed247::xml::xmlAttr_get_value(xml_attr, _sample_max_size_bytes);
    }else if(xml_attr.name == attr::UID){
      ::ed247::xml::xmlAttr_get_value(xml_attr, _uid);
    }else{
      THROW_PARSER_ERROR(xml_node, "Unknown attribute [" << xml_attr.name << "] in tag [" << node::SERIAL_Stream << "]");
    }
  }

  for(const auto& xml_node_iter : xml_node->children){
    const std::string& node_name = xml_node_iter->name;
    if(node_name == node::DataTimestamp){
      _data_timestamp.load(xml_node_iter.get());
    }else if(node_name == node::Errors){
      _errors.load(xml_node_iter.get());
    }else{
      THROW_PARSER_ERROR(xml_node_iter.get(), "Unknown node [" << node_name << "] in tag [" << node::SERIAL_Stream << "]");
    }
  }
}

void ed247::xml::SERIALStream::validate(node_ptr_t closest_node)
{
  if (_sample_max_size_bytes > std::numeric_limits<uint16_t>::max()) {
    THROW_PARSER_ERROR(closest_node, node::SERIAL_Stream << ": SampleMaxSizeByte shall be lower than " << std::numeric_limits<uint16_t>::max() <<
                       " (A greater size cannot be encoded in an ED247 frame)");
  }
}

//
// ETHStream
//
ed247::xml::ETHStream::ETHStream() :
  StreamProtocoled(ED247_STREAM_TYPE_ETHERNET, 1),
  _enable_message_size(ED247_YESNO_YES),
  _layer(std::string())
{}

void ed247::xml::ETHStream::load(node_ptr_t xml_node)
{
  //Finish adding all possible attibutes
  for(const auto& xml_attr : xml_node->attributes){
    if(xml_attr.name == attr::Name){
      ::ed247::xml::xmlAttr_get_value(xml_attr, _name);
    }else if(xml_attr.name == attr::Direction){
      ::ed247::xml::xmlAttr_get_value(xml_attr, _direction);
    }else if(xml_attr.name == attr::Comment){
      ::ed247::xml::xmlAttr_get_value(xml_attr, _comment);
    }else if(xml_attr.name == attr::ICD){
      ::ed247::xml::xmlAttr_get_value(xml_attr, _icd);
    }else if(xml_attr.name == attr::SampleMaxNumber){
      ::ed247::xml::xmlAttr_get_value(xml_attr, _sample_max_number);
    }else if(xml_attr.name == attr::SampleMaxSizeBytes){
      ::ed247::xml::xmlAttr_get_value(xml_attr, _sample_max_size_bytes);
    }else if(xml_attr.name == attr::UID){
      ::ed247::xml::xmlAttr_get_value(xml_attr, _uid);
    }else{
      THROW_PARSER_ERROR(xml_node, "Unknown attribute [" << xml_attr.name << "] in tag [" << node::ETH_Stream << "]");
    }
  }

  //check all possible children
  for(const auto& xml_node_iter : xml_node->children){
    const std::string& node_name = xml_node_iter->name;

    if(node_name == node::Frame){
      //search for Layer attribute
      for (const auto& xml_attr : xml_node_iter->attributes) {
        if (xml_attr.name == attr::Layer){
          ::ed247::xml::xmlAttr_get_value(xml_attr, _layer);
        } else {
          THROW_PARSER_ERROR(xml_node_iter.get(), "Unknown attribute [" << xml_attr.name << "] in tag [" << node::Frame << "] of node [" << node::ETH_Stream << "]");
        }
      }
    }else if(node_name == node::FrameSize){
      for (const auto& xml_attr : xml_node_iter->attributes) {
        if (xml_attr.name == attr::Enable) {
          ::ed247::xml::xmlAttr_get_value(xml_attr, _enable_message_size);
        } else {
          THROW_PARSER_ERROR(xml_node_iter.get(), "Unknown attribute [" << xml_attr.name << "] in tag [" << node::FrameSize << "] of node [" << node::ETH_Stream << "]");
        }
      }
    }else if(node_name == node::DataTimestamp){
      _data_timestamp.load(xml_node_iter.get());
    }else if(node_name == node::Errors){
      _errors.load(xml_node_iter.get());
    }else{
      THROW_PARSER_ERROR(xml_node_iter.get(), "Unknown node [" << node_name << "] in tag [" << node::ETH_Stream << "]");
    }
  }
}

void ed247::xml::ETHStream::validate(node_ptr_t closest_node)
{
  if (_enable_message_size == ED247_YESNO_NO) {
    if (_sample_max_number > 1) {
      THROW_PARSER_ERROR(closest_node, "ETH: Cannot encode several samples (SampleMaxNumber=" << _sample_max_number << ") "
      "in a stream without sample size (FrameSize disabled)");
      //Throws an error if SampleMaxNumber is > 1 while message size field is disabled
    }
  }
  if (_sample_max_size_bytes > std::numeric_limits<uint16_t>::max()) {
    THROW_PARSER_ERROR(closest_node, node::ETH_Stream << ": SampleMaxSizeBytes shall be lower than " << std::numeric_limits<uint16_t>::max() <<
                      " (A greater size cannot be encoded in an ED247 frame)");
  }
}

ed247::xml::Signal::Signal(ed247_signal_type_t type) :
  _type(type),
  _byte_offset(0),
  _nad_type(ED247_NAD_TYPE__INVALID),
  _vnad_position(0),
  _vnad_max_number(0)
{
  _nad_dimensions.push_back(1);
}

uint32_t ed247::xml::Signal::get_sample_max_size_bytes() const
{
  switch(_type) {
  case ED247_SIGNAL_TYPE_DISCRETE:
    return 1;
  case ED247_SIGNAL_TYPE_ANALOG:
    return 4;
  case ED247_SIGNAL_TYPE_NAD:
  {
    uint32_t size = 1;
    for (auto dim : _nad_dimensions) {
      size *= dim;
    }
    size = size * get_nad_type_size();
    return size;
  }
  case ED247_SIGNAL_TYPE_VNAD:
    return get_nad_type_size() * _vnad_max_number;
  default:
    return 0;
  }
}

uint32_t ed247::xml::Signal::get_nad_type_size(ed247_nad_type_t nad_type)
{
  switch(nad_type) {
  case ED247_NAD_TYPE_INT8    : return sizeof(int8_t);
  case ED247_NAD_TYPE_INT16   : return sizeof(int16_t);
  case ED247_NAD_TYPE_INT32   : return sizeof(int32_t);
  case ED247_NAD_TYPE_INT64   : return sizeof(int64_t);
  case ED247_NAD_TYPE_UINT8   : return sizeof(uint8_t);
  case ED247_NAD_TYPE_UINT16  : return sizeof(uint16_t);
  case ED247_NAD_TYPE_UINT32  : return sizeof(uint32_t);
  case ED247_NAD_TYPE_UINT64  : return sizeof(uint64_t);
  case ED247_NAD_TYPE_FLOAT32 : return sizeof(float);
  case ED247_NAD_TYPE_FLOAT64 : return sizeof(double);
  default: return 0;
  }
}

//
// DISSignal
//
ed247::xml::DISSignal::DISSignal() :
  Signal(ED247_SIGNAL_TYPE_DISCRETE)
{
  _nad_type = ED247_NAD_TYPE_UINT8;
}

void ed247::xml::DISSignal::load(node_ptr_t xml_node)
{
  for(const auto& xml_attr : xml_node->attributes){
    if(xml_attr.name == attr::Name){
      ::ed247::xml::xmlAttr_get_value(xml_attr, _name);
    }else if(xml_attr.name == attr::Comment){
      ::ed247::xml::xmlAttr_get_value(xml_attr, _comment);
    }else if(xml_attr.name == attr::ICD){
      ::ed247::xml::xmlAttr_get_value(xml_attr, _icd);
    }else if(xml_attr.name == attr::ByteOffset){
      ::ed247::xml::xmlAttr_get_value(xml_attr, _byte_offset);
    }else{
      THROW_PARSER_ERROR(xml_node, "Unknown attribute [" << xml_attr.name << "] in tag [" << node::Signal << "]");
    }
  }
}

//
// ANASignal
//
ed247::xml::ANASignal::ANASignal() :
  Signal(ED247_SIGNAL_TYPE_ANALOG)
{
  _nad_type = ED247_NAD_TYPE_FLOAT32;
}

void ed247::xml::ANASignal::load(node_ptr_t xml_node)
{
  for(const auto& xml_attr : xml_node->attributes){
    if(xml_attr.name == attr::Name){
      ::ed247::xml::xmlAttr_get_value(xml_attr, _name);
    }else if(xml_attr.name == attr::Comment){
      ::ed247::xml::xmlAttr_get_value(xml_attr, _comment);
    }else if(xml_attr.name == attr::ICD){
      ::ed247::xml::xmlAttr_get_value(xml_attr, _icd);
    }else if(xml_attr.name == attr::ElectricalUnit){
      ::ed247::xml::xmlAttr_get_value(xml_attr, _analogue_electrical_unit);
    }else if(xml_attr.name == attr::ByteOffset){
      ::ed247::xml::xmlAttr_get_value(xml_attr, _byte_offset);
    }else{
      THROW_PARSER_ERROR(xml_node, "Unknown attribute [" << xml_attr.name << "] in tag [" << node::Signal << "]");
    }
  }
}

//
// NADSignal
//
ed247::xml::NADSignal::NADSignal() :
  Signal(ED247_SIGNAL_TYPE_NAD)
{
}

void ed247::xml::NADSignal::load(node_ptr_t xml_node)
{
  for(const auto& xml_attr : xml_node->attributes){
    if(xml_attr.name == attr::Name){
      ::ed247::xml::xmlAttr_get_value(xml_attr, _name);
    }else if(xml_attr.name == attr::Comment){
      ::ed247::xml::xmlAttr_get_value(xml_attr, _comment);
    }else if(xml_attr.name == attr::ICD){
      ::ed247::xml::xmlAttr_get_value(xml_attr, _icd);
    }else if(xml_attr.name == attr::Dimensions){
      std::string testee(::ed247::xml::xml_get_prop(xml_node, xml_attr.name));
      size_t pos = 0;
      uint32_t dimension;
      std::istringstream iss;
      _nad_dimensions.clear();
      // This bloc capture each dimension followed by a separator
      while((pos = testee.find_first_of("xX")) != std::string::npos){
        iss.str(testee.substr(0, pos));
        iss >> dimension;
        iss.clear(); // Reset the flags or iss will not be usable anymore
        _nad_dimensions.push_back(dimension);
        testee.erase(0,pos+1);
      }
      // Capture the last dimension separatedly because it is not followed by a separator
      iss.str(testee.substr(0, pos));
      iss >> dimension;
      _nad_dimensions.push_back(dimension);

    }else if(xml_attr.name == attr::Type){
      ::ed247::xml::xmlAttr_get_value(xml_attr, _nad_type);
    }else if(xml_attr.name == attr::Unit){
      ::ed247::xml::xmlAttr_get_value(xml_attr, _nad_unit);
    }else if(xml_attr.name == attr::ByteOffset){
      ::ed247::xml::xmlAttr_get_value(xml_attr, _byte_offset);
    }else{
      THROW_PARSER_ERROR(xml_node, "Unknown attribute [" << xml_attr.name << "] in tag [" << node::Signal << "]");
    }
  }
}

//
// VNADSignal
//
ed247::xml::VNADSignal::VNADSignal() :
  Signal(ED247_SIGNAL_TYPE_VNAD)
{
}

void ed247::xml::VNADSignal::load(node_ptr_t xml_node)
{
  for(const auto& xml_attr : xml_node->attributes){
    if(xml_attr.name == attr::Name){
      ::ed247::xml::xmlAttr_get_value(xml_attr, _name);
    }else if(xml_attr.name == attr::Comment){
      ::ed247::xml::xmlAttr_get_value(xml_attr, _comment);
    }else if(xml_attr.name == attr::ICD){
      ::ed247::xml::xmlAttr_get_value(xml_attr, _icd);
    }else if(xml_attr.name == attr::Type){
      ::ed247::xml::xmlAttr_get_value(xml_attr, _nad_type);
    }else if(xml_attr.name == attr::Unit){
      ::ed247::xml::xmlAttr_get_value(xml_attr, _nad_unit);
    }else if(xml_attr.name == attr::MaxNumber){
      ::ed247::xml::xmlAttr_get_value(xml_attr, _vnad_max_number);
    }else if(xml_attr.name == attr::MaxLength){            // Backward compatibility
      ::ed247::xml::xmlAttr_get_value(xml_attr, _vnad_max_number);
    }else if(xml_attr.name == attr::Position){
      ::ed247::xml::xmlAttr_get_value(xml_attr, _vnad_position);
    }else{
      THROW_PARSER_ERROR(xml_node, "Unknown attribute [" << xml_attr.name << "] in tag [" << node::Signal << "]");
    }
  }
}


//
// DISStream
//
ed247::xml::DISStream::DISStream() :
  StreamSignals(ED247_STREAM_TYPE_DISCRETE, 0)
{
}

void ed247::xml::DISStream::load(node_ptr_t xml_node)
{
  for(const auto& xml_attr : xml_node->attributes){
    if(xml_attr.name == attr::Name){
      ::ed247::xml::xmlAttr_get_value(xml_attr, _name);
    }else if(xml_attr.name == attr::Direction){
      ::ed247::xml::xmlAttr_get_value(xml_attr, _direction);
    }else if(xml_attr.name == attr::Comment){
      ::ed247::xml::xmlAttr_get_value(xml_attr, _comment);
    }else if(xml_attr.name == attr::ICD){
      ::ed247::xml::xmlAttr_get_value(xml_attr, _icd);
    }else if(xml_attr.name == attr::SampleMaxNumber){
      ::ed247::xml::xmlAttr_get_value(xml_attr, _sample_max_number);
    }else if(xml_attr.name == attr::SampleMaxSizeBytes){
      ::ed247::xml::xmlAttr_get_value(xml_attr, _sample_max_size_bytes);
    }else if(xml_attr.name == attr::UID){
      ::ed247::xml::xmlAttr_get_value(xml_attr, _uid);
    }else{
      THROW_PARSER_ERROR(xml_node, "Unknown attribute [" << xml_attr.name << "] in tag [" << node::DIS_Stream << "]");
    }
  }

  for(const auto& xml_node_iter : xml_node->children){
    const std::string& node_name = xml_node_iter->name;
    if(node_name == node::Signals){
      // Read the attributes of the <Signals> element (e.g. SamplingPeriodUs).
      // Note: they are read once for all the signals (and not for each of them).
      for(const auto& xml_attr : xml_node_iter->attributes){
        if(xml_attr.name == attr::SamplingPeriodUs){
          ::ed247::xml::xmlAttr_get_value(xml_attr, _sampling_period_us);
        }else{
          THROW_PARSER_ERROR(xml_node_iter.get(), "Unknown attribute [" << xml_attr.name << "] in tag [" << node::Signals << "]");
        }
      }
      // Load the signals
      for(const auto& xml_node_child_iter : xml_node_iter->children){
        if(xml_node_child_iter->name == node::Signal) {
          DISSignal* signal = new DISSignal();
          signal->load(xml_node_child_iter.get());
          if(signal->_byte_offset + signal->get_sample_max_size_bytes() > _sample_max_size_bytes)
            THROW_PARSER_ERROR(xml_node_child_iter.get(), "Stream [" << _name << "] Signal [" << signal->_name << "]: "
                               "ByteOffset [" << signal->_byte_offset << "] + [" << signal->get_sample_max_size_bytes() << "] > "
                               "Stream SampleMaxSizeBytes [" << _sample_max_size_bytes << "]");
          _signal_list.emplace_back(signal);
        }else{
          THROW_PARSER_ERROR(xml_node_child_iter.get(), "Unknown node [" << xml_node_child_iter->name << "] in tag [" << node::Signals << "]");
        }
      }
    }else if(node_name == node::DataTimestamp){
      _data_timestamp.load(xml_node_iter.get());
    }else{
      THROW_PARSER_ERROR(xml_node_iter.get(), "Unknown node [" << node_name << "] in tag [" << node::DIS_Stream << "]");
    }
  }
  // Sort according to ByteOffset
  std::sort(_signal_list.begin(), _signal_list.end(),
            [](const std::unique_ptr<Signal>& a, const std::unique_ptr<Signal>& b) {
              return (a == nullptr || b == nullptr) ? false : (a->_byte_offset < b->_byte_offset);
            });
}

void ed247::xml::DISStream::validate(node_ptr_t closest_node)
{
  //Checks that a sample's max size cannot be greater than ED-247 frame size limit.
  if (_sample_max_size_bytes > std::numeric_limits<uint16_t>::max()) {
    THROW_PARSER_ERROR(closest_node, node::DIS_Stream << ": SampleMaxSizeByte shall be lower than " << std::numeric_limits<uint16_t>::max() <<
                       " (A greater size cannot be encoded in an ED247 frame)");
  }
}


//
// ANAStream
//
ed247::xml::ANAStream::ANAStream() :
  StreamSignals(ED247_STREAM_TYPE_ANALOG, 0)
{
}

void ed247::xml::ANAStream::load(node_ptr_t xml_node)
{
  for(const auto& xml_attr : xml_node->attributes){
    if(xml_attr.name == attr::Name){
      ::ed247::xml::xmlAttr_get_value(xml_attr, _name);
    }else if(xml_attr.name == attr::Direction){
      ::ed247::xml::xmlAttr_get_value(xml_attr, _direction);
    }else if(xml_attr.name == attr::Comment){
      ::ed247::xml::xmlAttr_get_value(xml_attr, _comment);
    }else if(xml_attr.name == attr::ICD){
      ::ed247::xml::xmlAttr_get_value(xml_attr, _icd);
    }else if(xml_attr.name == attr::SampleMaxNumber){
      ::ed247::xml::xmlAttr_get_value(xml_attr, _sample_max_number);
    }else if(xml_attr.name == attr::SampleMaxSizeBytes){
      ::ed247::xml::xmlAttr_get_value(xml_attr, _sample_max_size_bytes);
    }else if(xml_attr.name == attr::UID){
      ::ed247::xml::xmlAttr_get_value(xml_attr, _uid);
    }else{
      THROW_PARSER_ERROR(xml_node, "Unknown attribute [" << xml_attr.name << "] in tag [" << node::ANA_Stream << "]");
    }
  }

  for(const auto& xml_node_iter : xml_node->children){
    const std::string& node_name = xml_node_iter->name;
    if(node_name == node::Signals){
      // Read the attributes of the <Signals> element (e.g. SamplingPeriodUs).
      // Note: they are read once for all the signals (and not for each of them).
      for(const auto& xml_attr : xml_node_iter->attributes){
        if(xml_attr.name == attr::SamplingPeriodUs){
          ::ed247::xml::xmlAttr_get_value(xml_attr, _sampling_period_us);
        }else{
          THROW_PARSER_ERROR(xml_node_iter.get(), "Unknown attribute [" << xml_attr.name << "] in tag [" << node::Signals << "]");
        }
      }
      // Load the signals
      for(const auto& xml_node_child_iter : xml_node_iter->children){
        if(xml_node_child_iter->name == node::Signal){
          ANASignal* signal = new ANASignal();
          signal->load(xml_node_child_iter.get());
          if(signal->_byte_offset + signal->get_sample_max_size_bytes() > _sample_max_size_bytes)
            THROW_PARSER_ERROR(xml_node_child_iter.get(), "Stream [" << _name << "] Signal [" << signal->_name << "]: "
                               "ByteOffset [" << signal->_byte_offset << "] + [" << signal->get_sample_max_size_bytes() << "] > "
                               "Stream SampleMaxSizeBytes [" << _sample_max_size_bytes << "]");
          _signal_list.emplace_back(signal);
        }else{
          THROW_PARSER_ERROR(xml_node_child_iter.get(), "Unknown node [" << xml_node_child_iter->name << "] in tag [" << node::Signals << "]");
        }
      }
    }else if(node_name == node::DataTimestamp){
      _data_timestamp.load(xml_node_iter.get());
    }else{
      THROW_PARSER_ERROR(xml_node_iter.get(), "Unknown node [" << node_name << "] in tag [" << node::ANA_Stream << "]");
    }
  }
  // Sort according to ByteOffset
  std::sort(_signal_list.begin(), _signal_list.end(),
            [](const std::unique_ptr<Signal>& a, const std::unique_ptr<Signal>& b) {
              return (a == nullptr || b == nullptr) ? false : (a->_byte_offset < b->_byte_offset);
            });
}

void ed247::xml::ANAStream::validate(node_ptr_t closest_node)
{
  //Checks that a sample's max size cannot be greater than ED-247 frame size limit.
  if (_sample_max_size_bytes > std::numeric_limits<uint16_t>::max()) {
    THROW_PARSER_ERROR(closest_node, node::ANA_Stream << ": SampleMaxSizeByte shall be lower than " << std::numeric_limits<uint16_t>::max() <<
                       " (A greater size cannot be encoded in an ED247 frame)");
  }
}


//
// NADStream
//
ed247::xml::NADStream::NADStream() :
  StreamSignals(ED247_STREAM_TYPE_NAD, 0)
{
}

void ed247::xml::NADStream::load(node_ptr_t xml_node)
{
  for(const auto& xml_attr : xml_node->attributes){
    if(xml_attr.name == attr::Name){
      ::ed247::xml::xmlAttr_get_value(xml_attr, _name);
    }else if(xml_attr.name == attr::Direction){
      ::ed247::xml::xmlAttr_get_value(xml_attr, _direction);
    }else if(xml_attr.name == attr::Comment){
      ::ed247::xml::xmlAttr_get_value(xml_attr, _comment);
    }else if(xml_attr.name == attr::ICD){
      ::ed247::xml::xmlAttr_get_value(xml_attr, _icd);
    }else if(xml_attr.name == attr::SampleMaxNumber){
      ::ed247::xml::xmlAttr_get_value(xml_attr, _sample_max_number);
    }else if(xml_attr.name == attr::SampleMaxSizeBytes){
      ::ed247::xml::xmlAttr_get_value(xml_attr, _sample_max_size_bytes);
    }else if(xml_attr.name == attr::UID){
      ::ed247::xml::xmlAttr_get_value(xml_attr, _uid);
    }else{
      THROW_PARSER_ERROR(xml_node, "Unknown attribute [" << xml_attr.name << "] in tag [" << node::NAD_Stream << "]");
    }
  }

  for(const auto& xml_node_iter : xml_node->children){
    const std::string& node_name = xml_node_iter->name;
    if(node_name == node::Signals){
      // Read the attributes of the <Signals> element (e.g. SamplingPeriodUs).
      // Note: they are read once for all the signals (and not for each of them).
      for(const auto& xml_attr : xml_node_iter->attributes){
        if(xml_attr.name == attr::SamplingPeriodUs){
          ::ed247::xml::xmlAttr_get_value(xml_attr, _sampling_period_us);
        }else{
          THROW_PARSER_ERROR(xml_node_iter.get(), "Unknown attribute [" << xml_attr.name << "] in tag [" << node::Signals << "]");
        }
      }
      // Load the signals
      for(const auto& xml_node_child_iter : xml_node_iter->children){
        if(xml_node_child_iter->name == node::Signal){
          NADSignal* signal = new NADSignal();
          signal->load(xml_node_child_iter.get());
          if(signal->_byte_offset + signal->get_sample_max_size_bytes() > _sample_max_size_bytes)
            THROW_PARSER_ERROR(xml_node_child_iter.get(), "Stream [" << _name << "] Signal [" << signal->_name << "]: "
                               "ByteOffset [" << signal->_byte_offset << "] + [" << signal->get_sample_max_size_bytes() << "] > "
                               "Stream SampleMaxSizeBytes [" << _sample_max_size_bytes << "]");
          _signal_list.emplace_back(signal);
        }else{
          THROW_PARSER_ERROR(xml_node_child_iter.get(), "Unknown node [" << xml_node_child_iter->name << "] in tag [" << node::Signals << "]");
        }
      }
    }else if(node_name == node::DataTimestamp){
      _data_timestamp.load(xml_node_iter.get());
    }else{
      THROW_PARSER_ERROR(xml_node_iter.get(), "Unknown node [" << node_name << "] in tag [" << node::NAD_Stream << "]");
    }
  }
  // Sort according to ByteOffset
  std::sort(_signal_list.begin(), _signal_list.end(),
            [](const std::unique_ptr<Signal>& a, const std::unique_ptr<Signal>& b) {
              return (a == nullptr || b == nullptr) ? false : (a->_byte_offset < b->_byte_offset);
            });
}

void ed247::xml::NADStream::validate(node_ptr_t closest_node)
{
  //Checks that a sample's max size cannot be greater than ED-247 frame size limit.
  if (_sample_max_size_bytes > std::numeric_limits<uint16_t>::max()) {
    THROW_PARSER_ERROR(closest_node, node::NAD_Stream << ": SampleMaxSizeByte shall be lower than " << std::numeric_limits<uint16_t>::max() <<
                       " (A greater size cannot be encoded in an ED247 frame)");
  }
}

//
// VNADStream
//
ed247::xml::VNADStream::VNADStream() :
  StreamSignals(ED247_STREAM_TYPE_VNAD, 0, false)
{
}

void ed247::xml::VNADStream::load(node_ptr_t xml_node)
{
  for(const auto& xml_attr : xml_node->attributes){
    if(xml_attr.name == attr::Name){
      ::ed247::xml::xmlAttr_get_value(xml_attr, _name);
    }else if(xml_attr.name == attr::Direction){
      ::ed247::xml::xmlAttr_get_value(xml_attr, _direction);
    }else if(xml_attr.name == attr::Comment){
      ::ed247::xml::xmlAttr_get_value(xml_attr, _comment);
    }else if(xml_attr.name == attr::ICD){
      ::ed247::xml::xmlAttr_get_value(xml_attr, _icd);
    }else if(xml_attr.name == attr::SampleMaxNumber){
      ::ed247::xml::xmlAttr_get_value(xml_attr, _sample_max_number);
    }else if(xml_attr.name == attr::UID){
      ::ed247::xml::xmlAttr_get_value(xml_attr, _uid);
    }else{
      THROW_PARSER_ERROR(xml_node, "Unknown attribute [" << xml_attr.name << "] in tag [" << node::VNAD_Stream << "]");
    }
  }

  for(const auto& xml_node_iter : xml_node->children){
    const std::string& node_name = xml_node_iter->name;
    if(node_name == node::Signals){
      // Read the attributes of the <Signals> element (e.g. SamplingPeriodUs).
      // Note: they are read once for all the signals (and not for each of them).
      for(const auto& xml_attr : xml_node_iter->attributes){
        if(xml_attr.name == attr::SamplingPeriodUs){
          ::ed247::xml::xmlAttr_get_value(xml_attr, _sampling_period_us);
        }else{
          THROW_PARSER_ERROR(xml_node_iter.get(), "Unknown attribute [" << xml_attr.name << "] in tag [" << node::Signals << "]");
        }
      }
      // Load the signals
      for(const auto& xml_node_child_iter : xml_node_iter->children){
        if(xml_node_child_iter->name == node::Signal){
          VNADSignal* signal = new VNADSignal();
          signal->load(xml_node_child_iter.get());
          _sample_max_size_bytes += signal->get_sample_max_size_bytes() + sizeof(uint16_t);
          _signal_list.emplace_back(signal);
        }else{
          THROW_PARSER_ERROR(xml_node_child_iter.get(), "Unknown node [" << xml_node_child_iter->name << "] in tag [" << node::Signals << "]");
        }
      }
      PRINT_DEBUG("VNAD Stream [" << _name << "] SampleMaxSizeBytes[" << _sample_max_size_bytes << "] SampleMaxNumber[" << _sample_max_number << "]");
    }else if(node_name == node::DataTimestamp){
      _data_timestamp.load(xml_node_iter.get());
    }else{
      THROW_PARSER_ERROR(xml_node_iter.get(), "Unknown node [" << node_name << "] in tag [" << node::VNAD_Stream << "]");
    }
  }
  // Sort according to VNAD position
  std::sort(_signal_list.begin(), _signal_list.end(),
            [](const std::unique_ptr<Signal>& a, const std::unique_ptr<Signal>& b) {
              return (a == nullptr || b == nullptr) ? false : (a->_vnad_position < b->_vnad_position);
            });
}

void ed247::xml::VNADStream::validate(node_ptr_t closest_node)
{
  if (_sample_max_size_bytes > std::numeric_limits<uint16_t>::max()) {
    THROW_PARSER_ERROR(closest_node, node::VNAD_Stream << ": SampleMaxSizeByte shall be lower than " << std::numeric_limits<uint16_t>::max() <<
                       " (A greather size cannot be encoded in an ED247 frame)");
  }
}


//
// Header
//
ed247::xml::Header::Header() :
  _enable(ED247_YESNO_NO),
  _transport_timestamp(ED247_YESNO_NO)
{
}

void ed247::xml::Header::load(node_ptr_t xml_node)
{
  for(const auto& xml_attr : xml_node->attributes){
    if(xml_attr.name == attr::Enable){
      ::ed247::xml::xmlAttr_get_value(xml_attr, _enable);
    }else if(xml_attr.name == attr::TransportTimestamp){
      ::ed247::xml::xmlAttr_get_value(xml_attr, _transport_timestamp);
    }else{
      THROW_PARSER_ERROR(xml_node, "Unknown attribute [" << xml_attr.name << "] in tag [" << node::Header << "]");
    }
  }
}

//
// Channel
//
ed247::xml::Channel::Channel() :
  _frame_standard_revision(ED247_STANDARD__INVALID),
  _is_simple_channel(true)
{
}

void ed247::xml::Channel::load(node_ptr_t xml_node)
{
  ed247_direction_t streams_direction(ED247_DIRECTION__INVALID);

  for(const auto& xml_attr : xml_node->attributes){
    if(xml_attr.name == attr::Name){
      ::ed247::xml::xmlAttr_get_value(xml_attr, _name);
    }else if(xml_attr.name == attr::Comment){
      ::ed247::xml::xmlAttr_get_value(xml_attr, _comment);
    }else{
      THROW_PARSER_ERROR(xml_node, "Unknown attribute [" << xml_attr.name << "] in tag [" << node::MultiChannel << "] or [" << node::Channel << "]");
    }
  }

  for(const auto& xml_node_iter : xml_node->children){
    const std::string& node_name = xml_node_iter->name;
    if(node_name == node::ComInterface){
      _com_interface.load(xml_node_iter.get());
    }else if(node_name == node::Header){
      _header.load(xml_node_iter.get());
    }else if(node_name == node::FrameFormat){
      for(const auto& xml_attr : xml_node_iter->attributes){
        if(xml_attr.name == attr::StandardRevision){
          ::ed247::xml::xmlAttr_get_value(xml_attr, _frame_standard_revision);
        }else{
          THROW_PARSER_ERROR(xml_node_iter.get(), "Unknown attribute [" << xml_attr.name << "] in tag [" << node::FrameFormat <<"]");
        }
      }
    }
    else if((_is_simple_channel == false && node_name == node::Streams) ||
            (_is_simple_channel == true  && node_name == node::Stream))
    {
      for(const auto& xml_node_child_iter : xml_node_iter->children){
        const std::string& child_name = xml_node_child_iter->name;
        // A429
        if(child_name == node::A429_Stream){
          A429Stream* stream = new A429Stream();
          stream->load(xml_node_child_iter.get());
          _stream_list.emplace_back(stream);
          // A664
        }else if(child_name == node::A664_Stream){
          A664Stream* stream = new A664Stream();
          stream->load(xml_node_child_iter.get());
          _stream_list.emplace_back(stream);
          // A825
        }else if(child_name == node::A825_Stream){
          A825Stream* stream = new A825Stream();
          stream->load(xml_node_child_iter.get());
          _stream_list.emplace_back(stream);
          // SERIAL
        }else if(child_name == node::SERIAL_Stream){
          SERIALStream* stream = new SERIALStream();
          stream->load(xml_node_child_iter.get());
          _stream_list.emplace_back(stream);
          // DISCRETE
        }else if(child_name == node::DIS_Stream){
          DISStream* stream = new DISStream();
          stream->load(xml_node_child_iter.get());
          _stream_list.emplace_back(stream);
          // ANALOG
        }else if(child_name == node::ANA_Stream){
          ANAStream* stream = new ANAStream();
          stream->load(xml_node_child_iter.get());
          _stream_list.emplace_back(stream);
          // NAD
        }else if(child_name == node::NAD_Stream){
          NADStream* stream = new NADStream();
          stream->load(xml_node_child_iter.get());
          _stream_list.emplace_back(stream);
          // VNAD
        }else if(child_name == node::VNAD_Stream){
          VNADStream* stream = new VNADStream();
          stream->load(xml_node_child_iter.get());
          _stream_list.emplace_back(stream);
          //ETH
        }else if(child_name == node::ETH_Stream){
          ETHStream* stream = new ETHStream();
          stream->load(xml_node_child_iter.get());
          _stream_list.emplace_back(stream);
          // Otherwise
        }else{
          THROW_PARSER_ERROR(xml_node_child_iter.get(), "Unknown node [" << child_name << "] in tag [" << node::Streams << "]");
        }
        streams_direction = (ed247_direction_t)(streams_direction | _stream_list.back()->_direction);
        _stream_list.back()->validate(xml_node_child_iter.get());
      }
    }else{
      THROW_PARSER_ERROR(xml_node_iter.get(), "Unexpected node [" << node_name << "]");
    }
  }

  //
  // Consolidate direction
  //

  // Set a direction on UdpSockets without direction & look for overall ComInterface direction
  ed247_direction_t com_interface_direction(ED247_DIRECTION__INVALID);
  for(UdpSocket& udp_socket : _com_interface._udp_sockets) {
    if (udp_socket._direction == ED247_DIRECTION__INVALID) {
      if (streams_direction == ED247_DIRECTION__INVALID || streams_direction == ED247_DIRECTION_INOUT)
        THROW_PARSER_ERROR(xml_node, "Cannot decide UdpSocket " << udp_socket._dst_ip_address << ":" <<
                           udp_socket._dst_ip_port << " direction for channel " << _name);
      udp_socket._direction = streams_direction;
    }
    com_interface_direction = (ed247_direction_t)(com_interface_direction | udp_socket._direction);
  }

  // Set a direction on Steams without direction & look for overall streams direction
  // This prevent the creation of bidirectional streams when channel has only one-way UdpSockets
  for (std::unique_ptr<Stream>& stream : _stream_list) {
    if (stream->_direction == ED247_DIRECTION__INVALID) {
      stream->_direction = com_interface_direction;
    }
    streams_direction = (ed247_direction_t)(streams_direction | _stream_list.back()->_direction);
  }

  // Warn if some streams are not able to communicate
  if (((streams_direction       & ED247_DIRECTION_IN) != 0) &&
      ((com_interface_direction & ED247_DIRECTION_IN) == 0))
    PARSER_WARNING(xml_node, "Channel " << _name << " has input streams without input UdpSockets.");

  if (((streams_direction       & ED247_DIRECTION_OUT) != 0) &&
      ((com_interface_direction & ED247_DIRECTION_OUT) == 0))
    PARSER_WARNING(xml_node, "Channel " << _name << " has output streams without output UdpSockets.");
}

//
// Component
//
ed247::xml::Component::Component() :
  _identifier(0),
  _component_type(ED247_COMPONENT_TYPE_VIRTUAL),
  _standard_revision(ED247_STANDARD__INVALID)
{
}

void ed247::xml::Component::load(node_ptr_t xml_node)
{
  for(const auto& xml_attr : xml_node->attributes){
    // Ignore the attributes that describe the location of the XSD schema.
    // Note: the namespace declarations (xmlns / xmlns:prefix) are already filtered by the parser.
    if(xml_attr.name == "noNamespaceSchemaLocation" || xml_attr.name == "schemaLocation"){
      continue;
    }
    if(xml_attr.name == attr::Name){
      ::ed247::xml::xmlAttr_get_value(xml_attr, _name);
    }else if(xml_attr.name == attr::ComponentVersion){
      ::ed247::xml::xmlAttr_get_value(xml_attr, _version);
    }else if(xml_attr.name == attr::ComponentType){
      ::ed247::xml::xmlAttr_get_value(xml_attr, _component_type);
    }else if(xml_attr.name == attr::Comment){
      ::ed247::xml::xmlAttr_get_value(xml_attr, _comment);
    }else if(xml_attr.name == attr::StandardRevision){
      ::ed247::xml::xmlAttr_get_value(xml_attr, _standard_revision);
    }else if(xml_attr.name == attr::Identifier){
      ::ed247::xml::xmlAttr_get_value(xml_attr, _identifier);
    }else{
      THROW_PARSER_ERROR(xml_node, "Unknown attribute [" << xml_attr.name << "] in tag [" << node::ED247ComponentInstanceConfiguration <<"]");
    }
  }
  if(_standard_revision != ED247_STANDARD_ED247A)
    THROW_PARSER_ERROR(xml_node, "This version do not support any other standard than [" << std::string(ed247_standard_string(ED247_STANDARD_ED247A)) << "]");

  for(const auto& xml_node_iter : xml_node->children){
    const std::string& node_name = xml_node_iter->name;
    if(node_name == node::Channels){
      for(const auto& xml_node_channel : xml_node_iter->children){
        const std::string& channel_name = xml_node_channel->name;
        if(channel_name == node::MultiChannel) {
          _channel_list.emplace_back();
          Channel& channel = _channel_list.back();
          channel._is_simple_channel = false; // store if it is a simple channel (only one stream)
          channel.load(xml_node_channel.get());
          if(channel._frame_standard_revision != ED247_STANDARD_ED247A)
            THROW_PARSER_ERROR(xml_node_channel.get(), "This version do not support any other standard than [" << std::string(ed247_standard_string(ED247_STANDARD_ED247A)) << "]");
        }else if(channel_name == node::Channel) {
          _channel_list.emplace_back();
          Channel& channel = _channel_list.back();
          channel._is_simple_channel = true; // store if it is a simple channel (only one stream)
          channel.load(xml_node_channel.get());
          if(channel._frame_standard_revision != ED247_STANDARD_ED247A)
            THROW_PARSER_ERROR(xml_node_channel.get(), "This version do not support any other standard than [" << std::string(ed247_standard_string(ED247_STANDARD_ED247A)) << "]");
        }else{
          THROW_PARSER_ERROR(xml_node_channel.get(), "Unknown node [" << channel_name << "] in tag [" << node::Channels << "]");
        }
      }
    }else if(node_name == node::FileProducer){
      for(const auto& xml_attr : xml_node_iter->attributes){
        if(xml_attr.name == attr::Identifier){
          ::ed247::xml::xmlAttr_get_value(xml_attr, _file_producer_identifier);
        }else if(xml_attr.name == attr::Comment){
          ::ed247::xml::xmlAttr_get_value(xml_attr, _file_producer_comment);
        }else{
          THROW_PARSER_ERROR(xml_node_iter.get(), "Unknown attribute [" << xml_attr.name << "] in tag [" << node::FileProducer <<"]");
        }
      }
    }else{
      THROW_PARSER_ERROR(xml_node_iter.get(), "Unknown node [" << node_name << "] in tag [" << node::ED247ComponentInstanceConfiguration << "]");
    }
  }
}

//
// load
//
// Note: the ECIC file used to be validated against the XSD schema (ED247A_ECIC.xsd) by libXML2.
// As expat does not support the XSD validation, the schema is not applied anymore: only the
// semantic analysis performed by the load() methods above is done (unknown attributes, unknown
// nodes, invalid enumerated values, invalid numbers, invalid IP addresses, ...).
//
static std::unique_ptr<ed247::xml::Component> ed247_ecic_load(ed247::xml::dom::Document& document)
{
  if (document.root == nullptr) {
    // Note: the error is raised directly (not with the THROW_PARSER_ERROR macro) because
    // there is no node to locate the error in.
    const std::string message = "The XML document is empty";
    PRINT_ERROR(message);
    throw ed247::xml::exception(message);
  }

  // Check the root element of the document (this check used to be done by the XSD validation)
  if (document.root->name != ed247::xml::node::ED247ComponentInstanceConfiguration) {
    THROW_PARSER_ERROR(document.root.get(), "Unexpected root element [" << document.root->name << "]. Expected ["
                       << ed247::xml::node::ED247ComponentInstanceConfiguration << "]");
  }

  std::unique_ptr<ed247::xml::Component> root(new ed247::xml::Component());
  root->load(document.root.get());
  return root;
}

std::unique_ptr<ed247::xml::Component> ed247::xml::load_filepath(const std::string & filepath)
{
  // Parse the file and build its DOM (throws ed247::xml::exception if the file is not a valid XML document)
  ed247::xml::dom::Document document = ed247::xml::parse_file(filepath);
  return ed247_ecic_load(document);
}

std::unique_ptr<ed247::xml::Component> ed247::xml::load_content(const std::string & content)
{
  // Parse the content and build its DOM (throws ed247::xml::exception if the content is not a valid XML document)
  ed247::xml::dom::Document document = ed247::xml::parse_content(content, "<content>");
  return ed247_ecic_load(document);
}
