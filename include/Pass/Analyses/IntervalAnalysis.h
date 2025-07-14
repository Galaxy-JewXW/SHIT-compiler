#ifndef INTERVALANALYSIS_H
#define INTERVALANALYSIS_H

#include <cmath>
#include <limits>
#include <optional>
#include <type_traits>
#include <variant>

#include "FunctionAnalysis.h"
#include "LoopAnalysis.h"
#include "Mir/Const.h"
#include "Pass/Analysis.h"
#include "Pass/Analyses/ControlFlowGraph.h"

namespace Pass {
template<typename T>
struct numeric_limits_v {
    static_assert(std::is_same_v<T, int> || std::is_same_v<T, double>, "Only support int or double");
    static constexpr bool has_infinity = std::numeric_limits<T>::has_infinity;
    static constexpr T infinity =
            std::numeric_limits<T>::has_infinity ? std::numeric_limits<T>::infinity() : std::numeric_limits<T>::max();
    static constexpr T neg_infinity = std::numeric_limits<T>::has_infinity
                                          ? -std::numeric_limits<T>::infinity()
                                          : std::numeric_limits<T>::lowest();
    static constexpr T max = std::numeric_limits<T>::max();
    static constexpr T lowest = std::numeric_limits<T>::lowest();
};

// 参见：王雅文,宫云战,肖庆,等. 基于抽象解释的变量值范围分析及应用[J]. 电子学报,2011,39(2):296-303.
class IntervalAnalysis final : public Analysis {
public:
    IntervalAnalysis() :
        Analysis("IntervalAnalysis") {}

    // 代表一个闭区间 [lower, upper]
    template<typename T>
    struct Interval;

    // 数值型区间集
    template<typename T>
    struct IntervalSet;

    using AnyIntervalSet = std::variant<IntervalSet<int>, IntervalSet<double>>;

    struct Summary;

    class SummaryManager;

    class Context;

protected:
    void analyze(std::shared_ptr<const Mir::Module> module) override;

private:
    Summary rabai_function(const std::shared_ptr<Mir::Function> &func);

    std::shared_ptr<ControlFlowGraph> cfg_info{nullptr};

    std::shared_ptr<FunctionAnalysis> func_info{nullptr};

    std::shared_ptr<LoopAnalysis> loop_info{nullptr};
};

template<typename T>
struct IntervalAnalysis::Interval {
    T lower;
    T upper;

    // 默认构造函数
    Interval(const T l, const T u) :
        lower(l), upper(u) {}

    // 用于排序和查找
    bool operator<(const Interval &other) const { return lower < other.lower; }

    bool operator==(const Interval &other) const {
        if constexpr (std::is_floating_point_v<T>) {
            constexpr T epsilon = std::numeric_limits<T>::epsilon();
            return std::abs(lower - other.lower) <= epsilon * std::max(std::abs(lower), std::abs(other.lower)) &&
                   std::abs(upper - other.upper) <= epsilon * std::max(std::abs(upper), std::abs(other.upper));
        } else {
            return lower == other.lower && upper == other.upper;
        }
    }

    bool operator!=(const Interval &other) const { return !(*this == other); }

    // 判断两个区间是否相交或相邻
    [[nodiscard]] bool intersects_or_adjacent(const Interval &other) const {
        if constexpr (std::is_integral_v<T>) {
            // 对整数来说，[1, 2] 和 [3, 4] 是相邻的
            return std::max(lower, other.lower) <= std::min(upper, other.upper) + 1;
        } else {
            // 对浮点数来说，只关心是否重叠
            return std::max(lower, other.lower) <= std::min(upper, other.upper);
        }
    }

    // 合并两个相交或相邻的区间
    void merge(const Interval &other) {
        lower = std::min(lower, other.lower);
        upper = std::max(upper, other.upper);
    }

    Interval operator|(const Interval &other) {
        return Interval{std::min(lower, other.lower), std::max(upper, other.upper)};
    }

    // 方便打印输出
    [[nodiscard]] std::string to_string() const {
        std::stringstream ss;
        ss << "[";
        if constexpr (numeric_limits_v<T>::has_infinity) {
            if (lower == numeric_limits_v<T>::neg_infinity)
                ss << "-inf";
            else
                ss << lower;
        } else {
            ss << lower;
        }
        ss << ", ";
        if constexpr (numeric_limits_v<T>::has_infinity) {
            if (upper == numeric_limits_v<T>::infinity)
                ss << "+inf";
            else
                ss << upper;
        } else {
            ss << upper;
        }
        ss << "]";
        return ss.str();
    }
};

template<typename T>
struct IntervalAnalysis::IntervalSet {
private:
    std::vector<Interval<T>> intervals_;
    bool is_undefined_;

