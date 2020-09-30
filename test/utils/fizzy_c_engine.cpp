// Fizzy: A fast WebAssembly interpreter
// Copyright 2019-2020 The Fizzy Authors.
// SPDX-License-Identifier: Apache-2.0

#include <fizzy/fizzy.h>

#include <test/utils/adler32.hpp>
#include <test/utils/wasm_engine.hpp>
#include <cassert>
#include <cstring>

namespace fizzy::test
{
class FizzyCEngine final : public WasmEngine
{
    FizzyInstance* m_instance = nullptr;

public:
    ~FizzyCEngine();

    bool parse(bytes_view input) const final;
    std::optional<FuncRef> find_function(
        std::string_view name, std::string_view signature) const final;
    bool instantiate(bytes_view wasm_binary) final;
    bool init_memory(bytes_view memory) final;
    bytes_view get_memory() const final;
    Result execute(FuncRef func_ref, const std::vector<uint64_t>& args) final;
};

namespace
{
FizzyExecutionResult env_adler32(
    void*, FizzyInstance* instance, const FizzyValue* args, size_t, int)
{
    auto* memory = fizzy_get_instance_memory_data(instance);
    assert(memory != nullptr);
    const auto size = fizzy_get_instance_memory_size(instance);
    const auto ret = fizzy::test::adler32(
        bytes_view(memory, size)
            .substr(static_cast<uint32_t>(args[0].i64), static_cast<uint32_t>(args[1].i64)));
    return {false, true, {ret}};
}
}  // namespace

std::unique_ptr<WasmEngine> create_fizzy_c_engine()
{
    return std::make_unique<FizzyCEngine>();
}

FizzyCEngine::~FizzyCEngine()
{
    if (m_instance)
        fizzy_free_instance(m_instance);
}

bool FizzyCEngine::parse(bytes_view input) const
{
    const auto module = fizzy_parse(input.data(), input.size());
    if (!module)
        return false;

    fizzy_free_module(module);
    return true;
}

bool FizzyCEngine::instantiate(bytes_view wasm_binary)
{
    const auto module = fizzy_parse(wasm_binary.data(), wasm_binary.size());
    if (!module)
        return false;

    // TODO resolve_imported_functions
    FizzyExternalFunction imports[] = {{env_adler32, nullptr}};
    m_instance = fizzy_instantiate(module, imports, 1);

    assert(m_instance != nullptr);
    return true;
}

bool FizzyCEngine::init_memory(bytes_view memory)
{
    auto* instance_memory = fizzy_get_instance_memory_data(m_instance);
    if (instance_memory == nullptr)
        return false;

    const auto size = fizzy_get_instance_memory_size(m_instance);
    if (size < memory.size())
        return false;

    std::memcpy(instance_memory, memory.data(), memory.size());
    return true;
}

bytes_view FizzyCEngine::get_memory() const
{
    auto* data = fizzy_get_instance_memory_data(m_instance);
    if (data == nullptr)
        return {};

    const auto size = fizzy_get_instance_memory_size(m_instance);
    return {data, size};
}

std::optional<WasmEngine::FuncRef> FizzyCEngine::find_function(
    std::string_view /*name*/, std::string_view /*signature*/) const
{
    //    const auto func_idx = fizzy::find_exported_function(m_instance->module, name);
    //    if (func_idx.has_value())
    //    {
    //        const auto func_type = m_instance->module.get_function_type(*func_idx);
    //        const auto sig_type = translate_signature(signature);
    //        if (sig_type != func_type)
    //            return std::nullopt;
    //    }
    //    return func_idx;
    return std::nullopt;
}

WasmEngine::Result FizzyCEngine::execute(
    WasmEngine::FuncRef func_ref, const std::vector<uint64_t>& args)
{
    static_assert(sizeof(uint64_t) == sizeof(FizzyValue));
    const auto first_arg = reinterpret_cast<const FizzyValue*>(args.data());
    const auto status =
        fizzy_execute(m_instance, static_cast<uint32_t>(func_ref), first_arg, args.size(), 0);
    if (status.trapped)
        return {true, std::nullopt};
    else if (status.has_value)
        return {false, status.value.i64};
    else
        return {false, std::nullopt};
}
}  // namespace fizzy::test
