#include "ed247.h"
#include "time_tools.h"
#include "a429_tools.h"
#include <expat.h>
#include <fstream>
#include <iostream>
#include <string>
#include <string.h>
#include <vector>
#include <unordered_map>
#include <memory>


#define LOG_SHORTFILE       (strrchr("/" __FILE__, '/') + 1)
#define LOG_STREAM_FILELINE LOG_SHORTFILE << ":" << __LINE__ << " "

#define SAY(m) std::cout << m << std::endl;
#define ERR(m) std::cerr << LOG_STREAM_FILELINE << m << std::endl;
#define DIE(m) do { ERR(m); exit(1); } while (0)
#define ASSERT(t) do { if (!(t)) DIE(#t " FAILED"); } while (0)

// Options
namespace {
  std::string ecic_path;
  std::string cmd_path;
  bool fill_depth = false;
  uint64_t period_ms = 50;
  uint64_t duration_ms = 0;
}


//
// Store A429 buses found in CMD file
//
namespace a429 {
  struct label_t {
    a429_label_number_t number;
    a429_sdi_t sdi;
  };
  using label_list_t = std::vector<label_t>;
  struct bus_t {
    void push_label(std::string number, std::string sdi) {
      labels.push_back({
          a429_label_encode(number.c_str()),
          a429_sdi_from_string(sdi.c_str())
        });
    }
    label_list_t labels;
  };
  using bus_map_t = std::unordered_map<std::string, bus_t>;
}


//
// Parse the CMD file and fill the A429 buses
//
// Note: the CMD file used to be parsed with libXML2 and XPath (the expression
// "/ComponentMessagesDescription/A429_Bus" was evaluated by libXML2). It is now parsed with
// expat: as expat is a streaming parser without any XPath support, the elements are
// identified by their path in the document.
//
namespace {
  //
  // State of the parsing of the CMD file
  //
  struct cmd_context_t {
    std::vector<std::string> path;  // Path of the element being parsed (e.g. {"ComponentMessagesDescription", "A429_Bus"})
    a429::bus_map_t*         buses;  // A429 buses found in the CMD file
    a429::bus_t*             bus;    // A429 bus being parsed (nullptr if not in a bus)
    std::string              error;  // Error message (empty if no error)
  };

  //
  // Return the value of an attribute of an element (empty string if the attribute does not exist)
  //
  std::string cmd_attribute(const XML_Char** attributes, const std::string& name)
  {
    for (int attribute_id = 0 ; attributes[attribute_id] != nullptr ; attribute_id += 2) {
      if (name == attributes[attribute_id]) {
        return attributes[attribute_id + 1] != nullptr ? attributes[attribute_id + 1] : "";
      }
    }
    return std::string();
  }

  //
  // Called by expat when an element is opened
  //
  void XMLCALL cmd_start_element(void* user_data, const XML_Char* name, const XML_Char** attributes)
  {
    cmd_context_t* context = static_cast<cmd_context_t*>(user_data);
    context->path.push_back(name);

    // /ComponentMessagesDescription/A429_Bus
    if (context->path.size() == 2 &&
        context->path[0] == "ComponentMessagesDescription" &&
        context->path[1] == "A429_Bus")
    {
      const std::string bus_name = cmd_attribute(attributes, "Name");
      if (bus_name.empty()) {
        context->error = "Attribute 'Name' not found in A429_Bus element !";
        return;
      }
      // The buses are indexed by their name: an already known bus is not overwritten
      auto result = context->buses->emplace(bus_name, a429::bus_t());
      context->bus = &result.first->second;
    }
    // /ComponentMessagesDescription/A429_Bus/SamplingMessage
    else if (context->path.size() == 3 &&
             context->path[0] == "ComponentMessagesDescription" &&
             context->path[1] == "A429_Bus" &&
             context->path[2] == "SamplingMessage")
    {
      if (context->bus == nullptr) return;
      const std::string label_number = cmd_attribute(attributes, "Label");
      const std::string label_sdi    = cmd_attribute(attributes, "SDI");
      if (label_number.empty() || label_sdi.empty()) {
        context->error = "Attribute 'Label' or 'SDI' not found in SamplingMessage element !";
        return;
      }
      context->bus->push_label(label_number, label_sdi);
    }
  }

