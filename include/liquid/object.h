// Copyright (C) 2019-2021 Vincent Chambrin
// This file is part of the liquid project
// For conditions of distribution and use, see copyright notice in LICENSE

#ifndef LIQUID_OBJECT_H
#define LIQUID_OBJECT_H

#include "liquid/template.h"

namespace liquid
{

class Renderer;

class LIQUID_API Object : public templates::Node
{
public:
  explicit Object(size_t off = std::numeric_limits<size_t>::max());
  ~Object() = default;

  bool isObject() const override { return true; }
  virtual liquid::Value accept(Renderer& renderer) = 0;
};

} // namespace liquid

#endif // LIQUID_OBJECT_H
