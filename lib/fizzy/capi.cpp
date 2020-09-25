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
inline FizzyInstance* wrap(fizzy::Instance* instance) noexcept
{
    return reinterpret_cast<FizzyInstance*>(instance);
}

inline fizzy::Instance* unwrap(FizzyInstance* instance) noexcept
{
    return reinterpret_cast<fizzy::Instance*>(instance);
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
        // Convert fizzy_external_function to fizzy::ExternalFunction
        std::vector<fizzy::ExternalFunction> functions(imported_functions_size);
        for (size_t imported_func_idx = 0; imported_func_idx < imported_functions_size;
             ++imported_func_idx)
        {
            auto func = [cfunc = imported_functions[imported_func_idx]](fizzy::Instance& instance,
                            fizzy::span<const fizzy::Value> args,
                            int depth) -> fizzy::ExecutionResult {
                const auto cres = cfunc.function(cfunc.context, wrap(&instance),
                    reinterpret_cast<const FizzyValue*>(args.data()),
                    static_cast<uint32_t>(args.size()), depth);

                if (cres.trapped)
                    return fizzy::Trap;
                else if (!cres.has_value)
                    return fizzy::Void;
                else
                    return fizzy::bit_cast<fizzy::Value>(cres.value);
            };

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

FizzyExecutionResult fizzy_execute(FizzyInstance* instance, uint32_t func_idx,
    const FizzyValue* cargs, size_t args_size, int depth)
{
    const auto args = reinterpret_cast<const fizzy::Value*>(cargs);

    const auto result = fizzy::execute(
        *unwrap(instance), func_idx, fizzy::span<const fizzy::Value>(args, args_size), depth);

    return {result.trapped, result.has_value, fizzy::bit_cast<FizzyValue>(result.value)};
}
}