  //
  // Called by expat when an element is closed
  //
  void XMLCALL cmd_end_element(void* user_data, const XML_Char* /*name*/)
  {
    cmd_context_t* context = static_cast<cmd_context_t*>(user_data);
    if (!context->path.empty()) {
      context->path.pop_back();
    }
  }
}

//
// Load the CMD file and fill the A429 buses
//
void load_cmd_file(const std::string& cmd_path, a429::bus_map_t& a429_buses)
{
  std::ifstream file(cmd_path.c_str(), std::ios::in | std::ios::binary);
  if (!file.is_open()) {
    DIE("Cannot read CMD file '" << cmd_path << "'");
  }

  XML_Parser parser = XML_ParserCreate(nullptr);
  if (parser == nullptr) {
    DIE("Cannot create the XML parser !");
  }

  cmd_context_t context;
  context.buses = &a429_buses;
  XML_SetUserData(parser, &context);
  XML_SetElementHandler(parser, &cmd_start_element, &cmd_end_element);

  // Feed the parser chunk by chunk to avoid loading the whole file in memory.
  // As soon as an error is reported, the parsing is stopped: the parser must not be used anymore.
  bool parse_failed = false;
  std::string parse_error;
  char buffer[64 * 1024];
  while (file.good()) {
    file.read(buffer, sizeof(buffer));
    const std::streamsize count = file.gcount();
    if (count == 0) {
      break; // End of file
    }
    if (XML_Parse(parser, buffer, static_cast<int>(count), 0) == XML_STATUS_ERROR) {
      parse_failed = true;
      break;
    }
  }

  // Tell expat that the document is complete (only if no error occurred yet)
  if (!parse_failed && XML_Parse(parser, "", 0, 1) == XML_STATUS_ERROR) {
    parse_failed = true;
  }

  // Report the error reported by expat (with its position in the document)
  if (parse_failed) {
    parse_error = XML_ErrorString(XML_GetErrorCode(parser));
    parse_error += " at line " + std::to_string(XML_GetCurrentLineNumber(parser));
    parse_error += ", column " + std::to_string(XML_GetCurrentColumnNumber(parser));
  }
  XML_ParserFree(parser);

  if (parse_failed) {
    DIE("Cannot parse CMD file '" << cmd_path << "': " << parse_error);
  }
  if (!context.error.empty()) {
    DIE("Cannot read CMD file '" << cmd_path << "': " << context.error);
  }
}

//
// Classes that push data over network
//
struct transceiver_t {
  transceiver_t(ed247_stream_t stream) : _stream(stream) {}
  virtual void push() = 0;
protected:
  ed247_stream_t _stream;
};

struct generic_transceiver_t : public transceiver_t {
  generic_transceiver_t(ed247_stream_t stream) :
    transceiver_t(stream),
    _payload(nullptr)
  {
    _sample_size = ed247_stream_get_sample_max_size_bytes(stream);
    _qdepth = ed247_stream_get_sample_max_number(stream);
    _id = ed247_stream_get_uid(stream);

    // Generate an ID for non-multichannel streams
    if (_id == 0) _id = next_id++;

    SAY("Generic stream " << ed247_stream_get_name(stream) << " " <<
        ed247_stream_type_string(ed247_stream_get_type(stream)) << " "
        "ID: " << _id << " "
        "Sample size: " << _sample_size << " "
        "Depth: " << _qdepth << " "
        );
    _payload = new uint8_t[_sample_size];
    memset(_payload, 0, _sample_size);

    // If the payload size allows it, write the UID at the begining
    if (_sample_size > 4) {
      *(uint32_t*)_payload = _id;
     _payload_update = _payload + 4;
      _payload_update_size = _sample_size - 4;
    } else {
      _payload_update = _payload;
      _payload_update_size = _sample_size;
    }

  }
  void push() override {
    for (uint32_t depthid = 0;
         (fill_depth && depthid < _qdepth) || depthid == 0;
         depthid++)
    {
      memset(_payload_update,
             _payload_update[0] + 1,  // uint8 -> % 256
             _payload_update_size);
      ASSERT(ed247_stream_push_sample(_stream, _payload, _sample_size, nullptr, nullptr) == ED247_STATUS_SUCCESS);
    }
  }


private:
  uint32_t _id;
  uint8_t* _payload;
  uint32_t _sample_size;
  uint32_t _qdepth;

