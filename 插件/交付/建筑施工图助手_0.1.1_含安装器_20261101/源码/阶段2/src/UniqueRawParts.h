#pragma once
namespace nativeparts {
template<class Array,class Visitor>
void forEachDistinct(const Array& parts,Visitor&& visit){
 for(int i=0;i<parts.length();++i){auto* raw=parts[i];if(!raw)continue;bool duplicate=false;for(int j=0;j<i;++j)if(parts[j]==raw){duplicate=true;break;}if(!duplicate)visit(raw);}
}
}
