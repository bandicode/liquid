// Copyright (C) 2019-2021 Vincent Chambrin
// This file is part of the liquid project
// For conditions of distribution and use, see copyright notice in LICENSE

#include "liquid/parser.h"

#include "liquid/tags.h"

#include <algorithm>

namespace vec
{

template<typename T>
T take_first(std::vector<T>& list)
{
  T result = list.front();
  list.erase(list.begin());
  return result;
}

template<typename T>
T take_last(std::vector<T>& list)
{
  T result = list.back();
  list.pop_back();
  return result;
}

template<typename T>
std::vector<T> mid(const std::vector<T>& list, size_t offset, size_t n = std::numeric_limits<size_t>::max())
{
  if (n == std::numeric_limits<size_t>::max())
    n = list.size() - offset;

  return std::vector<T>(list.begin() + offset, list.begin() + offset + n);
}


} // namespace vec

namespace liquid
{

ParserException::ParserException(size_t off, const std::string& mssg)
  : offset_(off),
    message_(mssg)
{

}

const char* ParserException::what() const noexcept
{
  return "liquid::Parser parsing error";
}

StringView::StringView()
  : text_(nullptr),
  offset_(0),
  length_(0)
{

}

StringView::StringView(const std::string* str, size_t off, size_t len)
  : text_(str),
  offset_(off),
  length_(len)
{

}

std::string::const_iterator StringView::begin() const
{
  return text_->cbegin() + offset_;
}

std::string::const_iterator StringView::end() const
{
  return text_->cbegin() + offset_ + length_;
}

StringView StringView::mid(size_t off, size_t len) const
{
  return StringView(text_, offset_ + off, std::min(len, length_ - off));
}

char StringView::operator[](size_t off) const
{
  return text_->at(offset_ + off);
}

bool StringView::operator==(const char* str) const
{
  size_t i = 0;

  while (str[i] != '\0' && i < length_)
  {
    if (str[i] != text_->at(offset_ + i))
      return false;
    else
      ++i;
  }

  return str[i] == '\0' && i == length_;
}

std::string Token::toString() const
{
  return std::string(text.begin(), text.end());
}

bool Token::operator==(const char *str) const
{
  return text == str;
}

Tokenizer::Tokenizer()
  : mPosition(0), mStartPos(0)
{
  mPunctuators = std::set<char>{ '!', '<', '>', '=', '+', '-', '*', '/' };
}

std::vector<Token> Tokenizer::tokenize(StringView str)
{
  std::vector<Token> result;
  
  mInput = str;
  mPosition = 0;

  readSpaces();

  while (!atEnd())
    result.push_back(read());

  return result;
}

Token Tokenizer::read()
{
  if (atEnd())
  {
    throw ParserException{ mInput.offset_ + position(), "Unexpected end of input" };
  }

  mStartPos = position();

  char c = peekChar();

  if (c == '|')
    return readChar(), produce(Token::Pipe);
  else   if (c == ':')
    return readChar(), produce(Token::Colon);
  else if (c == '.')
    return readChar(), produce(Token::Dot);
  else if (c == ',')
    return readChar(), produce(Token::Comma);
  else if (c == '[')
    return readChar(), produce(Token::LeftBracket);
  else if (c == ']')
    return readChar(), produce(Token::RightBracket);
  else if (StringBackend::is_digit(c))
    return readIntegerLiteral();
  else if (c == '\'' || c == '"')
    return readStringLiteral();
  else if (StringBackend::is_letter(c) || c == '_')
    return readIdentifier();
  else if (isPunctuator(c))
    return readOperator();
  
  throw ParserException{ mInput.offset_ + position(), std::string("Unexpected input '") + c + std::string("'") };
}

char Tokenizer::readChar()
{
  return mInput[mPosition++];
}

char Tokenizer::peekChar() const
{
  return mInput[mPosition];
}

void Tokenizer::seek(size_t pos)
{
  mPosition = std::min(pos, mInput.length_);
}

bool Tokenizer::isPunctuator(char c) const
{
  return mPunctuators.find(c) != mPunctuators.end();
}

bool Tokenizer::readSpaces()
{
  const size_t offset = position();

  while (!atEnd() && (StringBackend::is_space(peekChar()) || StringBackend::is_newline(peekChar())))
    readChar();

  return position() != offset;
}

Token Tokenizer::produce(Token::Kind k)
{
  const size_t pos = position();
  readSpaces();
  return Token{ k, mInput.mid(mStartPos, pos-mStartPos) };
}

Token Tokenizer::readIdentifier()
{
  auto is_valid = [](char c) -> bool {
    return StringBackend::is_letter_or_number(c) || c == '_';
  };

  while (!atEnd() && is_valid(peekChar()))
    readChar();

  Token ret = produce(Token::Identifier);

  if (ret == "or" || ret == "and" || ret == "xor")
    ret.kind = Token::Operator;
  else if (ret == "true" || ret == "false")
    ret.kind = Token::BooleanLiteral;

  return ret;
}

Token Tokenizer::readIntegerLiteral()
{
  while (!atEnd() && StringBackend::is_digit(peekChar()))
    readChar();

  return produce(Token::IntegerLiteral);
}

Token Tokenizer::readStringLiteral()
{
  const char quote = readChar();
  const size_t index = mInput.text_->find(quote, position() + mInput.offset_);

  if (index == std::string::npos)
    throw ParserException{ mInput.offset_ + position(), std::string("Marlformed string literal") };

  seek(index - mInput.offset_);
  readChar();
  return produce(Token::StringLiteral);
}

Token Tokenizer::readOperator()
{
  if (peekChar() == '<' || peekChar() == '>' || peekChar() == '=')
  {
    char first_char = readChar();
    if (atEnd())
      return produce(Token::Operator);
    else if (peekChar() == '=')
      return readChar(), produce(Token::Operator);
    else if (first_char == '<' && peekChar() == '>')
      return readChar(), produce(Token::Operator);
  }
  else if (peekChar() == '!')
  {
    readChar();
    if (atEnd())
      return produce(Token::Operator);
    else if (peekChar() == '=')
      return readChar(), produce(Token::Operator);
  }
  else
  {
    readChar();
  }

  return produce(Token::Operator);
}

bool Tokenizer::atEnd() const
{
  return mInput.length_ == mPosition;
}

class ObjectParser
{
public:
  std::vector<Token>& tokens;

