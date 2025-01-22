
#ifndef IRSTRUCTURE_H
#define IRSTRUCTURE_H
#include <utility>

#include "IRInstruction.h"
#include "IRType.h"
#include "Value.h"

class IRGlobalVariable;
class IRFunction;

class IRModule final : public User {
    std::vector<std::shared_ptr<Use>> globals;
    std::vector<std::shared_ptr<Use>> functions;
    std::vector<std::string> globalString;
    std::shared_ptr<Use> MainFunction;

    public:
    IRModule();
    void addGlobalVariable(std::shared_ptr<IRGlobalVariable> global);
    void addFunction(std::shared_ptr<IRFunction> function);

    void addString(std::string string);
    void setMainFunction(std::shared_ptr<IRFunction> function);

    std::shared_ptr<IRFunction> getMainFunction();
    std::vector<std::shared_ptr<IRFunction>> getFunctions();
    std::shared_ptr<IRGlobalVariable> getGlobalVariable(std::string name);
    std::vector<std::string> getGlobalStrings();
};


class IRInitValue;

class IRGlobalVariable final : public User {
    std::string name;
    std::shared_ptr<IRTYPE::IRType> Type;

    std::shared_ptr<IRInitValue> Init;
    public:
    IRGlobalVariable(std::string name, std::shared_ptr<IRTYPE::IRType> type, std::shared_ptr<IRInitValue> init) : name(std::move(name)), Type(std::move(type)), Init(std::move(init)) {}
};

class IRInitValue final : public User {

};

class IRFParam;
class IRBlock;

class IRFunction final : public User {
    std::string name;
    std::shared_ptr<IRTYPE::IRType> rType;
    std::vector<std::shared_ptr<Use>> params;
    std::vector<std::shared_ptr<IRBlock>> blocks;
    public:
    IRFunction(std::string name, std::shared_ptr<IRTYPE::IRType> type) : name(std::move(name)), rType(std::move(type)) {}
};

class IRBlock final : User {
    int id;
    std::vector<std::shared_ptr<IRInstruction>> instructions;
    public:
    IRBlock();

};
#endif //IRSTRUCTURE_H
