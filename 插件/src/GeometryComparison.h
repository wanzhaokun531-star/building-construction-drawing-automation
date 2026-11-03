#pragma once
#include <string>
#include <cmath>
#include <cstdlib>
namespace bcpproduct {
// Input is generated public-geometry JSON. String contents (including handles,
// text and layer names) remain exact; numeric coordinates use model tolerance.
inline bool samePublicGeometry(const std::string& a,const std::string& b,double tolerance=1e-6){
 std::size_t i=0,j=0;
 const auto numeric=[](char c){return c=='-'||(c>='0'&&c<='9');};
 while(i<a.size()&&j<b.size()){
  if(a[i]=='"'||b[j]=='"'){
   if(a[i]!='"'||b[j]!='"')return false;
   const auto startI=i++,startJ=j++;bool escaped=false;
   for(;i<a.size();++i){const char c=a[i];if(c=='"'&&!escaped){++i;break;}if(c=='\\'&&!escaped)escaped=true;else escaped=false;}
   escaped=false;for(;j<b.size();++j){const char c=b[j];if(c=='"'&&!escaped){++j;break;}if(c=='\\'&&!escaped)escaped=true;else escaped=false;}
   if(a.substr(startI,i-startI)!=b.substr(startJ,j-startJ))return false;
  }else if(numeric(a[i])||numeric(b[j])){
   if(!numeric(a[i])||!numeric(b[j]))return false;char* ea{};char* eb{};
   const auto va=std::strtod(a.c_str()+i,&ea),vb=std::strtod(b.c_str()+j,&eb);
   if(!std::isfinite(va)||!std::isfinite(vb)||std::abs(va-vb)>tolerance)return false;
   i=static_cast<std::size_t>(ea-a.c_str());j=static_cast<std::size_t>(eb-b.c_str());
  }else{if(a[i++]!=b[j++])return false;}
 }
 return i==a.size()&&j==b.size();
}
}