  explicit ObjectParser(std::vector<Token>& toks)
    : tokens(toks)
  {

  }

  std::shared_ptr<liquid::Object> parse()
  {
    return parseObject();
  }

  Token readOperator()
  {
    Token tok = vec::take_first(tokens);

    if (tok.kind != Token::Operator)
      throw ParserException{ tok.text.offset_ , std::string("Expected operator") };

    return tok;
  }


  static liquid::Value createLiteral(const Token& tok) noexcept
  {
    assert(tok.kind == Token::BooleanLiteral || tok.kind == Token::IntegerLiteral || tok.kind == Token::StringLiteral);

    if (tok.kind == Token::BooleanLiteral)
    {
      return liquid::Value{ tok == "true" };
    }
    else if (tok.kind == Token::IntegerLiteral)
    {
      return liquid::Value{ std::stoi(tok.toString()) };
    }
    else // tok.kind == Token::StringLiteral
    {
      return liquid::Value(std::string(tok.text.begin() + 1, tok.text.end() - 1));
    }
  }

  liquid::Value readLiteral()
  {
    Token tok = vec::take_first(tokens);

    if(tok.kind == Token::BooleanLiteral || tok.kind == Token::IntegerLiteral || tok.kind == Token::StringLiteral)
      return createLiteral(tok);

    throw ParserException{ tok.text.offset_, "expected literal" };
  }

  std::shared_ptr<liquid::Object> readArray(Token tok)
  {
    assert(tok.kind == Token::LeftBracket);

    liquid::Array result;

    while (tokens.front().kind != Token::RightBracket)
    {
      result.push(readLiteral());

      if (tokens.front().kind == Token::Comma)
      {
        vec::take_first(tokens);
      }
    }

    vec::take_first(tokens);

    return std::make_shared<objects::Value>(result, tok.text.offset_);
  }