    template<typename U>
    friend struct IntervalSet;

    void normalize() {
        if (is_undefined_ || intervals_.size() <= 1) {
            return;
        }

        std::sort(intervals_.begin(), intervals_.end());

        std::vector<Interval<T>> merged;
        merged.push_back(intervals_[0]);

        for (size_t i = 1; i < intervals_.size(); ++i) {
            if (merged.back().intersects_or_adjacent(intervals_[i])) {
                merged.back().merge(intervals_[i]);
            } else {
                merged.push_back(intervals_[i]);
            }
        }
        intervals_ = merged;
    }

public:
    // 默认构造函数，创建一个空集
    IntervalSet() :
        is_undefined_(false) {}

    // 从单个区间构造
    IntervalSet(T lower, T upper) :
        is_undefined_(false) {
        if (lower <= upper) {
            intervals_.emplace_back(lower, upper);
        }
    }

    explicit IntervalSet(T constant) :
        is_undefined_(false) {
        intervals_.emplace_back(constant, constant);
    }

    // 创建 "Top" 元素 T_N (最大范围)
    static IntervalSet make_any() {
        T min_val = numeric_limits_v<T>::neg_infinity;
        T max_val = numeric_limits_v<T>::infinity;
        return IntervalSet(min_val, max_val);
    }

    // 创建 "Undefined" 元素 X_N
    static IntervalSet make_undefined() {
        IntervalSet is;
        is.is_undefined_ = true;
        return is;
    }

    [[nodiscard]] bool is_undefined() const { return is_undefined_; }

    [[nodiscard]] bool is_empty() const { return !is_undefined_ && intervals_.empty(); }

    // 区间集的并集运算
    IntervalSet &union_with(const IntervalSet &other) {
        if (other.is_undefined()) {
            return *this;
        }
        if (this->is_undefined()) {
            *this = other;
            return *this;
        }

        intervals_.insert(intervals_.end(), other.intervals_.begin(), other.intervals_.end());
        normalize();
        return *this;
    }

    // 区间集的交集运算
    IntervalSet &intersect_with(const IntervalSet &other) {
        if (other.is_undefined()) {
            return *this;
        }
        if (this->is_undefined()) {
            *this = other;
            return *this;
        }

        IntervalSet result;
        for (const auto &i1: intervals_) {
            for (const auto &i2: other.intervals_) {
                T lower = std::max(i1.lower, i2.lower);
                T upper = std::min(i1.upper, i2.upper);
                if (lower <= upper) {
                    result.intervals_.emplace_back(lower, upper);
                }
            }
        }
        *this = result;
        // 交集结果本身就是不相交的，无需 normalize
        return *this;
    }

    // 区间集的拓宽运算 (∇)
    IntervalSet &widen(const IntervalSet &other) {
        if (other.is_undefined() || other.is_empty()) {
            return *this;
        }
        if (this->is_empty() || this->is_undefined()) {
            *this = other;
            return *this;
        }

        // IS1 ∇ IS2 = [min(IS1), max(IS1)] ∇ [min(IS2), max(IS2)]
        T min1 = intervals_.front().lower;
        T max1 = intervals_.back().upper;
        T min2 = other.intervals_.front().lower;
        T max2 = other.intervals_.back().upper;

        T new_lower = min2 < min1 ? numeric_limits_v<T>::neg_infinity : min1;
        T new_upper = max2 > max1 ? numeric_limits_v<T>::infinity : max1;

        intervals_ = {Interval<T>(new_lower, new_upper)};
        is_undefined_ = false;

        return *this;
    }

    // 区间集的四则运算
    IntervalSet operator+(const IntervalSet &other) const {
        if (this->is_undefined() || other.is_undefined()) {
            return IntervalSet::make_undefined();
        }
        IntervalSet result;
        if (this->is_empty() || other.is_empty()) {
            return result; // 空集
        }
        for (const auto &i1: intervals_) {
            for (const auto &i2: other.intervals_) {
                result.intervals_.emplace_back(i1.lower + i2.lower, i1.upper + i2.upper);
            }
        }
        result.normalize();
        return result;
    }

