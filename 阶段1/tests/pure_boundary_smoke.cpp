#include "PureCommandBoundary.h"
#include <iostream>
#include <stdexcept>
#include <string>
int main(){
 int failed=0,ran=0,logged=0;std::string message;
 if(!stage1::commandBoundary([&]{++ran;},[&](const char*){++logged;})||ran!=1||logged)++failed;
 if(stage1::commandBoundary([]{throw std::runtime_error("report creation failed");},[&](const char* s){message=s;++logged;})||message!="report creation failed"||logged!=1)++failed;
 if(stage1::commandBoundary([]{throw 7;},[&](const char* s){message=s;++logged;})||message!="unknown native command exception"||logged!=2)++failed;
 if(stage1::commandBoundary([]{throw std::runtime_error("I/O failure");},[](const char*){throw 8;}))++failed;
 std::cout<<"compiler="<<_MSC_VER<<" assertions=4 failed="<<failed<<"\n";return failed?1:0;
}
