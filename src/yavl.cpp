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
    gen_error(Exception("Could not get 'type' from schema: " + e.msg, gr_path, doc_path));
    return false;
  }
}

bool Validator::get_key(const YAML::Node &grNode, std::string& key) {
  try {
    key = grNode["key"].as<string>();
    return true;
  } catch (const YAML::Exception &e) {
    gen_error(Exception("Could not get required key from schema: " + e.msg, gr_path, doc_path));
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
  assert(gr.IsMap());
  assert(doc.IsScalar());

  string t;
  if (!get_type(gr, t)) {
    return false;
  }

  if (t == "string") {
    try {
      auto val = doc.as<string>();
      return true;
    } catch (const YAML::Exception &e) {
      gen_error(Exception("Value in document is not a string: " + e.msg, gr_path, doc_path));
    }
  }

  return false;
}

bool Validator::validate_leaf(const YAML::Node &gr, const YAML::Node &doc)
{
  assert( gr.IsSequence() );
  const YAML::Node& typespec_map = gr[0];
  assert( typespec_map.size() == 1);

  string type = gr[0]["type"].as<string>();
  bool optional_var = gr[0]["optional"].as<bool>(false);

  bool ok = true;
  if (type == "string") {
    attempt_to_convert<string>(doc, ok);
  } else if (type == "uint64") {
    attempt_to_convert<unsigned long long>(doc, ok);
  } else if (type == "int64") {
    attempt_to_convert<long long>(doc, ok);
  } else if (type == "int") {
    attempt_to_convert<int>(doc, ok);
  } else if (type == "uint") {
    attempt_to_convert<unsigned int>(doc, ok);
  } else if (type == "bool") {
    attempt_to_convert<bool>(doc, ok);
  } else if (type == "enum") {
    /* XXX
    auto docValue = doc;
    ok = std::any_of(type_specifics.begin(), type_specifics.end(), [docValue](const YAML::detail::iterator_value i) { return i == docValue; });
    if (!ok) {
      string reason = "enum string '" + docValue.as<string>() + "' is not allowed.";
      gen_error(Exception(reason, gr_path, doc_path));
    }
    */
  }
  return ok;
}

bool Validator::validate_list(const YAML::Node &gr, const YAML::Node &doc)
{
  if (!gr.IsSequence()) {
    gen_error(Exception("Schema error: \"list\" description is not a sequence", gr_path, doc_path));
    return false;
  }
  if(gr.size() != 1) {
    gen_error(Exception("Schema error: \"list\" description sequence's size is not 1 but " + gr.size(), gr_path, doc_path));
    return false;
  }

  if (!doc.IsSequence()) {
    string reason = "expected list, but found " + type2str(doc.Type());
    gen_error(Exception(reason, gr_path, doc_path));
    return false;
  }

  bool ok = true;
  int n = 0;
  char buf[128];

  string t;
  if (!get_type(gr[0], t)) {
    return false;
  }

  for (const auto &i : doc) {
    snprintf(buf, sizeof(buf), "[%d]", n);
    doc_path.push_back(buf);
    ok = validate_element(gr[0], i) && ok;
    doc_path.pop_back();
    n++;
  }
  return ok;
}

bool Validator::validate_doc(const YAML::Node &gr, const YAML::Node &doc)
{
  if (gr.Type() != YAML::NodeType::Sequence) {
    gen_error(Exception("Document is no sequence", gr_path, doc_path));
    return false;
  }

  bool ok = true;
  for (const auto &i: gr) {
    if (!i["type"]) {
      gen_error(Exception("No type field", gr_path, doc_path));
      ok = false;
      continue;
    }

    string node_type;
    try {
      node_type = i["type"].as<string>();
    } catch (const YAML::Exception &e) {
      gen_error(Exception("type field is not a string:" + e.msg, gr_path, doc_path));
      ok = false;
      continue;
    }

    if (node_type == "map") {
      gr_path.push_back("map");
      ok = validate_map(i["map"], doc) && ok;
      gr_path.pop_back();
    } else if (node_type == "list") {
      gr_path.push_back("list");
      ok = validate_list(i["list"], doc) && ok;
      gr_path.pop_back();
    } else {
      ok = validate_leaf(i, doc) && ok;
    }
  }

  return ok;
}
