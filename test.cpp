extern "C" {
#include "lua.h"
#include "lualib.h"
#include "lauxlib.h"
}

#include <cstdlib>
#include <iostream>
#include <memory>
#include <string>
#include <stdexcept>
#include <sstream>
#include <map>

#include <boost/filesystem/path.hpp>

enum class SettingType {
    DOUBLE, INTEGER, STRING, BOOL, NIL,
};

// Ideally we'd provide something like what json cpp does over these
// raw values... and then give that to a programs constructor for actual
// usage...
struct Setting {
    Setting(const SettingType & t) : type(t) {};
    SettingType type;
    std::shared_ptr<void> value;
};

class ProgramSettingsRunner {
public:
    ProgramSettingsRunner(const boost::filesystem::path & prog_path) : _prog_path(prog_path) {
    }
    std::map<std::string, Setting> run() {
        std::string prog_path = _prog_path.string();
        std::shared_ptr<lua_State> engine(luaL_newstate(), lua_close);
        if (!engine) {
            throw std::runtime_error("Could not create new lua engine!");
        }
        // TODO: limit the accessible libraries.
        luaL_openlibs(engine.get());
        int res = luaL_loadfile(engine.get(), prog_path.c_str());
        if (res != 0) {
            std::stringstream ss;
            ss << "Could not load file: " << prog_path;
            throw std::runtime_error(ss.str());
        }
        res = lua_pcall(engine.get(), 0, 0, 0);
        if (res != 0) {
            std::stringstream ss;
            ss << "Could not run file: " << prog_path;
            throw std::runtime_error(ss.str());
        }
        lua_getglobal(engine.get(), "build");
        lua_pushstring(engine.get(), std::getenv("VEHICLE_NAME"));
        lua_call(engine.get(), 1, 1);
        // TODO: ensure a table is on the stack...
        return convertTable(engine.get());
    }
private:
    boost::filesystem::path _prog_path;
    std::map<std::string, Setting> convertTable(lua_State *L) {
        std::map<std::string, Setting> m;
        lua_pushnil(L);
        while(lua_next(L, -2) != 0) {
            if(lua_isinteger(L, -1)) {
                std::string k = lua_tostring(L, -2);
                Setting s(SettingType::INTEGER);
                s.value = std::shared_ptr<int>(new int(lua_tointeger(L, -1)));
                m.insert({k, s});
            }
            else if(lua_isnumber(L, -1)) {
                std::string k = lua_tostring(L, -2);
                Setting s(SettingType::DOUBLE);
                s.value = std::shared_ptr<double>(new double(lua_tonumber(L, -1)));
                m.insert({k, s});
            }
            else if(lua_isnil(L, -1)) {
                std::string k = lua_tostring(L, -2);
                Setting s(SettingType::NIL);
                m.insert({k, s});
            }
            else if(lua_isboolean(L, -1)) {
                std::string k = lua_tostring(L, -2);
                Setting s(SettingType::BOOL);
                int v = lua_toboolean(L, -1);
                if (v) {
                    s.value = std::shared_ptr<bool>(new bool(true));
                }
                else {
                    s.value = std::shared_ptr<bool>(new bool(false));
                }
                m.insert({k, s});
            }
            else if(lua_isstring(L, -1)) {
                std::string k = lua_tostring(L, -2);
                Setting s(SettingType::STRING);
                s.value = std::shared_ptr<std::string>(new std::string(lua_tostring(L, -1)));
                m.insert({k, s});
            }
            else if(lua_istable(L, -1)) {
                throw std::runtime_error("Nested tables not currently supported!");
            }
            lua_pop(L, 1);
        }
        return m;
    }
};

void printSettings(const std::map<std::string, Setting> & settings) {
    for (const auto & kv : settings) {
        std::cout << kv.first << std::endl;
        switch (kv.second.type) {
        case SettingType::STRING:
            std::cout << "  S:" << *((std::string*)kv.second.value.get()) << std::endl;
            break;
        case SettingType::DOUBLE:
            std::cout << "  D:" << *((double*)kv.second.value.get()) << std::endl;
            break;
        case SettingType::BOOL:
            std::cout << "  B:" << *((bool*)kv.second.value.get()) << std::endl;
            break;
        case SettingType::INTEGER:
            std::cout << "  I:" << *((int*)kv.second.value.get()) << std::endl;
            break;
        case SettingType::NIL:
            std::cout << "  N:" << "<NIL>" << std::endl;
            break;
        };
    }
}

int main(int argc, char**argv) {
    boost::filesystem::path p = argv[1];
    ProgramSettingsRunner psr(p);
    std::map<std::string, Setting> ps = psr.run();
    printSettings(ps);
    return 0;
}