  uint8_t* _payload_update;
  uint32_t _payload_update_size;

  static uint32_t next_id;
};
uint32_t generic_transceiver_t::next_id = 100000;


struct a429_transceiver_t : public transceiver_t {
  a429_transceiver_t(ed247_stream_t stream, a429::bus_t bus) :
    transceiver_t(stream),
    _bus(bus),
    _value(0)
  {
    SAY("A429 stream " << ed247_stream_get_name(stream) << " " <<
        "label count: " << _bus.labels.size()
        );
  }

  void push() override {
    a429_word_t word;
    for (const auto& label: _bus.labels) {
      a429_set(word, label.number, label.sdi, _value);
      ASSERT(ed247_stream_push_sample(_stream, word, 4, nullptr, nullptr) == ED247_STATUS_SUCCESS);
    }
    _value = (_value + 1) % 262143;
  }

  a429::bus_t _bus;
  uint32_t _value;
};


// Parse an integer integer (note: broken with negative value)
uint64_t parse_int_parameter(std::string arg, std::string value) {
  try {
    std::size_t last;
    uint64_t res = std::stoi(value, &last);
    if (last != value.size()) throw 42;
    return res;
  } catch (...) {
    DIE("Invalid " << arg << " parameter: '" << value << "'");
  }
}


void usage() {
  SAY("USAGE: chatbot [-p <period_ms>] [-d <duration_ms>] [--fill_depth] <ECIC> [<CMD>]");
  SAY("");
  SAY("  Send data on output streams of provided ECIC file.");
  SAY("  To pollute an AC, you have to provide an 'invertred' ECICI");
  SAY("  The data sent will be the same whatever is the stream kind:");
  SAY("  [UID(4bytes)][counter(1byte)][counter(1byte)]...");
  SAY("  That means, as example, a signal streem will receive strange data in each signals");
  SAY("");
  SAY("  <ECIC>           Full path to ECIC file.");
  SAY("  -p <period_ms>   number: Send all streams each <period_ms> miliseconds. Default: " << period_ms << " ms.");
  SAY("                   'match': Send samples according to period defined in ECIC and CMD files (NOT YET IMPLEMENTED)");
  SAY("  -d <duration_ms> number: Send all streams during <duration_ms> miliseconds. 0 means never stop. Default: " << duration_ms << " ms.");
  SAY("  --fill_depth     Push as mush sample as possible (according to sample_max_number value in ECIC)");
  SAY("                   By default, only one sample is pushed. (i.e. only one A429 on A429 buses, see CMD option.)");
  SAY("                   If CMD is provided, this option does not affect A429 (see CMD below)");
  SAY("  <CMD>            Optionnal full path to CMD file.");
  SAY("                   If provided, A429 buses will be filled with all listed samplings labels, with correct number and SDI.");
  SAY("                   The A429 payload will be a BNR one with a counter as value.");
  SAY("                   The Direction in the CMD is ignored, so you can provide an CMD of a target bridge.");
  SAY("                   A429 queuing will never be pushed.");
  exit(1);
}


