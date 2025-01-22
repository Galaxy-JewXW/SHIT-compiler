#ifndef IRTYPE_H
#define IRTYPE_H
#include <memory>
#include <utility>

namespace IRTYPE {

    class IRType {
        public:
            IRType();
    };

    class IRArrayType : public IRType {
    private:
        size_t size{};
        std::shared_ptr<IRType> elementType;
        public:
            IRArrayType(const size_t size, std::shared_ptr<IRType> elementType) : size(size), elementType(std::move(elementType)) {};

            [[nodiscard]] size_t getSize() const;
            [[nodiscard]] std::shared_ptr<IRType> getElementType() const;
    };

    class IRPointerType : public IRType {
        private:
            std::shared_ptr<IRType> containType;
        public:
            explicit IRPointerType(std::shared_ptr<IRType> containType) : containType(std::move(containType)) {};
            [[nodiscard]] std::shared_ptr<IRType> getContainType() const;
    };

    class IRIntegerType : public IRType {
        private:
            IRIntegerType() {}
        public:
            IRIntegerType(const IRIntegerType&) = delete;
            IRIntegerType& operator=(const IRIntegerType&) = delete;

            [[nodiscard]] static std::shared_ptr<IRIntegerType> getIntegerType() {
                static auto integerType = std::make_shared<IRIntegerType>();
                return integerType;
            }

    };
    class IRFloatType : public IRType {
        private:
        IRFloatType() {}
        public:
        IRFloatType(const IRFloatType&) = delete;
        IRFloatType& operator=(const IRFloatType&) = delete;
        [[nodiscard]] static std::shared_ptr<IRFloatType> getFloatType() {
            static auto floatType = std::make_shared<IRFloatType>();
            return floatType;
        }
    };

    class IRVoidType : public IRType {
        private:
        IRVoidType() {}
        public:
        IRVoidType(const IRVoidType&) = delete;
        IRVoidType& operator=(const IRVoidType&) = delete;
        [[nodiscard]] static std::shared_ptr<IRVoidType> getVoidType() {
            static auto voidType = std::make_shared<IRVoidType>();
            return voidType;
        }
    };

}
#endif //IRTYPE_H
