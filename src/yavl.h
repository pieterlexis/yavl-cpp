#pragma once

#include <yaml-cpp/yaml.h>
#include <vector>
#include <string>
#include <ostream>
#include <algorithm>

namespace YAVL
{

  typedef std::vector<std::string> Path;

  // really sucks that I have to do this sort of crap since I can't
  // pass a type as an argument to a function.
  template <typename T>
  std::string ctype2str()
  {
    return "FAIL";
  }

  class Exception {
  public:
    std::string why;
    Path gr_path;
    Path doc_path;
    Exception(const std::string _why,
      const Path& _gr_path,
      const Path& _doc_path) :
        why(_why), gr_path(_gr_path), doc_path(_doc_path) {};
  };

  typedef std::vector<Exception> Errors;

  class Validator {
    const YAML::Node& gr;
    const YAML::Node& doc;
    Path gr_path;
    Path doc_path;
    Errors errors;

    int num_keys(const YAML::Node& doc);
    const std::string& type2str(YAML::NodeType::value t);
    bool validate_doc(const YAML::Node &gr, const YAML::Node &doc);
    bool validate_map(const YAML::Node &mapNode, const YAML::Node &doc);
    bool validate_list(const YAML::Node &gr, const YAML::Node &doc);
    bool validate_element(const YAML::Node &gr, const YAML::Node &doc);

    void gen_error(const Exception& err) {
      errors.push_back(err);
    }

    // Returns true if the key was found and "key" is filled with the value
    // Returns false otherwise (and has added an error)
    bool get_key(const YAML::Node &grNode, std::string& key);
    bool get_type(const YAML::Node &grNode, std::string& t);
    template<typename T>
    bool get_enum(const YAML::Node &grNode, std::vector<T>& enums);

    static const std::string schema_err;
    static const std::string document_err;

    template<typename T>
    bool get_scalar(const YAML::Node &scalar_node, T &s)
    {
      try {
        s = scalar_node.as<T>();
        return true;
      } catch (const YAML::Exception &e) {
        gen_error(Exception(document_err + "problem with enum: " + e.msg, gr_path, doc_path));
        return false;
      }
    };

    template<typename T>
    bool check_enum_contains(const std::vector<T> &enums, const T &e) {
      if (!std::any_of(enums.begin(), enums.end(), [e](const T &i) { return i == e; })) {
        gen_error(Exception(document_err + "enum value is not in the list of allowed values", gr_path, doc_path));
        return false;
      }
      return true;
    };

    template<typename T>
    bool scalar_is_of_type(const YAML::Node& scalar_node) {
      try {
        scalar_node.as<T>();
        return true;
      } catch (const YAML::Exception& e) {
        gen_error(Exception(document_err + "scalar is not of type " + YAVL::ctype2str<T>() + ", but " + type2str(scalar_node.Type()), gr_path, doc_path));
        return false;
      }
    }

    template<typename T>
    bool check_enum(const YAML::Node &gr, const YAML::Node &doc) {
      std::vector<T> enums;
      if (!get_enum<T>(gr, enums)) {
        return false;
      }
      T e;
      if (!get_scalar<T>(doc, e)) {
        return false;
      }
      return check_enum_contains<T>(enums, e);
    }

    bool check_enum(const YAML::Node &gr, const YAML::Node &doc) {
      std::string enum_type;
      if (!get_type(gr["enum"], enum_type)) {
        return false;
      }
      if (enum_type == "string") {
        return check_enum<std::string>(gr, doc);
      } else if (enum_type == "uint64") {
        return check_enum<uint64_t>(gr, doc);
      } else if (enum_type == "int64") {
        return check_enum<int64_t>(gr, doc);
      } else if (enum_type == "uint32") {
        return check_enum<uint32_t>(gr, doc);
      } else if (enum_type == "int32") {
        return check_enum<int32_t>(gr, doc);
      }

      return false;
    };

    bool scalar_is_of_type(const YAML::Node& scalar_node, const std::string &t) {
      if (t == "string") {
        return scalar_is_of_type<std::string>(scalar_node);
      } else if (t == "uint64") {
        return scalar_is_of_type<uint64_t>(scalar_node);
      } else if (t == "int64") {
        return scalar_is_of_type<int64_t>(scalar_node);
      } else if (t == "uint32") {
        return scalar_is_of_type<uint32_t>(scalar_node);
      } else if (t == "int32") {
        return scalar_is_of_type<int32_t>(scalar_node);
      } else if (t == "bool") {
        return scalar_is_of_type<bool>(scalar_node);
      }
      return false;
    };

  public:
    Validator(const YAML::Node& _gr, const YAML::Node& _doc) :
      gr(_gr), doc(_doc) {};
    bool validate() {
      return validate_doc(gr, doc);
    }
    const Errors& get_errors() {
      return errors;
    }
  };
}

std::ostream& operator << (std::ostream& os, const YAVL::Path& path);
std::ostream& operator << (std::ostream& os, const YAVL::Exception& v);
std::ostream& operator << (std::ostream& os, const YAVL::Errors& v);
