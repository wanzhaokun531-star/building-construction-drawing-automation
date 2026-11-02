#include "../src/ProductDiagnostics.h"
#include "../src/PublicProperties.h"
#include <iostream>
int main(){int n=0;
 if(bcpproduct::decodeMessage("cache omitted/added source objects")!=L"cache omitted/added source objects")return 1;++n;
 if(bcpproduct::decodeMessage(u8"输入图纸：方案.dwg")!=L"输入图纸：方案.dwg")return 2;++n;
 auto report=bcpproduct::countDifference({308,63,30},{213,63,30});if(report.find(u8"模型对象") == std::string::npos||report.find("308")==std::string::npos||report.find("213")==std::string::npos)return 3;++n;
 if(!bcpproduct::countDifference({213,63,30},{213,63,30}).empty())return 4;++n;
 report=bcpproduct::countDifference({10,0,0},{213,63,30});if(report.find(u8"墙体")==std::string::npos||report.find(u8"门窗")==std::string::npos)return 5;++n;
 auto fields=bcpproduct::parseProperties(L" 左厚度: 100\r\n 右厚度: 100\n 门窗类型: 窗\n 高度: 1500\n 文字内容: 厨房\n");if(bcpproduct::propertyNumber(fields,L"高度")!=1500||fields.at(L"文字内容")!=L"厨房")return 7;++n;
 bool missing=false;try{bcpproduct::propertyNumber(fields,L"宽度");}catch(...){missing=true;}if(!missing)return 8;++n;
 bool repeated=false;try{bcpproduct::parseProperties(L"高度: 1500\n高度: 1600");}catch(...){repeated=true;}if(!repeated)return 9;++n;
 if(!bcpproduct::sameWindowsFolder(L"C:/SomeFolder/",L"c:\\somefolder"))return 10;++n;
 if(bcpproduct::cacheTextHex(u8"厨房")!="E58EA8E688BF")return 11;++n;
 std::cout<<"diagnostic checks="<<n<<'\n';return n==10?0:6;}