int main(int argc, char** argv)
{
  //
  // Parse arguments
  //
  for (int arg_id = 1; arg_id < argc; arg_id++) {
    std::string arg = argv[arg_id];

    if (arg == "-h" || arg == "--help") {
      usage();
    }
    else if (arg == "--fill_depth") {
      fill_depth = true;
    }
    else if (arg == "-p") {
      if (++arg_id >= argc) DIE("-p option require an argument !");
      std::string value = argv[arg_id];
      if (arg == "match") DIE("period = 'match' not yet implemented !");
      period_ms = parse_int_parameter(arg, value);
    }
    else if (arg == "-d") {
      if (++arg_id >= argc) DIE("-d option require an argument !");
      std::string value = argv[arg_id];
      duration_ms = parse_int_parameter(arg, value);
    }
    else if (arg[0] == '-') {
      DIE("Unknown option '" << arg << "'.");
    }
    else {
      if (ecic_path.empty() == false) {
        if (cmd_path.empty() == false) {
          DIE("Unexpected argument '" << arg << "'.");
        }
        cmd_path = arg;
      } else {
        ecic_path = arg;
      }
    }
  }

  //
  // Load CMD if provided
  //
  a429::bus_map_t a429_buses;
  if (cmd_path.empty() == false) {
    load_cmd_file(cmd_path, a429_buses);

    SAY("CMD: Found " << a429_buses.size() << " A429 buses");
    for (const auto& a429_bus : a429_buses) {
      SAY("CMD Bus name: " << a429_bus.first << " label count: " << a429_bus.second.labels.size());
    }
  }

  //
  // Parse ECIC and create transceivers
  //
  ed247_context_t context = nullptr;
  ed247_stream_list_t stream_list = nullptr;
  ASSERT(ed247_load_file(ecic_path.c_str(), &context) == ED247_STATUS_SUCCESS);
  ASSERT(ed247_get_stream_list(context, &stream_list) == ED247_STATUS_SUCCESS);

  uint32_t stream_count = 0;
  ASSERT(ed247_stream_list_size(stream_list, &stream_count) == ED247_STATUS_SUCCESS);
  SAY(stream_count << " streams in this ECIC.");

  using transceiver_list_t = std::vector<std::unique_ptr<transceiver_t>>;
  transceiver_list_t transceiver_list;
  ed247_stream_t stream = nullptr;
  do {
    ASSERT(ed247_stream_list_next(stream_list, &stream) == ED247_STATUS_SUCCESS);
    if (stream == nullptr) break;
    transceiver_t* tranceiver = nullptr;
    if (ed247_stream_get_direction(stream) & ED247_DIRECTION_OUT) {
      if (ed247_stream_get_type(stream) == ED247_STREAM_TYPE_A429) {
        auto ia429_bus = a429_buses.find(ed247_stream_get_name(stream));
        if (ia429_bus != a429_buses.end()) {
          tranceiver = new a429_transceiver_t(stream, ia429_bus->second);
        }
      }
      if (tranceiver == nullptr) {
        tranceiver = new generic_transceiver_t(stream);
      }
      transceiver_list.emplace_back(tranceiver);
    }
  } while (stream != nullptr);


  //
  // Send data
  //
  if (duration_ms) {
    SAY("Begin sending at " << period_ms << " ms during " << duration_ms << " ms.");
  } else {
    SAY("Begin sending at " << period_ms << " ms.");
  }
  uint64_t send_start_time = time_tools::get_monotonic_time_us();
  do {
    uint64_t cycle_start_time = time_tools::get_monotonic_time_us();
    for (auto& itransceiver: transceiver_list) {
      itransceiver->push();
    }
    ed247_send_pushed_samples(context);
    uint64_t cycle_end_time = time_tools::get_monotonic_time_us();
    if (duration_ms && duration_ms * 1000 > cycle_end_time - send_start_time) break;
    time_tools::sleep_us(period_ms * 1000 + cycle_start_time - cycle_end_time);
  } while (true);

  ed247_stream_list_free(stream_list);
  ed247_unload(context);
  return 0;
}