  std::shared_ptr<liquid::Object> readOperand()
  {
    std::shared_ptr<liquid::Object> obj;

    Token tok = vec::take_first(tokens);

    if (tok.kind == Token::Identifier && tok.text != "not")
      obj = std::make_shared<objects::Variable>(tok.toString(), tok.text.offset_);
    else if (tok.text == "not")
      return std::make_shared<objects::LogicalNot>(readOperand(), tok.text.offset_);
    else if (tok.kind == Token::BooleanLiteral || tok.kind == Token::IntegerLiteral || tok.kind == Token::StringLiteral)
      obj = std::make_shared<objects::Value>(createLiteral(tok), tok.text.offset_);
    else if (tok.kind == Token::LeftBracket)
      obj = readArray(tok);
    else
      throw ParserException{ tok.text.offset_, "Expected operand" };

    while (!tokens.empty())
    {
      if (tokens.front().kind == Token::Dot)
      {
        vec::take_first(tokens);

        if (tokens.empty() || tokens.front().kind != Token::Identifier)
          throw ParserException{ tokens.front().text.offset_, "Expected identifier after '.'" };

        tok = vec::take_first(tokens);
        obj = std::make_shared<objects::MemberAccess>(obj, tok.toString(), tok.text.offset_);
      }
      else if (tokens.front().kind == Token::LeftBracket)
      {
        const Token left_bracket = vec::take_first(tokens);
        std::vector<Token> subtokens;

        while (!tokens.empty() && tokens.front().kind != Token::RightBracket)
        {
          subtokens.push_back(vec::take_first(tokens));
        }

        if (tokens.empty())
          throw ParserException{ left_bracket.text.offset_, "Could not find closing bracket ']'" };

        vec::take_first(tokens);

        if (subtokens.empty())
          throw ParserException{ left_bracket.text.offset_, "Invalid empty index in array access" };

        ObjectParser subobj_parser{ subtokens };
        std::shared_ptr<liquid::Object> index = subobj_parser.parse();
        obj = std::make_shared<objects::ArrayAccess>(obj, index, tok.text.offset_);
      }
      else
      {
        break;
      }
    }

    return obj;
  }

  static std::shared_ptr<liquid::Object> buildExpr(std::vector<std::shared_ptr<liquid::Object>> operands, std::vector<Token> operators)
  {
    struct OpInfo { objects::BinOp::Operation name; int precedence; };

    static std::vector<std::pair<std::string, OpInfo>> map{
      { "or", { objects::BinOp::Or, 6 } },
      { "xor", { objects::BinOp::Xor, 6 } },
      { "and", { objects::BinOp::And, 5 } },
      { "!=", { objects::BinOp::Inequal, 4 } },
      { "<>", { objects::BinOp::Inequal, 4 } },
      { "==", { objects::BinOp::Equal, 4 } },
      { "<", { objects::BinOp::Less, 3 } },
      { "<=", { objects::BinOp::Leq, 3 } },
      { ">", { objects::BinOp::Greater, 3 } },
      { ">=", { objects::BinOp::Geq, 3 } },
      { "+", { objects::BinOp::Add, 2 } },
      { "-", { objects::BinOp::Sub, 2 } },
      { "*", { objects::BinOp::Mul, 1 } },
      { "/", { objects::BinOp::Div, 1 } },
    };

    if (operators.size() == 0)
      return operands.front();

    auto get_info = [](const std::vector<std::pair<std::string, OpInfo>>& map, const Token& optok) -> OpInfo
    {
      return std::find_if(map.begin(), map.end(), [&optok](const std::pair<std::string, OpInfo>& elem) -> bool {
        return optok == elem.first.data();
        })->second;
    };

    size_t op_index = operators.size() - 1;
    OpInfo op_info = get_info(map, operators.back());

    for (size_t i(operators.size() - 1); i-- > 0;)
    {
      OpInfo temp_op_info = get_info(map, operators.at(i));
      if (temp_op_info.precedence > op_info.precedence)
        op_index = i, op_info = temp_op_info;
    }

    auto lhs = buildExpr(vec::mid(operands, 0, op_index + 1), vec::mid(operators, 0, op_index));
    auto rhs = buildExpr(vec::mid(operands, op_index + 1), vec::mid(operators, op_index + 1));

    return std::make_shared<objects::BinOp>(op_info.name, lhs, rhs, operators.at(op_index).text.offset_);
  }

