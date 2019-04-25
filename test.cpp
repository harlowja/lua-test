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
#include <unordered_map>

#include <boost/filesystem/path.hpp>
#include <boost/filesystem/operations.hpp>

enum class ValueType {
    DOUBLE, INTEGER, STRING, BOOL, NIL, TABLE,
};

// Ideally we'd provide something like what json cpp does over these
// raw values... and then give that to a programs constructor for actual
// usage...
//
// See: https://open-source-parsers.github.io/jsoncpp-docs/doxygen/index.html#_example
struct Value {
    Value(const ValueType & t) : type(t) {};
    ValueType type;
    std::shared_ptr<void> value;
};

enum class KeyType {
    STRING, INTEGER,
};

struct Key {
    Key(const KeyType & t) : type(t) {};
    KeyType type;
    std::shared_ptr<void> value;
    bool operator==(const Key &other) const {
        if(this->type != other.type) {
            return false;
        }
        switch (this->type) {
            case KeyType::STRING:
            {
                std::string * me = ((std::string*)this->value.get());
                std::string * other_me = ((std::string*)other.value.get());
                return *me == *other_me;
            }
            case KeyType::INTEGER:
            {
                int * me = ((int*)this->value.get());
                int * other_me = ((int*)other.value.get());
                return *me == *other_me;
            }
        }
        return false;
    }
};

namespace std {
    template <>
    struct hash<Key>
    {
        size_t operator()(const Key& k) const {
            size_t res = (size_t) k.type;
            res = res * 599;
            switch(k.type) {
                case KeyType::STRING:
                {
                    std::string * me = ((std::string*)k.value.get());
                    res = res * std::hash<std::string>{}(*me);
                }
                break;
                case KeyType::INTEGER:
                {
                    int * me = ((int*)k.value.get());
                    res = res * std::hash<int>{}(*me);
                }
                break;
            }
            return res;
        }
    };
}

