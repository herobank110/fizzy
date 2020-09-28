// Fizzy: A fast WebAssembly interpreter
// Copyright 2020 The Fizzy Authors.
// SPDX-License-Identifier: Apache-2.0

#include "cxx20/bit.hpp"
#include "execute.hpp"
#include "instantiate.hpp"
#include "parser.hpp"
#include <fizzy/fizzy.h>
#include <memory>

namespace
{
inline fizzy::ValType unwrap(FizzyValueType value_type) noexcept
{
    switch (value_type)
    {
    case FizzyValueTypeI32:
        return fizzy::ValType::i32;
    case FizzyValueTypeI64:
        return fizzy::ValType::i64;
    case FizzyValueTypeF32:
        return fizzy::ValType::f32;
    case FizzyValueTypeF64:
        return fizzy::ValType::f64;
    }
    __builtin_unreachable();
}

inline fizzy::FuncType unwrap(const FizzyFunctionType& type)
{
    fizzy::FuncType func_type{std::vector<fizzy::ValType>(type.inputs_size),
        std::vector<fizzy::ValType>(type.outputs_size)};

    fizzy::ValType (*unwrap_valtype_fn)(FizzyValueType value) = &unwrap;
    std::transform(
        type.inputs, type.inputs + type.inputs_size, func_type.inputs.begin(), unwrap_valtype_fn);
    std::transform(type.outputs, type.outputs + type.outputs_size, func_type.outputs.begin(),
        unwrap_valtype_fn);

    return func_type;
}

inline FizzyValue wrap(fizzy::Value value) noexcept
{
    return fizzy::bit_cast<FizzyValue>(value);
}

inline fizzy::Value unwrap(FizzyValue value) noexcept
{
    return fizzy::bit_cast<fizzy::Value>(value);
}

inline const FizzyValue* wrap(const fizzy::Value* values) noexcept
{
    return reinterpret_cast<const FizzyValue*>(values);
}

inline const fizzy::Value* unwrap(const FizzyValue* values) noexcept
{
    return reinterpret_cast<const fizzy::Value*>(values);
}

inline FizzyInstance* wrap(fizzy::Instance* instance) noexcept
{
    return reinterpret_cast<FizzyInstance*>(instance);
}

inline fizzy::Instance* unwrap(FizzyInstance* instance) noexcept
{
    return reinterpret_cast<fizzy::Instance*>(instance);
}

inline FizzyExecutionResult wrap(const fizzy::ExecutionResult& result) noexcept
{
    return {result.trapped, result.has_value, wrap(result.value)};
}

inline fizzy::ExecutionResult unwrap(FizzyExecutionResult result) noexcept
{
    if (result.trapped)
        return fizzy::Trap;
    else if (!result.has_value)
        return fizzy::Void;
    else
        return unwrap(result.value);
}

inline auto unwrap(FizzyExternalFn func, void* context) noexcept
{
    return [func, context](fizzy::Instance& instance, fizzy::span<const fizzy::Value> args,
               int depth) noexcept -> fizzy::ExecutionResult {
        const auto result = func(context, wrap(&instance), wrap(args.data()), args.size(), depth);
        return unwrap(result);
    };
}

inline fizzy::ExternalFunction unwrap(const FizzyExternalFunction& external_func)
{
    return fizzy::ExternalFunction{
        unwrap(external_func.function, external_func.context), unwrap(external_func.type)};
}
}  // namespace

extern "C" {
struct FizzyModule
{
    fizzy::Module module;
};

struct FizzyInstance
{
    fizzy::Instance* instance;
};

bool fizzy_validate(const uint8_t* wasm_binary, size_t wasm_binary_size)
{
    try
    {
        fizzy::parse({wasm_binary, wasm_binary_size});
        return true;
    }
    catch (...)
    {
        return false;
    }
}

FizzyModule* fizzy_parse(const uint8_t* wasm_binary, size_t wasm_binary_size)
{
    try
    {
        auto cmodule = std::make_unique<FizzyModule>();
        cmodule->module = fizzy::parse({wasm_binary, wasm_binary_size});
        return cmodule.release();
    }
    catch (...)
    {
        return nullptr;
    }
}

void fizzy_free_module(FizzyModule* module)
{
    delete module;
}

FizzyInstance* fizzy_instantiate(FizzyModule* module,
    const FizzyExternalFunction* imported_functions, size_t imported_functions_size)
{
    try
    {
        std::vector<fizzy::ExternalFunction> functions(imported_functions_size);
        fizzy::ExternalFunction (*unwrap_external_func_fn)(const FizzyExternalFunction&) = &unwrap;
        std::transform(imported_functions, imported_functions + imported_functions_size,
            functions.begin(), unwrap_external_func_fn);

        auto instance = fizzy::instantiate(std::move(module->module), std::move(functions));

        fizzy_free_module(module);
        return wrap(instance.release());
    }
    catch (...)
    {
        fizzy_free_module(module);
        return nullptr;
    }
}

void fizzy_free_instance(FizzyInstance* instance)
{
    delete unwrap(instance);
}

FizzyExecutionResult fizzy_execute(
    FizzyInstance* instance, uint32_t func_idx, const FizzyValue* args, size_t args_size, int depth)
{
    const auto result =
        fizzy::execute(*unwrap(instance), func_idx, {unwrap(args), args_size}, depth);
    return wrap(result);
}
}
