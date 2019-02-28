#include <stdio.h>
#include <assert.h>
#include <algorithm>
#include <iostream>
#include <yaml-cpp/yaml.h>
#include "yavl.h"

using namespace std;
using namespace YAVL;

namespace YAVL {
  template <>
  std::string ctype2str<unsigned long long>()
  {
    return "unsigned long long";
  }

  template <>
  std::string ctype2str<string>()
  {
    return "string";
  }

  template <>
  std::string ctype2str<long long>()
  {
    return "long long";
  }

  template <>
  std::string ctype2str<unsigned int>()
  {
    return "unsigned int";
  }

  template <>
  std::string ctype2str<int>()
  {
    return "int";
  }

}

ostream& operator << (ostream& os, const Path& path)
{
  bool f = true;
  for (auto const &i : path) {
    // no dot before list indexes and before first element
    if (!f && i.at(0) != '[') {
      os << '.';
    }
    f = false;
    os << i;
  }
  return os;
}

ostream& operator << (ostream& os, const Exception& v)
{
  os << "REASON: " << v.why << endl;
  os << "  doc path: " << v.doc_path << endl;
  os << "  treespec path: " << v.gr_path << endl;
  os << endl;
  return os;
}

ostream& operator << (ostream& os, const Errors& v)
{
  for (const auto &i: v) {
    os << i;
  }
  return os;
}

const string Validator::schema_err = "Schema malformed: ";
const string Validator::document_err = "Error in document: ";

const string& Validator::type2str(YAML::NodeType::value t)
{
  static string nonestr = "none";
  static string scalarstr = "scalar";
  static string liststr = "list";
  static string mapstr = "map";
  static string undefinedstr = "undefined";

  switch (t) {
    case YAML::NodeType::Null:
      return nonestr;
    case YAML::NodeType::Scalar:
      return scalarstr;
    case YAML::NodeType::Sequence:
      return liststr;
    case YAML::NodeType::Map:
      return mapstr;
    case YAML::NodeType::Undefined:
      return undefinedstr;
  }
  assert(0);
  return nonestr;
}

int Validator::num_keys(const YAML::Node& doc)
{
  if (doc.Type() != YAML::NodeType::Map) {
    return 0;
  }
  return doc.size();
}

bool Validator::get_type(const YAML::Node &grNode, std::string& t) {
  try {
    t = grNode["type"].as<string>();
    return true;
  } catch (const YAML::Exception &e) {
    gen_error(Exception(schema_err + "problem with 'type': " + e.msg, gr_path, doc_path));
    return false;
  }
}

bool Validator::get_key(const YAML::Node &grNode, std::string& key) {
  try {
    key = grNode["key"].as<string>();
    return true;
  } catch (const YAML::Exception &e) {
    gen_error(Exception(schema_err + "problem with 'key': " + e.msg, gr_path, doc_path));
    return false;
  }
}

template<typename T>
bool Validator::get_enum(const YAML::Node &grNode, std::vector<T>& enums) {
  try {
    enums = grNode["enum"]["choices"].as<vector<T>>();
    return true;
  } catch (const YAML::Exception &e) {
    gen_error(Exception(schema_err + "problem with 'enum': " + e.msg, gr_path, doc_path));
    return false;
  }
}

bool Validator::validate_map(const YAML::Node &mapNode, const YAML::Node &doc)
{
  if (!mapNode.IsSequence()) {
    gen_error(Exception("Schema error: \"map\" description is not a sequence", gr_path, doc_path));
    return false;
  }
  if (!doc.IsMap()) {
    string reason = "expected map, but found " + type2str(doc.Type());
    gen_error(Exception(reason, gr_path, doc_path));
    return false;
  }

  bool ok = true;
  for (const auto &mapItem: mapNode) {
    string key, item_type;
    auto got_key = get_key(mapItem, key);
    auto got_type = get_type(mapItem, item_type);
    if (!(got_key && got_type)) {
      ok = false;
      continue;
    }

    if (!doc[key]) {
      gen_error(Exception("required key: " + key + " not found in document.", gr_path, doc_path));
      ok = false;
      continue;
    }

    doc_path.push_back(key);
    gr_path.push_back(item_type);

    if (item_type == "map") {
      ok = validate_map(mapItem["map"], doc[key]) && ok;
    } else if (item_type == "list") {
      ok = validate_list(mapItem["list"], doc[key]) && ok;
    } else {
      // This is where we would validate end nodes like "string" and uint
    }

    gr_path.pop_back();
    doc_path.pop_back();
  }
  return ok;
}

bool Validator::validate_element(const YAML::Node &gr, const YAML::Node &doc)
{
  // Is the schema valid?
  if (!gr.IsMap()) {
    gen_error(Exception(schema_err + "element description is not a map", gr_path, doc_path));
    return false;
  }

  if (!doc.IsScalar()) {
    gen_error(Exception(document_err + "element not found", gr_path, doc_path));
    return false;
  }

  string t;
  if (!get_type(gr, t)) {
    return false;
  }

  if (t == "enum") {
    return check_enum(gr, doc);
  }

  return scalar_is_of_type(doc, t);
}

bool Validator::validate_list(const YAML::Node &gr, const YAML::Node &doc)
{
  // Is the schema valid?
  if (!gr.IsMap()) {
    gen_error(Exception(schema_err + "\"list\" description is not a map", gr_path, doc_path));
    return false;
  }

  // Is the subdocument valid?
  if (!doc.IsSequence()) {
    gen_error(Exception(document_err + "expected list, but found " + type2str(doc.Type()), gr_path, doc_path));
    return false;
  }

  bool ok = true;
  int n = 0;
  char buf[128];

  string t;
  if (!get_type(gr, t)) {
    return false;
  }

  for (const auto &i : doc) {
    snprintf(buf, sizeof(buf), "[%d]", n);
    doc_path.push_back(buf);
    if (t == "map") {
      gr_path.push_back("map");
      ok = validate_map(gr["map"], i) && ok;
      gr_path.pop_back();
    } else if (t == "list") {
      gr_path.push_back("list");
      ok = validate_list(gr["list"], i) && ok;
      gr_path.pop_back();
    } else {
      ok = validate_element(gr, i) && ok;
    }
    doc_path.pop_back();
    n++;
  }
  return ok;
}

bool Validator::validate_doc(const YAML::Node &gr, const YAML::Node &doc)
{
  if (!gr.IsSequence()) {
    gen_error(Exception(schema_err + "it is not a sequence", gr_path, doc_path));
    return false;
  }

  bool ok = true;
  int n = 0;
  char buf[128];

  for (const auto &i: gr) {
    snprintf(buf, sizeof(buf), "[%d]", n);
    gr_path.push_back(buf);

    string t;
    if (!get_type(i, t)) {
      ok = false;
      continue;
    }

    if (t == "map") {
      gr_path.push_back("map");
      ok = validate_map(i["map"], doc) && ok;
      gr_path.pop_back();
    } else if (t == "list") {
      gr_path.push_back("list");
      ok = validate_list(i["list"], doc) && ok;
      gr_path.pop_back();
    } else {
      ok = validate_element(i, doc) && ok;
    }

    gr_path.pop_back();
  }

  return ok;
}
