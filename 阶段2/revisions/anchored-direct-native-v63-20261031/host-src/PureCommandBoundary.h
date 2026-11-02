#pragma once
#include <exception>
#include <utility>
namespace stage1 {
template<class Operation,class Logger>
bool commandBoundary(Operation&& operation,Logger&& logger) noexcept {
 try{std::forward<Operation>(operation)();return true;}
 catch(const std::exception& e){try{logger(e.what());}catch(...){}}
 catch(...){try{logger("unknown native command exception");}catch(...){}}
 return false;
}
}
