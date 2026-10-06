#ifndef BALLISTICS_BRIDGE_API_H
#define BALLISTICS_BRIDGE_API_H

#include <memory>
#include <string>

// The app behind one call: UIs (the Kotlin app through JNI, tests) send a
// method name and JSON arguments and get JSON back. It holds what a screen
// session needs — the database, the chosen rifle and cartridge, the current
// conditions and settings — and is built only on applogic, so all of it is
// unit-tested in C++.
//
// Every call answers {"ok": true, "result": ...} or {"ok": false,
// "error": "..."}. Errors are English sentences that UIs use as translation
// keys. Keys are camelCase; angles in the current angle unit ("mrad" |
// "moa"); ids are numbers, 0 = none. Methods are listed in api.cpp.
namespace ballistics::bridge {

class Api {
public:
    Api();
    ~Api();
    Api(const Api&) = delete;
    Api& operator=(const Api&) = delete;

    std::string Call(const std::string& method, const std::string& args_json);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace ballistics::bridge

#endif // BALLISTICS_BRIDGE_API_H
