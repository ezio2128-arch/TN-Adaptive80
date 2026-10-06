#pragma once
#include "Adaptive/Engine.hpp"
#include <winrt/Windows.Data.Json.h>
#include <filesystem>
#include <fstream>
#include <sstream>
namespace fp {
using namespace winrt::Windows::Data::Json;
inline Config parseConfig(const JsonObject& o){
    Config c;c.minimum=o.GetNamedNumber(L"minimum",30);c.maximum=o.GetNamedNumber(L"maximum",120);
    c.manual=o.GetNamedNumber(L"manual",117);auto mode=o.GetNamedString(L"mode",L"Adaptive");
    if(mode!=L"Manual"&&mode!=L"Adaptive")throw std::invalid_argument("invalid mode");
    c.mode=mode==L"Manual"?Mode::Manual:Mode::Adaptive;
    auto response=o.GetNamedString(L"response",L"Balanced");
    if(response!=L"Smooth"&&response!=L"Balanced"&&response!=L"Responsive")throw std::invalid_argument("invalid response");
    c.response=response==L"Smooth"?Response::Smooth:response==L"Responsive"?Response::Responsive:Response::Balanced;
    c.validate();return c;
}
inline JsonObject configJson(Config c){
    JsonObject o;o.SetNamedValue(L"minimum",JsonValue::CreateNumberValue(c.minimum));
    o.SetNamedValue(L"maximum",JsonValue::CreateNumberValue(c.maximum));o.SetNamedValue(L"manual",JsonValue::CreateNumberValue(c.manual));
    o.SetNamedValue(L"mode",JsonValue::CreateStringValue(c.mode==Mode::Manual?L"Manual":L"Adaptive"));
    o.SetNamedValue(L"response",JsonValue::CreateStringValue(c.response==Response::Smooth?L"Smooth":c.response==Response::Responsive?L"Responsive":L"Balanced"));return o;
}
class Store {
    std::filesystem::path directory_;
    std::filesystem::path file(std::wstring name)const {
        name=std::filesystem::path(name).filename().wstring();
        if(name.empty()||name==L"."||name==L".."||name.find_first_of(L"<>:\"/\\|?*")!=std::wstring::npos)
            throw std::invalid_argument("invalid profile name");
        return directory_/(name+L".json");
    }
public:
    explicit Store(std::filesystem::path directory):directory_(std::move(directory)){std::filesystem::create_directories(directory_);}
    Config load(const std::wstring& exe){
        auto path=file(exe);std::ifstream in(path);if(!in)return {};
        std::ostringstream text;text<<in.rdbuf();
        if(text.str().size()>65536)throw std::runtime_error("profile too large");
        return parseConfig(JsonObject::Parse(winrt::to_hstring(text.str())));
    }
    void save(const std::wstring& exe,Config c){
        c.validate();auto path=file(exe),temp=path;temp+=L".tmp";
        {std::ofstream out(temp,std::ios::binary|std::ios::trunc);out<<winrt::to_string(configJson(c).Stringify());
         if(!out)throw std::runtime_error("profile write failed");}
        if(!MoveFileExW(temp.c_str(),path.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH))
            throw std::runtime_error("profile commit failed");
    }
};
}
