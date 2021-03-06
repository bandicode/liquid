// Copyright (C) 2019-2021 Vincent Chambrin
// This file is part of the liquid project
// For conditions of distribution and use, see copyright notice in LICENSE

#include "liquid/template.h"

#include "liquid/parser.h"
#include "liquid/renderer.h"

#include <fstream>
#include <sstream>

/*!
 * \namespace liquid
 */

namespace liquid
{

namespace templates
{

bool Node::isText() const
{
  return false;
}

bool Node::isTag() const
{
  return false;
}

bool Node::isObject() const
{
  return false;
}

TextNode::TextNode(std::string str, size_t off)
  : Node(off),
    text(std::move(str))
{

}

bool TextNode::isText() const
{
  return true;
}

} // namespace templates

/*!
 * \class Template
 */

Template::Template()
{

}

Template::Template(std::string src, std::vector<std::shared_ptr<templates::Node>> nodes, std::string filepath)
  : mFilePath(std::move(filepath)),
    mSource(std::move(src)),
    mNodes(std::move(nodes))
{

}

Template::~Template()
{

}

/*!
 * \fn const std::string& filePath() const
 * \brief returns the template's file path
 * 
 * This function returns the filepath that was passed through the constructor.
 */
const std::string& Template::filePath() const
{
  return mFilePath;
}

/*!
 * \fn const std::string& source() const
 * \brief returns the original source of the template
 */
const std::string& Template::source() const
{
  return mSource;
}

/*!
 * \fn std::string render(const liquid::Map& data) const
 * \param rendering data
 * \brief renders the template
 * 
 * This function uses the default Renderer.
 */
std::string Template::render(const liquid::Map& data) const
{
  Renderer r;
  return r.render(*this, data);
}

/*!
 * \fn std::pair<int, int> linecol(size_t off) const
 * \param offset in bytes
 * \brief returns the line an column number of the character at a given offset
 */
std::pair<int, int> Template::linecol(size_t off) const
{
  int line = 0;

  for (size_t i(0); i < off; ++i)
  {
    line += source().at(i) == '\n' ? 1 : 0;
  }

  int col = 0;

  while (off > 0 && source().at(off - 1) != '\n')
  {
    --off;
    ++col;
  }

  return { line, col };
}

/*!
 * \fn std::string getLine(size_t off) const
 * \param offset in bytes
 * \brief returns the line containing the character at a given offset
 */
std::string Template::getLine(size_t off) const
{
  size_t begin = off;

  while (begin > 0 && source().at(begin-1) != '\n') --begin;

  size_t end = off;

  while (end < source().size() && source().at(end) != '\n') ++end;

  return std::string(source().begin() + begin, source().begin() + end);
}

inline static bool is_space(char c)
{
  return c == ' ' || c == '\r' || c == '\t';
}

/*!
 * \fn void lstrip(std::string& str) noexcept
 * \param input string
 * \brief removes whitespaces at the beginning of string
 */
void Template::lstrip(std::string& str) noexcept
{
  size_t i = 0;

  while (i < str.size() && is_space(str.at(i))) ++i;

  if (i == str.size() || str.at(i) != '\n')
  {
    str.erase(0, i);
  }
  else
  {
    ++i;
    while (i < str.size() && is_space(str.at(i))) ++i;
    str.erase(0, i);
  }
}

/*!
 * \fn void rstrip(std::string& str) noexcept
 * \param input string
 * \brief removes trailing whitespaces from the string
 */
void Template::rstrip(std::string& str) noexcept
{
  if (str.empty())
    return;

  size_t i = str.size();

  while (i > 0 && is_space(str.at(--i)));

  str.erase(is_space(str.at(i)) ? i : i + 1);
}

static void strip_whitespaces_at_tag(const std::vector<std::shared_ptr<templates::Node>>& nodes, bool strip_first, bool strip_last)
{
  bool prev_was_tag = strip_first;
  bool prev_was_text = false;
  std::shared_ptr<templates::Node> prev_text = nullptr;

  for (auto n : nodes)
  {
    if (n->isText())
    {
      if (prev_was_tag)
      {
        std::string& text = n->as<templates::TextNode>().text;
        Template::lstrip(text);
      }

      prev_was_text = true;
      prev_was_tag = false;
      prev_text = n;
    }
    else if (n->isTag())
    {
      if (prev_was_text)
      {
        Template::rstrip(prev_text->as<templates::TextNode>().text);
      }

      if (n->is<tags::If>())
      {
        for (tags::If::Block& block : n->as<tags::If>().blocks)
        {
          strip_whitespaces_at_tag(block.body, true, true);
        }
      }
      else if (n->is<tags::For>())
      {
        strip_whitespaces_at_tag(n->as<tags::For>().body, true, true);
      }

      prev_was_tag = true;
      prev_was_text = false;
    }
    else
    {
      prev_was_text = false;
      prev_was_tag = false;
    }
  }

  if (prev_was_text && strip_last)
  {
    Template::rstrip(prev_text->as<templates::TextNode>().text);
  }
}

/*!
 * \fn void stripWhitespacesAtTag()
 * \brief removes extra whitespaces from the template
 *
 * This function removes whitespaces before and after each 'tag' node.
 */
void Template::stripWhitespacesAtTag()
{
  strip_whitespaces_at_tag(mNodes, false, false);
}

static void skip_whitespaces_at_tag(const std::vector<std::shared_ptr<templates::Node>>& nodes, bool strip_first)
{
  bool prev_was_tag = strip_first;
  std::shared_ptr<templates::Node> prev_text = nullptr;

  for (auto n : nodes)
  {
    if (n->isText())
    {
      if (prev_was_tag)
      {
        std::string& text = n->as<templates::TextNode>().text;
        Template::lstrip(text);
      }

      prev_was_tag = false;
    }
    else if (n->isTag())
    {
      if (n->is<tags::If>())
      {
        for (tags::If::Block& block : n->as<tags::If>().blocks)
        {
          skip_whitespaces_at_tag(block.body, true);
        }
      }
      else if (n->is<tags::For>())
      {
        skip_whitespaces_at_tag(n->as<tags::For>().body, true);
      }

      prev_was_tag = true;
    }
    else
    {
      prev_was_tag = false;
    }
  }
}

/*!
 * \fn void skipWhitespacesAfterTag()
 * \brief removes extra whitespaces from the template
 * 
 * This function removes whitespaces after each 'tag' node.
 */
void Template::skipWhitespacesAfterTag()
{
  skip_whitespaces_at_tag(mNodes, false);
}

/*!
 * \endclass
 */

/*!
 * \fn Template parse(const std::string& str, std::string filepath = {})
 * \param template source
 * \param optional filepath from which the source was read
 * \brief parse a template
 * \relates Template
 *
 * This function throws ParserException if parsing fails.
 */
Template parse(const std::string& str, std::string filepath)
{
  liquid::Parser lp;
  return Template{ str, lp.parse(str), filepath };
}

/*!
 * \fn Template parseFile(std::string filepath)
 * \param filepath of the template to parse
 * \brief parse a template
 * \relates Template
 *
 * This function uses \c{parse()} internally.
 */
Template parseFile(std::string filepath)
{
  std::ifstream file{ filepath };
  std::stringstream buffer;
  buffer << file.rdbuf();
  return liquid::parse(buffer.str(), std::move(filepath));
}

/*!
 * \endnamespace
 */

} // namespace liquid
