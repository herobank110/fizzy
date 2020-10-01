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

bool fizzy_find_exported_function(
    const struct FizzyModule* module, const char* name, uint32_t* out_func_idx)
{
    const auto optional_func_idx = fizzy::find_exported_function(module->module, name);
    if (!optional_func_idx)
        return false;

    *out_func_idx = *optional_func_idx;
    return true;
}

FizzyInstance* fizzy_instantiate(FizzyModule* module,
    const FizzyExternalFunction* imported_functions, size_t imported_functions_size)
{
    try
    {
        // Convert fizzy_external_function to fizzy::ExternalFunction
        std::vector<fizzy::ExternalFunction> functions(imported_functions_size);
        for (size_t imported_func_idx = 0; imported_func_idx < imported_functions_size;
             ++imported_func_idx)
        {
            const auto& cfunc = imported_functions[imported_func_idx];

            auto func = unwrap(cfunc.function, cfunc.context);
            // TODO get type from input array
            auto func_type = module->module.imported_function_types[imported_func_idx];

            functions[imported_func_idx] =
                fizzy::ExternalFunction{std::move(func), std::move(func_type)};
        }

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

uint8_t* fizzy_get_instance_memory_data(struct FizzyInstance* instance)
{
    auto& memory = unwrap(instance)->memory;
    if (!memory)
        return nullptr;

    return memory->data();
}

size_t fizzy_get_instance_memory_size(struct FizzyInstance* instance)
{
    auto& memory = unwrap(instance)->memory;
    if (!memory)
        return 0;

    return memory->size();
}

FizzyExecutionResult fizzy_execute(
    FizzyInstance* instance, uint32_t func_idx, const FizzyValue* args, size_t args_size, int depth)
{
    const auto result =
        fizzy::execute(*unwrap(instance), func_idx, {unwrap(args), args_size}, depth);
    return wrap(result);
}
}