    IntervalSet operator-(const IntervalSet &other) const {
        if (this->is_undefined() || other.is_undefined()) {
            return IntervalSet::make_undefined();
        }
        IntervalSet result;
        if (this->is_empty() || other.is_empty()) {
            return result; // 空集
        }
        for (const auto &i1: intervals_) {
            for (const auto &i2: other.intervals_) {
                result.intervals_.emplace_back(i1.lower - i2.upper, i1.upper - i2.lower);
            }
        }
        result.normalize();
        return result;
    }

    IntervalSet operator*(const IntervalSet &other) const {
        if (this->is_undefined() || other.is_undefined()) {
            return IntervalSet::make_undefined();
        }
        IntervalSet result;
        if (this->is_empty() || other.is_empty()) {
            return result; // 空集
        }
        for (const auto &i1: intervals_) {
            for (const auto &i2: other.intervals_) {
                std::initializer_list<T> list{i1.lower * i2.lower, i1.lower * i2.upper, i1.upper * i2.lower,
                                              i1.upper * i2.upper};
                T lower_val = std::min(list);
                T upper_val = std::max(list);
                result.intervals_.emplace_back(lower_val, upper_val);
            }
        }
        result.normalize();
        return result;
    }

    IntervalSet operator/(const IntervalSet &other) const {
        if (this->is_undefined() || other.is_undefined()) {
            return IntervalSet::make_undefined();
        }
        IntervalSet result;
        if (this->is_empty() || other.is_empty()) {
            return result; // 空集
        }
        for (const auto &i1: intervals_) {
            for (const auto &i2: other.intervals_) {
                // 除法需要处理除零和符号问题
                if (i2.lower <= 0 && i2.upper >= 0) {
                    // 除数区间包含0，结果可能包含无穷大
                    result.intervals_.emplace_back(numeric_limits_v<T>::neg_infinity, numeric_limits_v<T>::infinity);
                } else {
                    std::initializer_list<T> list{i1.lower / i2.lower, i1.lower / i2.upper, i1.upper / i2.lower,
                                                  i1.upper / i2.upper};
                    T lower_val = std::min(list);
                    T upper_val = std::max(list);
                    result.intervals_.emplace_back(lower_val, upper_val);
                }
            }
        }
        result.normalize();
        return result;
    }

    // 取模运算 (仅适用于整数)
    template<typename U = T>
    std::enable_if_t<std::is_same_v<U, int>, IntervalSet> operator%(const IntervalSet &other) const {
        if (this->is_undefined() || other.is_undefined()) {
            return IntervalSet::make_undefined();
        }
        IntervalSet result;
        if (this->is_empty() || other.is_empty()) {
            return result; // 空集
        }

        for ([[maybe_unused]] const auto &i1: intervals_) {
            for (const auto &i2: other.intervals_) {
                // 取模运算的特殊情况处理
                if (i2.lower <= 0 && i2.upper >= 0) {
                    // 除数区间包含0，结果不确定
                    result.intervals_.emplace_back(numeric_limits_v<T>::neg_infinity, numeric_limits_v<T>::infinity);
                } else if (i2.lower > 0) {
                    // 正数除数
                    T lower_val = 0;
                    T upper_val = i2.upper - 1;
                    result.intervals_.emplace_back(lower_val, upper_val);
                } else if (i2.upper < 0) {
                    // 负数除数
                    T lower_val = i2.upper + 1;
                    T upper_val = 0;
                    result.intervals_.emplace_back(lower_val, upper_val);
                }
            }
        }
        result.normalize();
        return result;
    }

    // 位与运算 (仅适用于整数)
    template<typename U = T>
    std::enable_if_t<std::is_same_v<U, int>, IntervalSet> operator&(const IntervalSet &other) const {
        if (this->is_undefined() || other.is_undefined()) {
            return IntervalSet::make_undefined();
        }
        IntervalSet result;
        if (this->is_empty() || other.is_empty()) {
            return result; // 空集
        }

        for (const auto &i1: intervals_) {
            for (const auto &i2: other.intervals_) {
                // 位与运算的保守估计
                // 对于区间 [a, b] & [c, d]，结果的下界是 min(a & c, a & d, b & c, b & d)
                // 上界是 max(a & c, a & d, b & c, b & d)
                std::initializer_list<T> list{i1.lower & i2.lower, i1.lower & i2.upper, i1.upper & i2.lower,
                                              i1.upper & i2.upper};
                T lower_val = std::min(list);
                T upper_val = std::max(list);
                result.intervals_.emplace_back(lower_val, upper_val);
            }
        }
        result.normalize();
        return result;
    }