  std::shared_ptr<liquid::Object> applyFilter(std::shared_ptr<liquid::Object> obj)
  {
    Token tok = vec::take_first(tokens);
    tok = vec::take_first(tokens);

    std::string name = tok.toString();

    auto ret = std::make_shared<objects::Pipe>(obj, name, tok.text.offset_);

    if (tokens.empty() || tokens.front().kind == Token::Pipe)
      return ret;

    if (tokens.front().kind != Token::Colon)
      throw ParserException{ tokens.front().text.offset_, "Expected ':' after filter name" };

    vec::take_first(tokens);

    while (!tokens.empty() && tokens.front().kind != Token::Pipe)
    {
      ret->arguments.push_back(readOperand());

      if (tokens.empty() || tokens.front().kind == Token::Pipe)
        break;

      if (tokens.front().kind != Token::Comma)
        throw ParserException{ tokens.front().text.offset_, "Expected ',' or '|' or end of filter expression" };

      // read the comma
      vec::take_first(tokens);
    }

    return ret;
  }

  std::shared_ptr<liquid::Object> parseObject()
  {
    assert(!tokens.empty());

    if (tokens.size() == 1 && tokens.front().kind == Token::Identifier)
      return std::make_shared<objects::Variable>(tokens.front().toString(), tokens.front().text.offset_);

    auto obj = readOperand();

    if (tokens.empty())
      return obj;

    std::vector<std::shared_ptr<liquid::Object>> operands;
    operands.push_back(obj);
    std::vector<Token> operators;

    while (!tokens.empty() && tokens.front().kind != Token::Pipe)
    {
      operators.push_back(readOperator());
      operands.push_back(readOperand());
    }

    obj = buildExpr(operands, operators);

    /* Apply filters */
    while (!tokens.empty() && tokens.front().kind == Token::Pipe)
    {
      obj = applyFilter(obj);
    }

    return obj;
  }

};

Parser::Parser()
  : mPosition(0)
{

}

Parser::~Parser()
{

}

std::vector<std::shared_ptr<liquid::templates::Node>> Parser::parse(const std::string & document)
{
  mDocument = StringBackend::normalize(document);
  mPosition = 0;
  mNodes.clear();
  mStack.clear();

  while (!atEnd())
    readNode();

  return mNodes;
}

void Parser::readNode()
{
  size_t pos = document().find('{', position());
  
  if (pos == std::string::npos || pos == document().length() - 1)
  {
    auto text = std::string(document().begin() + position(), document().end());
    auto ret = std::make_shared<templates::TextNode>(std::move(text), position());
    mPosition = document().length();
    dispatchNode(ret);
    return;
  }

  if (pos == position())
  {
    if (document().at(pos + 1) == '{')
    {
      pos = pos + 2;
      const size_t endpos = document().find("}}", pos);

      if (endpos == std::string::npos)
        throw ParserException{ pos, "Could not match '{{' with a closing '}}'" };

      auto tokens = tokenizer().tokenize(StringView(&document(), pos, endpos - pos));
      auto obj = parseObject(tokens);
      dispatchNode(obj);

      mPosition = endpos + 2;
    }
    else if (document().at(static_cast<size_t>(pos) + 1) == '%')
    {
      pos = pos + 2;
      size_t endpos = document().find("%}", pos);

      if (endpos == std::string::npos)
        throw ParserException{ pos, "Could not match '{%' with a closing '%}'" };

      auto tokens = tokenizer().tokenize(StringView(&document(), pos, endpos - pos));
      processTag(tokens);

      mPosition = endpos + 2;

      // Strips new-line after a tag
      //if (!atEnd() && document().at(position()) == '\n')
      //  mPosition += 1;
    }
    else
    {
      auto text = std::string(document().begin() + position(), document().begin() + (pos + 1));
      auto ret = std::make_shared<templates::TextNode>(std::move(text), position());
      mPosition = pos + 1;
      dispatchNode(ret);
    }
  }
  else
  {
    auto text = std::string(document().begin() + position(), document().begin() + pos);
    auto ret = std::make_shared<templates::TextNode>(std::move(text), position());
    mPosition = pos;
    dispatchNode(ret);
    return;
  }
}

void Parser::dispatchNode(std::shared_ptr<liquid::templates::Node> n)
{
  if (stack().empty())
  {
    mNodes.push_back(n);
  }
  else
  {
    auto top = stack().back();

    const bool is_for = top->is<tags::For>();
    const bool is_if = !is_for && top->is<tags::If>();
    const bool is_capture = !is_for && !is_if && top->is<tags::Capture>();

    assert(is_for || is_if || is_capture);

    if (is_for)
      top->as<tags::For>().body.push_back(n);
    else if (is_if)
      top->as<tags::If>().blocks.back().body.push_back(n);
    else if (is_capture)
      top->as<tags::Capture>().body.push_back(n);
  }
}

void Parser::processTag(std::vector<Token> & tokens)
{
  Token tok = vec::take_first(tokens);

  if (tok == "assign")
    process_tag_assign(tok, tokens);
  else if (tok == "if")
    process_tag_if(tok, tokens);
  else if (tok == "elsif")
    process_tag_elsif(tok, tokens);
  else if (tok == "else")
    process_tag_else(tok, tokens);
  else if (tok == "endif")
    process_tag_endif(tok, tokens);
  else if (tok == "for")
    process_tag_for(tok, tokens);
  else if (tok == "break")
    process_tag_break(tok, tokens);
  else if (tok == "continue")
    process_tag_continue(tok, tokens);
  else if (tok == "endfor")
    process_tag_endfor(tok, tokens);
  else if (tok == "comment")
    process_tag_comment();
  else if (tok == "eject")
    process_tag_eject();
  else if (tok == "discard")
    process_tag_discard();
  else if (tok == "include")
    process_tag_include(tok, tokens);
  else if (tok == "capture")
    process_tag_capture(tok, tokens);
  else if (tok == "endcapture")
    process_tag_endcapture(tok, tokens);
  else if (tok == "newline")
    process_tag_newline(tok, tokens);
  else
    throw ParserException{ tok.text.offset_, "Unknown tag name" };
}

std::shared_ptr<liquid::Object> Parser::parseObject(std::vector<Token> & tokens)
{
  ObjectParser parser{ tokens };
  return parser.parse();
}

void Parser::process_tag_comment()
{
  auto node = std::make_shared<tags::Comment>();
  dispatchNode(node);
}

void Parser::process_tag_eject()
{
  auto node = std::make_shared<tags::Eject>();
  dispatchNode(node);
}

void Parser::process_tag_discard()
{
  auto node = std::make_shared<tags::Discard>();
  dispatchNode(node);
}

void Parser::process_tag_assign(const Token& keyword, std::vector<Token>& tokens)
{
  Token name = vec::take_first(tokens);
  Token eq = vec::take_first(tokens);

  bool parent_scope = false;
  bool global = false;

  if (tokens.back() == "parent_scope")
  {
    tokens.pop_back();
    parent_scope = true;
  }
  else if (tokens.back() == "global")
  {
    tokens.pop_back();
    global = true;
  }

  auto expr = parseObject(tokens);

  auto node = std::make_shared<tags::Assign>(name.toString(), expr, keyword.text.offset_);
  node->parent_scope = parent_scope;
  node->global_scope = global;
  dispatchNode(node);
}

void Parser::process_tag_if(const Token& keyword, std::vector<Token>& tokens)
{
  auto cond = parseObject(tokens);
  auto tag = std::make_shared<tags::If>(cond, keyword.text.offset_);
  mStack.push_back(tag);
}

void Parser::process_tag_elsif(const Token& keyword, std::vector<Token>& tokens)
{
  if (stack().empty() || !stack().back()->is<tags::If>())
    throw ParserException{ keyword.text.offset_, "Unexpected 'elsif' tag" };

  tags::If::Block block;
  block.condition = parseObject(tokens);

  stack().back()->as<tags::If>().blocks.push_back(block);
}

void Parser::process_tag_else(const Token& keyword, std::vector<Token>& tokens)
{
  if (stack().empty() || !stack().back()->is<tags::If>())
    throw ParserException{ keyword.text.offset_, "Unexpected 'else' tag" };

  tags::If::Block block;
  block.condition = std::make_shared<objects::Value>(liquid::Value(true));

  stack().back()->as<tags::If>().blocks.push_back(block);
}

void Parser::process_tag_endif(const Token& keyword, std::vector<Token>& tokens)
{
  if (stack().empty() || !stack().back()->is<tags::If>())
    throw ParserException{ keyword.text.offset_, "Unexpected 'endif' tag" };

  auto node = vec::take_last(mStack);
  assert(node->is<tags::If>());
  dispatchNode(node);
}

void Parser::process_tag_for(const Token& keyword, std::vector<Token>& tokens)
{
  std::string name = vec::take_first(tokens).toString();

  std::string in = vec::take_first(tokens).toString();

  if (in != "in")
    throw ParserException{ keyword.text.offset_, "Expected token 'in'" };

  auto container = parseObject(tokens);

  auto tag = std::make_shared<tags::For>(name, container, keyword.text.offset_);
  mStack.push_back(tag);
}

void Parser::process_tag_break(const Token& keyword, std::vector<Token>& tokens)
{
  dispatchNode(std::make_shared<tags::Break>(keyword.text.offset_));
}

void Parser::process_tag_continue(const Token& keyword, std::vector<Token>& tokens)
{
  dispatchNode(std::make_shared<tags::Continue>(keyword.text.offset_));
}

void Parser::process_tag_endfor(const Token& keyword, std::vector<Token>& tokens)
{
  if (stack().empty() || !stack().back()->is<tags::For>())
    throw ParserException{ keyword.text.offset_, "Unexpected 'endfor' tag" };

  auto node = vec::take_last(mStack);
  assert(node->is<tags::For>());
  dispatchNode(node);
}

class IncludeParser
{
public:
  tags::Include& result;
  std::vector<Token>& tokens;
  size_t index = 0;

