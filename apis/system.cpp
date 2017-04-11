#include "system.h"
#include "model/log.h"

// statics
double SystemApi::_timeout = 5;
bool SystemApi::_gc = false;
bool SystemApi::_bytecode = false;

SystemApi::SystemApi() : LuaProxy("system")
{
    cadd("allowGC", &SystemApi::allowGC);
    cadd("timeout", &SystemApi::timeout);
    cadd("allowBytecode", &SystemApi::allowBytecode);
}

SystemApi* SystemApi::get()
{
    static SystemApi it;
    return &it;
}

int SystemApi::allowGC(lua_State* lua)
{
    return ValuePack::ret(lua, get()->_gc);
}

int SystemApi::timeout(lua_State* lua)
{
    return ValuePack::ret(lua, get()->_timeout);
}

int SystemApi::allowBytecode(lua_State* lua)
{
    return ValuePack::ret(lua, get()->_bytecode);
}

////////
void SystemApi::configure(const Value& settings)
{
    _timeout = settings.get("timeout").Or(_timeout).toNumber();
    _bytecode = settings.get("allowBytecode").Or(_bytecode).toBool();
    _gc = settings.get("allowGC").Or(_gc).toBool();
}
