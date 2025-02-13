#ifndef ANALYSIS_H
#define ANALYSIS_H
#include "Pass.h"

namespace Pass {
// 计算相关IR单元的高层信息，但不对其进行修改
// 提供其它pass需要查询的信息并提供查询接口
class Analysis : public Pass {
public:
    explicit Analysis(const std::string &name) : Pass(PassType::ANALYSIS, name) {}

    void run_on(const std::shared_ptr<Mir::Module> module) override {
        // 强制转换为const版本保证只读访问
        const auto const_module = std::const_pointer_cast<const Mir::Module>(module);
        analyze(const_module);
    }

protected:
    // 子类必须实现的纯虚函数（只读版本）
    virtual void analyze(std::shared_ptr<const Mir::Module> module) = 0;
};

// 示例分析Pass
class Example final : public Analysis {
public:
    explicit Example() : Analysis("Example") {}

    void analyze(std::shared_ptr<const Mir::Module> module) override;
};
}

#endif //ANALYSIS_H