  IncludeParser(tags::Include& target, std::vector<Token>& toks) : result(target), tokens(toks)
  {

  }

  bool atEnd() const
  {
    return index == tokens.size();
  }

  Token read()
  {
    if(atEnd())
      throw ParserException{ tokens.back().text.offset_, "unexpected end of input" };

    return tokens.at(index++);
  }

  void parse()
  {
    std::vector<Token> buffer;

    while (!atEnd())
    {
      std::string name = read().toString();

      Token tok = read();

      if (tok.text != "=")
      {
        throw ParserException{ tok.text.offset_, "expected '=' after variable name in 'include'" };
      }

      buffer.clear();

      while (!atEnd())
      {
        tok = read();

        if (tok == "and")
          break;

        buffer.push_back(tok);
      }

      ObjectParser obj_parser{ buffer };
      auto obj = obj_parser.parse();
      result.objects[name] = obj;
    }
  }
};

void Parser::process_tag_include(const Token& keyword, std::vector<Token>& tokens)
{
  if(tokens.empty())
    throw ParserException{ keyword.text.offset_, "'include' should provide a template name" };

  std::string template_name = tokens.front().toString();

  auto result = std::make_shared<tags::Include>(std::move(template_name));
  result->setOffset(keyword.text.offset_);

  tokens.erase(tokens.begin());

  if (!tokens.empty())
  {
    if(tokens.front().toString() != "with")
      throw ParserException{ tokens.front().text.offset_, "expected 'with' keyword after 'include' name" };

    tokens.erase(tokens.begin());

    IncludeParser incparser{ *result, tokens };
    incparser.parse();
  }

  dispatchNode(result);
}

void Parser::process_tag_capture(const Token& keyword, std::vector<Token>& tokens)
{
  std::string name = vec::take_first(tokens).toString();

  auto tag = std::make_shared<tags::Capture>(name, keyword.text.offset_);
  tag->setOffset(keyword.text.offset_);

  mStack.push_back(tag);
}

void Parser::process_tag_endcapture(const Token& keyword, std::vector<Token>& tokens)
{
  if (stack().empty() || !stack().back()->is<tags::Capture>())
    throw ParserException{ keyword.text.offset_, "Unexpected 'endcapture' tag" };

  auto node = vec::take_last(mStack);
  dispatchNode(node);
}

void Parser::process_tag_newline(const Token& keyword, std::vector<Token>& /* tokens */)
{
  dispatchNode(std::make_shared<tags::Newline>(keyword.text.offset_));
}

} // namespace liquid
