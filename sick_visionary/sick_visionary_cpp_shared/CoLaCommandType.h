//
// Copyright note: Redistribution and use in source, with or without modification, are permitted.
// 
// Created: October 2018
// 
// @author:  Johan Falk
// SICK AG, Waldkirch
// email: TechSupport0905@sick.de

#pragma once

// Available CoLa command types.
namespace CoLaCommandType
{
  enum Enum
  {
    NETWORK_ERROR = -2,
    UNKNOWN = -1,
    READ_VARIABLE,
    READ_VARIABLE_RESPONSE,
    WRITE_VARIABLE,
    WRITE_VARIABLE_RESPONSE,
    METHOD_INVOCATION,
    METHOD_RETURN_VALUE,
    COLA_ERROR
  };
}