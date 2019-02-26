#include <stdio.h>
#include <assert.h>
#include <algorithm>
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

bool Validator::validate_map(const YAML::Node &mapNode, const YAML::Node &doc)
{
  if(!doc.IsMap()) {
    string reason = "expected map, but found " + type2str(doc.Type());
    gen_error(Exception(reason, gr_path, doc_path));
    return false;
  }

  bool ok = true;
  for (const auto &i : mapNode) {
    string key = i.first.as<string>();
    const YAML::Node &valueNode = i.second;
    if (!doc[key]) {
      string reason = "key: " + key + " not found.";
      gen_error(Exception(reason, gr_path, doc_path));
      ok = false;
    } else {
      doc_path.push_back(key);
      gr_path.push_back(key);

      ok = validate_doc(valueNode, doc[key]) && ok;

      gr_path.pop_back();
      doc_path.pop_back();
    }
  }
  return ok;
}

bool Validator::validate_leaf(const YAML::Node &gr, const YAML::Node &doc)
{
  assert( gr.IsSequence() );
  const YAML::Node& typespec_map = gr[0];
  assert( typespec_map.size() == 1);

  string type = typespec_map.begin()->first.as<string>();
  const auto type_specifics = typespec_map.begin()->second;

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
    auto docValue = doc;
    ok = std::any_of(type_specifics.begin(), type_specifics.end(), [docValue](const YAML::detail::iterator_value i) { return i == docValue; });
    if (!ok) {
      string reason = "enum string '" + docValue.as<string>() + "' is not allowed.";
      gen_error(Exception(reason, gr_path, doc_path));
    }
  }
  return ok;
}

bool Validator::validate_list(const YAML::Node &gr, const YAML::Node &doc)
{
  if (!doc.IsSequence()) {
    string reason = "expected list, but found " + type2str(doc.Type());
    gen_error(Exception(reason, gr_path, doc_path));
    return false;
  }

  bool ok = true;
  int n = 0;
  char buf[128];

  for (const auto &i : doc) {
    snprintf(buf, sizeof(buf), "[%d]", n);
    doc_path.push_back(buf);
    ok = validate_doc(gr, i) && ok;
    doc_path.pop_back();
    n++;
  }
  return ok;
}

bool Validator::validate_doc(const YAML::Node &gr, const YAML::Node &doc)
{
  bool ok = true;

  if (gr["map"]) {
    gr_path.push_back("map");
    ok = validate_map(gr["map"], doc) && ok;
    gr_path.pop_back();
  } else if (gr["list"]) {
    gr_path.push_back("list");
    ok = validate_list(gr["list"], doc) && ok;
    gr_path.pop_back();
  } else {
    ok = validate_leaf(gr, doc) && ok;
  }

  return ok;
}
