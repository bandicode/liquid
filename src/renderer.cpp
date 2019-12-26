// Copyright (C) 2019 Vincent Chambrin
// This file is part of the liquid project
// For conditions of distribution and use, see copyright notice in LICENSE

#include "liquid/renderer.h"

#include "liquid/context.h"

namespace liquid
{

Renderer::Renderer()
  : m_strip_whitespace_at_tag(false)
{


}

Renderer::~Renderer()
{
  
}

void Renderer::reset()
{
  m_result.clear();
  context().scopes().clear();
  context().scopes().emplace_back();
  context().flags() = 0;
}

Context& Renderer::context()
{
  return m_context;
}

std::string Renderer::render(const Template& t, const json::Object& data)
{
  reset();
  context().currentScope().data = data;

  for (auto n : t.nodes())
    process(n);

  return m_result;
}

void Renderer::setStripWhitespacesAtTag(bool on)
{
  m_strip_whitespace_at_tag = on;
}

bool Renderer::stripWhiteSpacesAtTag() const
{
  return m_strip_whitespace_at_tag;
}

void Renderer::process(const std::shared_ptr<Template::Node>& n)
{
  auto last_node = m_last_processed_node;
  m_last_processed_node = n;

  if (n->isText())
  {
    if (last_node && last_node->isTag() && stripWhiteSpacesAtTag())
    {
      std::string text = n->as<templates::TextNode>().text;
      Renderer::lstrip(text);
      write(text);
    }
    else
    {
      write(n->as<templates::TextNode>().text);
    }
  }
  else if (n->isObject())
  {
    write(stringify(eval(std::static_pointer_cast<Object>(n))));
  }
  else if (n->isTag())
  {
    if (last_node && last_node->isText() && stripWhiteSpacesAtTag())
    {
      Renderer::rstrip(m_result);
    }

    static_cast<Tag*>(n.get())->accept(*this);
  }
}

std::string Renderer::stringify(const json::Json & val)
{
  if (val.isNull())
    return {};

  if (val.isString())
    return val.toString();
  else if (val.isInteger())
    return StringBackend::from_integer(val.toInt());
  else if (val.isNumber())
    return StringBackend::from_number(val.toNumber());

  throw std::runtime_error("Renderer::stringify() : could not convert to string");
}

void Renderer::write(const std::string& str)
{
  m_result += str;
}

bool Renderer::evalCondition(const json::Json& val)
{
  return val.isBoolean() ? val.toBool() :
    (val.isInteger() ? val.toInt() != 0 : !val.isNull());
}

json::Json Renderer::eval(const std::shared_ptr<Object>& obj)
{
  return obj->accept(*this);
}

json::Json Renderer::eval_value(const objects::Value& val)
{
  return val.value;
}

json::Json Renderer::eval_variable(const objects::Variable& var)
{
  for (int i = static_cast<int>(context().scopes().size()) - 1; i >= 0; --i)
  {
    const json::Object data = context().scopes().at(i).data;
    
    json::Json val = data[var.name];

    if (val != nullptr)
      return val;
  }
  
  return nullptr;
}

json::Json Renderer::eval_memberaccess(const objects::MemberAccess& ma)
{
  const json::Json obj = eval(ma.object);

  if (obj.isArray())
  {
    if (ma.name == "size" || ma.name == "length")
      return json::Json(obj.length());
    else
      return nullptr;
  }
  else if (obj.isObject())
  {
    return obj[ma.name];
  }
  else if (obj.isString())
  {
    if (ma.name == "size" || ma.name == "length")
      return json::Json(static_cast<int>(obj.toString().size()));
    else
      return nullptr;
  }

  return nullptr;
}

json::Json Renderer::eval_arrayaccess(const objects::ArrayAccess & aa)
{
  json::Json obj = eval(aa.object);
  json::Json index = eval(aa.index);

  if (index.isInteger())
  {
    if (obj.isArray())
    {
      return obj.at(index.toInt());
    }
    else
    {
      return nullptr;
    }
  }
  else if (index.isString())
  {
    if (obj.isObject())
    {
      return obj[index.toString()];
    }
    else
    {
      return nullptr;
    }
  }
  else
  {
    throw std::runtime_error{ "Bad array access" };
  }
}

json::Json Renderer::eval_binop(const objects::BinOp & binop)
{
  switch (binop.operation)
  {
  case objects::BinOp::Or:
    return evalCondition(eval(binop.lhs)) || evalCondition(eval(binop.rhs));
  case objects::BinOp::And:
    return evalCondition(eval(binop.lhs)) && evalCondition(eval(binop.rhs));
  default:
    break;
  }

  json::Json lhs = eval(binop.lhs);
  json::Json rhs = eval(binop.rhs);

  switch (binop.operation)
  {
  case objects::BinOp::Equal:
    return lhs == rhs;
  case objects::BinOp::Inequal:
    return lhs != rhs;
  case objects::BinOp::Less:
    return json::compare(lhs, rhs) < 0;
  case objects::BinOp::Leq:
    return json::compare(lhs, rhs) <= 0;
  case objects::BinOp::Greater:
    return json::compare(lhs, rhs) > 0;
  case objects::BinOp::Geq:
    return json::compare(lhs, rhs) >= 0;
  default:
    break;
  }

  assert(false);
  return nullptr;
}

json::Json Renderer::eval_pipe(const objects::Pipe & pipe)
{
  json::Json obj = eval(pipe.object);
  std::vector<json::Json> args = pipe.arguments;

  return applyFilter(pipe.filterName, obj, args);
}

json::Json Renderer::applyFilter(const std::string& name, const json::Json& object, const std::vector<json::Json>& args)
{
  throw std::runtime_error("Unknown filter");
}

void Renderer::process(const std::vector<std::shared_ptr<Template::Node>>& nodes)
{
  for (const auto & n : nodes)
  {
    process(n);

    if (context().flags() != 0)
      return;
  }
}


inline static bool is_space(char c)
{
  return c == ' ' || c == '\r' || c == '\t';
}

void Renderer::lstrip(std::string& str)
{
  size_t i = 0;

  while (i < str.size() && is_space(str.at(i))) ++i;

  if (i < str.size() && str.at(i) == '\n')
    ++i;

  str.erase(0, i);
}

void Renderer::rstrip(std::string& str)
{
  if (str.empty())
    return;

  size_t i = str.size();

  while (i > 0 && is_space(str.at(--i)));

  str.erase(is_space(str.at(i)) ? i : i + 1);
}

void Renderer::visitTag(const Tag& tag)
{
  throw std::runtime_error("Unsupported tag");
}

void Renderer::visitTag(const tags::Assign & assign)
{
  context().currentScope().data[assign.variable] = eval(assign.value);
}

void Renderer::visitTag(const tags::For & tag)
{
  json::Json container = eval(tag.object);

  Context::Scope forloop{ context() };

  if (container.isArray())
  {
    for (int i(0); i < container.length(); ++i)
    {
      forloop[tag.variable] = container.at(i);

      forloop["forloop"]["index"] = i;
      forloop["forloop"]["first"] = (i == 0);
      forloop["forloop"]["last"] = (i == container.length() - 1);

      process(tag.body);

      int rflags = context().flags();
      context().flags() = 0;

      if (rflags & Context::Break)
        return;
    }
  }
  else
  {
    /// TODO:
  }
}

void Renderer::visitTag(const tags::If & tag)
{
  for (size_t i(0); i < tag.blocks.size(); ++i)
  {
    const auto& b = tag.blocks.at(i);

    if (evalCondition(eval(b.condition)))
    {
      process(b.body);
      return;
    }
  }
}

void Renderer::visitTag(const tags::Break & tag)
{
  context().flags() |= Context::Break;
}

void Renderer::visitTag(const tags::Continue & tag)
{
  context().flags() |= Context::Continue;
}

json::Json Renderer::visitObject(const objects::Value& val)
{
  return eval_value(val);
}

json::Json Renderer::visitObject(const objects::Variable& var)
{
  return eval_variable(var);
}

json::Json Renderer::visitObject(const objects::MemberAccess& ma)
{
  return eval_memberaccess(ma);
}

json::Json Renderer::visitObject(const objects::ArrayAccess& aa)
{
  return eval_arrayaccess(aa);
}

json::Json Renderer::visitObject(const objects::BinOp& binop)
{
  return eval_binop(binop);
}

json::Json Renderer::visitObject(const objects::Pipe& pipe)
{
  return eval_pipe(pipe);
}

} // namespace liquid