class ProgramSettingsRunner {
public:
    ProgramSettingsRunner(const std::string & component, const boost::filesystem::path & script_path) : _script_path(script_path), _component(component) {
    }
    std::unordered_map<Key, Value> run() {
        int res;
        std::string script_path = _script_path.string();
        if (!boost::filesystem::is_regular_file(_script_path)) {
            std::stringstream ss;
            ss << "Can not run unknown file: " << script_path;
            throw std::runtime_error(ss.str());
        }
        std::shared_ptr<lua_State> engine(luaL_newstate(), lua_close);
        if (!engine) {
            throw std::runtime_error("Could not create new lua engine!");
        }
        setupEngine(engine.get());
        res = luaL_loadfile(engine.get(), script_path.c_str());
        if (res != 0) {
            std::stringstream ss;
            ss << "Could not load file: " << script_path;
            throw std::runtime_error(ss.str());
        }
        res = lua_pcall(engine.get(), 0, 0, 0);
        if (res != 0) {
            // TODO: handle call errors better...
            std::stringstream ss;
            ss << "Could not run file: " << script_path;
            throw std::runtime_error(ss.str());
        }
        lua_getglobal(engine.get(), "build_configuration");
        char * v = std::getenv("VEHICLE_NAME");
        lua_pushstring(engine.get(), v);
        lua_pushstring(engine.get(), _component.c_str());
        res = lua_pcall(engine.get(), 2, 1, 0);
        if (res != 0) {
            // TODO: handle call errors better...
            std::stringstream ss;
            ss << "Could not run function build() in file: " << script_path;
            throw std::runtime_error(ss.str());
        }
        // TODO: handle call errors...
        // TODO: ensure a table is on the stack...
        return convertTable(engine.get());
    }
private:
    boost::filesystem::path _script_path;
    std::string _component;
    void setupEngine(lua_State * L) {
        // TODO: limit libs opened...
        luaL_openlibs(L);
    }
    Key extractKey(lua_State *L) {
        if(lua_isinteger(L, -2)) {
            Key k(KeyType::INTEGER);
            k.value = std::shared_ptr<int>(new int(lua_tointeger(L, -2)));
            return k;
        }
        else if(lua_isstring(L, -2)) {
            Key k(KeyType::STRING);
            k.value = std::shared_ptr<std::string>(new std::string(lua_tostring(L, -2)));
            return k;
        }
        else {
            throw std::runtime_error("Unknown/unsupported lua key type encountered!");
        }
    }
    std::unordered_map<Key, Value> convertTable(lua_State *L) {
        std::unordered_map<Key, Value> m;
        lua_pushnil(L);
        while(lua_next(L, -2) != 0) {
            if(lua_isinteger(L, -1)) {
                auto k = extractKey(L);
                Value v(ValueType::INTEGER);
                v.value = std::shared_ptr<int>(new int(lua_tointeger(L, -1)));
                m.insert({k, v});
            }
            else if(lua_isnumber(L, -1)) {
                auto k = extractKey(L);
                Value v(ValueType::DOUBLE);
                v.value = std::shared_ptr<double>(new double(lua_tonumber(L, -1)));
                m.insert({k, v});
            }
            else if(lua_isnil(L, -1)) {
                auto k = extractKey(L);
                Value v(ValueType::NIL);
                m.insert({k, v});
            }
            else if(lua_isboolean(L, -1)) {
                auto k = extractKey(L);
                Value v(ValueType::BOOL);
                int bv = lua_toboolean(L, -1);
                if (bv) {
                    v.value = std::shared_ptr<bool>(new bool(true));
                }
                else {
                    v.value = std::shared_ptr<bool>(new bool(false));
                }
                m.insert({k, v});
            }
            else if(lua_isstring(L, -1)) {
                auto k = extractKey(L);
                Value v(ValueType::STRING);
                v.value = std::shared_ptr<std::string>(new std::string(lua_tostring(L, -1)));
                m.insert({k, v});
            }
            else if(lua_istable(L, -1)) {
                auto k = extractKey(L);
                Value v(ValueType::TABLE);
                v.value = std::shared_ptr<std::unordered_map<Key, Value>>(new std::unordered_map<Key, Value>(convertTable(L)));
                m.insert({k, v});
            }
            else {
                throw std::runtime_error("Unknown/unsupported lua value type encountered!");
            }
            lua_pop(L, 1);
        }
        return m;
    }
};

void printSettings(const std::unordered_map<Key, Value> & settings, int indent) {
    std::string prefix(indent, ' ');
    for (const auto & kv : settings) {
        if (kv.first.type == KeyType::STRING) {
            std::cout << prefix << *((std::string*)kv.first.value.get()) << std::endl;
        }
        else {
            std::cout << prefix << *((int*)kv.first.value.get()) << std::endl;
        }
        switch (kv.second.type) {
        case ValueType::STRING:
            std::cout << prefix << "  S:" << *((std::string*)kv.second.value.get()) << std::endl;
            break;
        case ValueType::DOUBLE:
            std::cout << prefix << "  D:" << *((double*)kv.second.value.get()) << std::endl;
            break;
        case ValueType::BOOL:
            std::cout << prefix << "  B:" << *((bool*)kv.second.value.get()) << std::endl;
            break;
        case ValueType::INTEGER:
            std::cout << prefix << "  I:" << *((int*)kv.second.value.get()) << std::endl;
            break;
        case ValueType::NIL:
            std::cout << prefix << "  N:" << "NIL" << std::endl;
            break;
        case ValueType::TABLE:
            printSettings(*((std::unordered_map<Key, Value>*)kv.second.value.get()), indent + 2);
            break;
        };
    }
}

int main(int argc, char**argv) {
    std::string component = argv[1];
    boost::filesystem::path p = argv[2];
    ProgramSettingsRunner psr(component, p);
    auto ps = psr.run();
    std::cout<<"Settings for component: " << component << std::endl;
    std::cout<< "------------------------" << std::endl;
    printSettings(ps, 2);
    return 0;
}
