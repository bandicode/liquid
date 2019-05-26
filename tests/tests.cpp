// Copyright (C) 2019 Vincent Chambrin
// This file is part of the liquid project
// For conditions of distribution and use, see copyright notice in LICENSE

#include "liquid/liquid.h"

#include <gtest/gtest.h>

TEST(Liquid, hello) {

  std::string str = "Hello {{ name }}!";

  liquid::Template tmplt = liquid::parse(str);

  json::Object data = {};
  data["name"] = "Alice";
  std::string result = tmplt.render(data);

  ASSERT_EQ(result, "Hello Alice!");
}

TEST(Liquid, greetings) {

  std::string str = "Hi! My name is {{ name }} and I am {{ age }} years old.";

  liquid::Template tmplt = liquid::parse(str);

  json::Object data = {};
  data["name"] = "Bob";
  data["age"] = 18;
  std::string result = tmplt.render(data);

  ASSERT_EQ(result, "Hi! My name is Bob and I am 18 years old.");
}

TEST(Liquid, fruits) {

  std::string str = "I love {% for fruit in fruits %}{{ fruit }}{% if forloop.last == false %}, {% endif %}{% endfor %}!";

  liquid::Template tmplt = liquid::parse(str);

  json::Array fruits;
  fruits.push("apples");
  fruits.push("strawberries");
  fruits.push("bananas");
  json::Object data = {};
  data["fruits"] = fruits;
  std::string result = tmplt.render(data);

  ASSERT_EQ(result, "I love apples, strawberries, bananas!");
}

TEST(Liquid, controlflow) {

  std::string str = "{% for n in numbers %}{% if n > 10 %}{% break %}{% elsif n <= 3 %}{% continue %}{% endif %}{{ n }}{% endfor %}";

  liquid::Template tmplt = liquid::parse(str);

  json::Array numbers;
  numbers.push(1);
  numbers.push(2);
  numbers.push(5);
  numbers.push(4);
  numbers.push(12);
  numbers.push(10);
  json::Object data = {};
  data["numbers"] = numbers;
  std::string result = tmplt.render(data);

  ASSERT_EQ(result, "54");
}

TEST(Liquid, logic) {

  std::string str = "{% if x or y %}1{% endif %}"
    "{% if a >= b %}2{% endif %}"
    "{% if a and b %}3{% endif %}"
    "{% if a != b %}4{% endif %}";

  liquid::Template tmplt = liquid::parse(str);

  json::Object data = {};
  data["x"] = true;
  data["y"] = false;
  data["a"] = 5;
  data["b"] = 10;
  std::string result = tmplt.render(data);

  ASSERT_EQ(result, "134");
}

TEST(Liquid, arrayaccess) {

  std::string str = "{% assign index = 1 %}{{ numbers[index] }}";

  liquid::Template tmplt = liquid::parse(str);

  json::Array numbers;
  numbers.push(1);
  numbers.push(2);
  numbers.push(3);
  json::Object data = {};
  data["numbers"] = numbers;
  std::string result = tmplt.render(data);

  ASSERT_EQ(result, "2");
}

static json::Json createContact(const liquid::String& name, int age, bool restricted = false)
{
  json::Object obj = {};
  obj["name"] = name;
  obj["age"] = age;

  if (restricted)
    obj["private"] = restricted;

  return obj;
}

TEST(Liquid, contacts) {

  std::string str = 
    " There are {{ contacts.length }} contacts."
    " {% for c in contacts %}                  "
    "   {% if c.private %}                     "
    " This contact is private.                 "
    "   {% else %}                             "
    " Contact {{ c['name'] }} ({{ c.age }}).   "
    "   {% endif %}                            "
    " {% endfor %}                             ";

  liquid::Template tmplt = liquid::parse(str);

  json::Array contacts;
  contacts.push(createContact("Bob", 19));
  contacts.push(createContact("Alice", 18));
  contacts.push(createContact("Eve", 22, true));

  json::Object data = {};
  data["contacts"] = contacts;
  std::string result = tmplt.render(data);

  {
    size_t pos = result.find("Eve");
    ASSERT_EQ(pos, std::string::npos);
  }

  {
    size_t pos = result.find("Alice");
    ASSERT_NE(pos, std::string::npos);
  }

  {
    size_t pos = result.find("19");
    ASSERT_NE(pos, std::string::npos);
  }
}

#include "liquid/renderer.h"
#include "liquid/filter.h"

#include <cctype>

class CustomRenderer : public liquid::Renderer
{
public:

  json::Json applyFilter(const liquid::String& name, const json::Json& object, const std::vector<json::Json>& args) override;

  json::Serializer serializer;
};

static liquid::String filter_uppercase(const liquid::String& str)
{
  std::string result = str;
  for (char& c : result)
    c = std::toupper(c);
  return result;
}

static int filter_mul(int x, int y)
{
  return x * y;
}

static std::string filter_substr(std::string str, int pos, int count)
{
  return str.substr(pos, count);
}

json::Json CustomRenderer::applyFilter(const liquid::String& name, const json::Json& object, const std::vector<json::Json>& args)
{
  if(name == "uppercase")
    return liquid::filters::apply(filter_uppercase, object, args, serializer);
  else if(name == "mul")
    return liquid::filters::apply(filter_mul, object, args, serializer);
  else if (name == "substr")
    return liquid::filters::apply(filter_substr, object, args, serializer);

  return Renderer::applyFilter(name, object, args);
}

TEST(Liquid, filters) {

  std::string str = "Hello {{ 'Bob2' | substr: 0, 3 | uppercase }}, your account now contains {{ money | mul: 2 }} dollars.";

  liquid::Template tmplt = liquid::parse(str);

  json::Object data = {};
  data["money"] = 5;
  std::string result = tmplt.render<CustomRenderer>(data);

  ASSERT_EQ(result, "Hello BOB, your account now contains 10 dollars.");
}