    // 位或运算 (仅适用于整数)
    template<typename U = T>
    std::enable_if_t<std::is_same_v<U, int>, IntervalSet> operator|(const IntervalSet &other) const {
        if (this->is_undefined() || other.is_undefined()) {
            return IntervalSet::make_undefined();
        }
        IntervalSet result;
        if (this->is_empty() || other.is_empty()) {
            return result; // 空集
        }

        for (const auto &i1: intervals_) {
            for (const auto &i2: other.intervals_) {
                // 位或运算的保守估计
                std::initializer_list<T> list{i1.lower | i2.lower, i1.lower | i2.upper, i1.upper | i2.lower,
                                              i1.upper | i2.upper};
                T lower_val = std::min(list);
                T upper_val = std::max(list);
                result.intervals_.emplace_back(lower_val, upper_val);
            }
        }
        result.normalize();
        return result;
    }

    // 位异或运算 (仅适用于整数)
    template<typename U = T>
    std::enable_if_t<std::is_same_v<U, int>, IntervalSet> operator^(const IntervalSet &other) const {
        if (this->is_undefined() || other.is_undefined()) {
            return IntervalSet::make_undefined();
        }
        IntervalSet result;
        if (this->is_empty() || other.is_empty()) {
            return result; // 空集
        }

        for (const auto &i1: intervals_) {
            for (const auto &i2: other.intervals_) {
                // 位异或运算的保守估计
                std::initializer_list<T> list{i1.lower ^ i2.lower, i1.lower ^ i2.upper, i1.upper ^ i2.lower,
                                              i1.upper ^ i2.upper};
                T lower_val = std::min(list);
                T upper_val = std::max(list);
                result.intervals_.emplace_back(lower_val, upper_val);
            }
        }
        result.normalize();
        return result;
    }

    // std::max
    [[nodiscard]] IntervalSet max(const IntervalSet &other) const {
        if (this->is_undefined() || other.is_undefined()) {
            return IntervalSet::make_undefined();
        }
        IntervalSet result;
        if (this->is_empty() || other.is_empty()) {
            return result; // 空集
        }

        for (const auto &i1: intervals_) {
            for (const auto &i2: other.intervals_) {
                // max([a, b], [c, d]) = [max(a, c), max(b, d)]
                T lower_val = std::max(i1.lower, i2.lower);
                T upper_val = std::max(i1.upper, i2.upper);
                result.intervals_.emplace_back(lower_val, upper_val);
            }
        }
        result.normalize();
        return result;
    }

    // std::min
    [[nodiscard]] IntervalSet min(const IntervalSet &other) const {
        if (this->is_undefined() || other.is_undefined()) {
            return IntervalSet::make_undefined();
        }
        IntervalSet result;
        if (this->is_empty() || other.is_empty()) {
            return result; // 空集
        }

        for (const auto &i1: intervals_) {
            for (const auto &i2: other.intervals_) {
                // min([a, b], [c, d]) = [min(a, c), min(b, d)]
                T lower_val = std::min(i1.lower, i2.lower);
                T upper_val = std::min(i1.upper, i2.upper);
                result.intervals_.emplace_back(lower_val, upper_val);
            }
        }
        result.normalize();
        return result;
    }

    bool operator==(const IntervalSet &other) const {
        if (is_undefined_ != other.is_undefined_) {
            return false;
        }
        if (is_undefined_) {
            return true; // 两个都是 undefined
        }
        if (intervals_.size() != other.intervals_.size()) {
            return false;
        }
        for (size_t i = 0; i < intervals_.size(); ++i) {
            if (intervals_[i] != other.intervals_[i]) {
                return false;
            }
        }
        return true;
    }

    bool operator!=(const IntervalSet &other) const { return !(*this == other); }

    // 类型转换函数
    // 从 IntervalSet<int> 转换为 IntervalSet<double>
    template<typename U = T>
    std::enable_if_t<std::is_same_v<U, int>, IntervalSet<double>> to_double() const {
        if (is_undefined_) {
            return IntervalSet<double>::make_undefined();
        }
        if (intervals_.empty()) {
            return IntervalSet<double>(); // 空集
        }

        IntervalSet<double> result;
        for (const auto &interval: intervals_) {
            result.intervals_.emplace_back(static_cast<double>(interval.lower), static_cast<double>(interval.upper));
        }
        result.is_undefined_ = false;
        return result;
    }

    // 从 IntervalSet<double> 转换为 IntervalSet<int>
    template<typename U = T>
    std::enable_if_t<std::is_same_v<U, double>, IntervalSet<int>> to_int() const {
        if (is_undefined_) {
            return IntervalSet<int>::make_undefined();
        }
        if (intervals_.empty()) {
            return IntervalSet<int>(); // 空集
        }

        IntervalSet<int> result;
        for (const auto &interval: intervals_) {
            // 对于浮点数转整数，需要向下取整下界，向上取整上界
            int lower = static_cast<int>(std::floor(interval.lower));
            int upper = static_cast<int>(std::ceil(interval.upper));
            result.intervals_.emplace_back(lower, upper);
        }
        result.is_undefined_ = false;
        return result;
    }

    // 显式类型转换操作符
    template<typename U>
    explicit operator IntervalSet<U>() const {
        if constexpr (std::is_same_v<T, int> && std::is_same_v<U, double>) {
            return to_double();
        } else if constexpr (std::is_same_v<T, double> && std::is_same_v<U, int>) {
            return to_int();
        } else {
            static_assert(std::is_same_v<T, U>, "Only conversion between int and double is supported");
        }
        log_fatal("Invalid transform");
    }

    [[nodiscard]] std::string to_string() const {
        if (is_undefined_) {
            return "Undefined (X_N)";
        }
        if (intervals_.empty()) {
            return "Empty (⊥_N)";
        }

        std::stringstream ss;
        ss << "{";
        for (size_t i = 0; i < intervals_.size(); ++i) {
            ss << intervals_[i].to_string() << (i == intervals_.size() - 1 ? "" : ", ");
        }
        ss << "}";
        return ss.str();
    }
};

struct IntervalAnalysis::Summary {
    using ConditionsMap = std::unordered_map<std::shared_ptr<Mir::Value>, AnyIntervalSet>;

    // 前置条件
    ConditionsMap constraints{};
    // 后置条件
    ConditionsMap post_conditions{};

    explicit Summary() = default;

    bool operator==(const Summary &other) const {
        return constraints == other.constraints && post_conditions == other.post_conditions;
    }

    bool operator!=(const Summary &other) const { return !(*this == other); }
};

class IntervalAnalysis::SummaryManager {
public:
    using FunctionSummaryMap = std::unordered_map<std::shared_ptr<Mir::Function>, Summary>;

    void update(const std::shared_ptr<Mir::Function> &func, const Summary &s) { summaries_[func] = s; }

    Summary get(const std::shared_ptr<Mir::Function> &func) const {
        if (const auto it = summaries_.find(func); it != summaries_.end()) {
            return it->second;
        }
        // 如果找不到摘要，返回一个空的默认摘要
        return Summary{};
    }

private:
    FunctionSummaryMap summaries_;
};

class IntervalAnalysis::Context {
public:
    bool contains(const std::shared_ptr<Mir::Value> &value) const { return intervals.find(value) != intervals.end(); }

    bool insert(const std::shared_ptr<Mir::Value> &value, const AnyIntervalSet &interval) {
        if (contains(value)) {
            return false;
        }
        intervals[value] = interval;
        return true;
    }

    bool insert_top(const std::shared_ptr<Mir::Value> &value) {
        if (contains(value)) {
            return false;
        }
        if (value->get_type()->is_float()) {
            intervals[value] = IntervalSet<double>::make_any();
        } else {
            intervals[value] = IntervalSet<int>::make_any();
        }
        return true;
    }

    bool insert_undefined(const std::shared_ptr<Mir::Value> &value) {
        if (contains(value)) {
            return false;
        }
        if (value->get_type()->is_float()) {
            intervals[value] = IntervalSet<double>::make_undefined();
        } else {
            intervals[value] = IntervalSet<int>::make_undefined();
        }
        return true;
    }

    AnyIntervalSet get(const std::shared_ptr<Mir::Value> &value) const {
        if (contains(value)) [[likely]] {
            return intervals.at(value);
        }
        if (value->is_constant()) {
            const auto constant{value->as<Mir::Const>()->get_constant_value()};
            if (constant.holds<int>()) {
                return IntervalSet(constant.get<int>());
            }
            return IntervalSet(constant.get<double>());
        }
        log_error("Does not exist");
    }

private:
    std::unordered_map<std::shared_ptr<Mir::Value>, AnyIntervalSet> intervals{};
};
} // namespace Pass

#endif // INTERVALANALYSIS_H
