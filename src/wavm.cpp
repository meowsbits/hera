/*
 * Copyright 2018 Paul Dworzanski
 * Copyright 2016-2018 Alex Beregszaszi et al.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <iostream>
#include <memory>
#include <stack>

#include "wavm.h"

#define DLL_IMPORT // Needed by wavm on some platforms
#include "Inline/Serialization.h"
#include "IR/Module.h"
#include "IR/Validate.h"
#include "Runtime/Intrinsics.h"
#include "Runtime/Linker.h"
#include "Runtime/Runtime.h"
#include "WASM/WASM.h"

#include "debugging.h"
#include "eei.h"
#include "exceptions.h"

#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wunused-variable"

using namespace std;

namespace hera {

class WavmEthereumInterface : public EthereumInterface {
public:
  explicit WavmEthereumInterface(
    evmc_context* _context,
    vector<uint8_t> const& _code,
    evmc_message const& _msg,
    ExecutionResult & _result,
    bool _meterGas
  ):
    EthereumInterface(_context, _code, _msg, _result, _meterGas)
  {}

  void setWasmMemory(Runtime::MemoryInstance* _wasmMemory) {
    m_wasmMemory = _wasmMemory;
  }

private:
  // These assume that m_wasmMemory was set prior to execution.
  size_t memorySize() const override { return Runtime::getMemoryNumPages(m_wasmMemory) * 65536; }
  void memorySet(size_t offset, uint8_t value) override { (Runtime::memoryArrayPtr<U8>(m_wasmMemory, offset, 1))[0] = value; }
  uint8_t memoryGet(size_t offset) override { return (Runtime::memoryArrayPtr<U8>(m_wasmMemory, offset, 1))[0]; }

  Runtime::MemoryInstance* m_wasmMemory;
};

unique_ptr<WasmEngine> WavmEngine::create()
{
  return unique_ptr<WasmEngine>{new WavmEngine};
}

namespace wavm_host_module {
  // first the ethereum interface(s), the top of the stack is used in host functions
  stack<WavmEthereumInterface*> interface;

  // the host module is called 'ethereum'
  DEFINE_INTRINSIC_MODULE(ethereum)

  // host functions follow
  DEFINE_INTRINSIC_FUNCTION(ethereum, "useGas", void, useGas, I64 amount)
  {
    interface.top()->eeiUseGas(amount);
  }

  DEFINE_INTRINSIC_FUNCTION(ethereum, "getGasLeft", I64, getGasLeft)
  {
    return interface.top()->eeiGetGasLeft();
  }

  DEFINE_INTRINSIC_FUNCTION(ethereum, "getAddress", void, getAddress, U32 resultOffset)
  {
    interface.top()->eeiGetAddress(resultOffset);
  }

  DEFINE_INTRINSIC_FUNCTION(ethereum, "getExternalBalance", void, getExternalBalance, U32 addressOffset, U32 resultOffset)
  {
    interface.top()->eeiGetExternalBalance(addressOffset, resultOffset);
  }

  DEFINE_INTRINSIC_FUNCTION(ethereum, "getBlockHash", U32, getBlockHash, U32 number, U32 resultOffset)
  {
    return interface.top()->eeiGetBlockHash(number, resultOffset);
  }

  DEFINE_INTRINSIC_FUNCTION(ethereum, "getCallDataSize", U32, getCallDataSize)
  {
    return interface.top()->eeiGetCallDataSize();
  }

  DEFINE_INTRINSIC_FUNCTION(ethereum, "callDataCopy", void, callDataCopy, U32 resultOffset, U32 dataOffset, U32 length)
  {
    interface.top()->eeiCallDataCopy(resultOffset, dataOffset, length);
  }

  DEFINE_INTRINSIC_FUNCTION(ethereum, "getCaller", void, getCaller, U32 resultOffset)
  {
    interface.top()->eeiGetCaller(resultOffset);
  }

  DEFINE_INTRINSIC_FUNCTION(ethereum, "getCallValue", void, getCallValue, U32 resultOffset)
  {
    interface.top()->eeiGetCallValue(resultOffset);
  }

  DEFINE_INTRINSIC_FUNCTION(ethereum, "getCodeSize", U32, getCodeSize)
  {
    return interface.top()->eeiGetCodeSize();
  }

  DEFINE_INTRINSIC_FUNCTION(ethereum, "codeCopy", void, codeCopy, U32 resultOffset, U32 codeOffset, U32 length)
  {
    interface.top()->eeiCodeCopy(resultOffset, codeOffset, length);
  }

  DEFINE_INTRINSIC_FUNCTION(ethereum, "getExternalCodeSize", U32, getExternalCodeSize, U32 addressOffset)
  {
    return interface.top()->eeiGetExternalCodeSize(addressOffset);
  }

  DEFINE_INTRINSIC_FUNCTION(ethereum, "externalCodeCopy", void, externalCodeCopy, U32 addressOffset, U32 resultOffset, U32 codeOffset, U32 length)
  {
    interface.top()->eeiExternalCodeCopy(addressOffset, resultOffset, codeOffset, length);
  }

  DEFINE_INTRINSIC_FUNCTION(ethereum, "getBlockCoinbase", void, getBlockCoinbase, U32 resultOffset)
  {
    interface.top()->eeiGetBlockCoinbase(resultOffset);
  }

  DEFINE_INTRINSIC_FUNCTION(ethereum, "getBlockDifficulty", void, getBlockDifficulty, U32 resultOffset)
  {
    interface.top()->eeiGetBlockDifficulty(resultOffset);
  }

  DEFINE_INTRINSIC_FUNCTION(ethereum, "getBlockGasLimit", I64, getBlockGasLimit)
  {
    return interface.top()->eeiGetBlockGasLimit();
  }

  DEFINE_INTRINSIC_FUNCTION(ethereum, "getTxGasPrice", void, getTxGasPrice, U32 resultOffset)
  {
    interface.top()->eeiGetTxGasPrice(resultOffset);
  }

  DEFINE_INTRINSIC_FUNCTION(ethereum, "log", void, log, U32 dataOffset, U32 length, U32 numberOfTopics, U32 topic1, U32 topic2, U32 topic3, U32 topic4)
  {
    interface.top()->eeiLog(dataOffset, length, numberOfTopics, topic1, topic2, topic3, topic4);
  }

  DEFINE_INTRINSIC_FUNCTION(ethereum, "getBlockNumber", I64, getBlockNumber)
  {
    return interface.top()->eeiGetBlockNumber();
  }

  DEFINE_INTRINSIC_FUNCTION(ethereum, "getBlockTimestamp", I64, getBlockTimestamp)
  {
    return interface.top()->eeiGetBlockTimestamp();
  }

  DEFINE_INTRINSIC_FUNCTION(ethereum, "getTxOrigin", void, getTxOrigin, U32 resultOffset)
  {
    interface.top()->eeiGetTxOrigin(resultOffset);
  }

  DEFINE_INTRINSIC_FUNCTION(ethereum, "storageStore", void, storageStore, U32 pathOffset, U32 valueOffset)
  {
    interface.top()->eeiStorageStore(pathOffset, valueOffset);
  }

  DEFINE_INTRINSIC_FUNCTION(ethereum, "storageLoad", void, storageLoad, U32 pathOffset, U32 valueOffset)
  {
    interface.top()->eeiStorageLoad(pathOffset, valueOffset);
  }

  DEFINE_INTRINSIC_FUNCTION(ethereum, "finish", void, finish, U32 dataOffset, U32 length)
  {
    interface.top()->eeiFinish(dataOffset, length);
  }

  DEFINE_INTRINSIC_FUNCTION(ethereum, "revert", void, revert, U32 dataOffset, U32 length)
  {
    interface.top()->eeiRevert(dataOffset, length);
  }

  DEFINE_INTRINSIC_FUNCTION(ethereum, "getReturnDataSize", U32, getReturnDataSize)
  {
    return interface.top()->eeiGetReturnDataSize();
  }

  DEFINE_INTRINSIC_FUNCTION(ethereum, "returnDataCopy", void, returnDataCopy, U32 resultOffset, U32 dataOffset, U32 length)
  {
    interface.top()->eeiReturnDataCopy(resultOffset, dataOffset, length);
  }

  DEFINE_INTRINSIC_FUNCTION(ethereum, "call", U32, call, I64 gas, U32 addressOffset, U32 valueOffset, U32 dataOffset, U32 dataLength)
  {
    return interface.top()->eeiCall(EthereumInterface::EEICallKind::Call, gas, addressOffset, valueOffset, dataOffset, dataLength);
  }

  DEFINE_INTRINSIC_FUNCTION(ethereum, "callCode", U32, callCode, I64 gas, U32 addressOffset, U32 valueOffset, U32 dataOffset, U32 dataLength)
  {
    return interface.top()->eeiCall(EthereumInterface::EEICallKind::CallCode, gas, addressOffset, valueOffset, dataOffset, dataLength);
  }

  DEFINE_INTRINSIC_FUNCTION(ethereum, "callDelegate", U32, callDelegate, I64 gas, U32 addressOffset, U32 dataOffset, U32 dataLength)
  {
    return interface.top()->eeiCall(EthereumInterface::EEICallKind::CallDelegate, gas, addressOffset, 0, dataOffset, dataLength);
  }

  DEFINE_INTRINSIC_FUNCTION(ethereum, "callStatic", U32, callStatic, I64 gas, U32 addressOffset, U32 dataOffset, U32 dataLength)
  {
    return interface.top()->eeiCall(EthereumInterface::EEICallKind::CallStatic, gas, addressOffset, 0, dataOffset, dataLength);
  }

  DEFINE_INTRINSIC_FUNCTION(ethereum, "create", U32, create, U32 valueOffset, U32 dataOffset, U32 dataLength, U32 resultOffset)
  {
    return interface.top()->eeiCreate(valueOffset, dataOffset, dataLength, resultOffset);
  }

  DEFINE_INTRINSIC_FUNCTION(ethereum, "selfDestruct", void, selfDestruct, U32 addressOffset)
  {
    interface.top()->eeiSelfDestruct(addressOffset);
  }

  // this is needed for resolving names of imported host functions
  struct HeraWavmResolver : Runtime::Resolver {
    HashMap<string, Runtime::ModuleInstance*> moduleNameToInstanceMap;

    bool resolve(
      const string& moduleName,
      const string& exportName,
      IR::ObjectType type,
      Runtime::Object*& outObject
    ) override
    {
      outObject = nullptr;
      auto namedInstance = moduleNameToInstanceMap.get(moduleName);
      if (namedInstance)
        outObject = Runtime::getInstanceExport(*namedInstance, exportName);
      if (outObject && !isA(outObject, type))
        outObject = nullptr;
      HERA_DEBUG << "Resolving " << moduleName << " : " << exportName << " -> " << (outObject != nullptr) << "\n";
      return outObject != nullptr;
    }
  };
} // namespace wavm_host_module

struct WavmInterfaceKeeper {
  explicit WavmInterfaceKeeper(WavmEthereumInterface& interface)
  {
    wavm_host_module::interface.push(&interface);
  }

  ~WavmInterfaceKeeper() noexcept { wavm_host_module::interface.pop(); }
};

ExecutionResult WavmEngine::execute(
  evmc_context* context,
  vector<uint8_t> const& code,
  vector<uint8_t> const& state_code,
  evmc_message const& msg,
  bool meterInterfaceGas
) {
  try {
    ExecutionResult result = internalExecute(context, code, state_code, msg, meterInterfaceGas);
    // And clean up mess left by this run.
    Runtime::collectGarbage();
    return result;
  } catch (exception const&) {
    // And clean up mess left by this run.
    Runtime::collectGarbage();
    // We only catch this exception here in order to clean up garbage..
    // TODO: hopefully WAVM is fixed so that this isn't needed
    throw;
  }
}

ExecutionResult WavmEngine::internalExecute(
  evmc_context* context,
  vector<uint8_t> const& code,
  vector<uint8_t> const& state_code,
  evmc_message const& msg,
  bool meterInterfaceGas
) {
  HERA_DEBUG << "Executing with wavm...\n";

  // set up a new ethereum interface just for this contract invocation
  ExecutionResult result;
  WavmEthereumInterface interface{context, state_code, msg, result, meterInterfaceGas};
  WavmInterfaceKeeper interfaceKeeper{interface};

  // first parse module
  IR::Module moduleIR;
  try {
    // NOTE: this expects U8, which is a typedef over uint8_t
    Serialization::MemoryInputStream input(code.data(), code.size());
    WASM::serialize(input, moduleIR);
  } catch (Serialization::FatalSerializationException const& e) {
    ensureCondition(false, ContractValidationFailure, "Failed to deserialise contract: " + e.message);
  } catch (IR::ValidationException const& e) {
    ensureCondition(false, ContractValidationFailure, "Failed to validate contract: " + e.message);
  } catch (std::bad_alloc const&) {
    // Catching this here because apparently wavm doesn't necessarily checks bounds before allocation
    ensureCondition(false, ContractValidationFailure, "Bug in wavm: didn't check bounds before allocation");
  }

  // next set up the VM
  // Note: in ewasm, we create a new VM for each call to a module, so we must instantiate a new host module for each of these VMs, this is inefficient, but OK for prototyping.

  // compartment is like the Wasm store, represents the VM, has lists of globals, memories, tables, and also has wavm's runtime stuff
  Runtime::GCPointer<Runtime::Compartment> compartment = Runtime::createCompartment();
  // context stores the compartment and some other stuff
  Runtime::GCPointer<Runtime::Context> wavm_context = Runtime::createContext(compartment);

  // instantiate host Module
  Runtime::GCPointer<Runtime::ModuleInstance> ethereumHostModule = Intrinsics::instantiateModule(compartment, wavm_host_module::INTRINSIC_MODULE_REF(ethereum), "ethereum", {});
  heraAssert(ethereumHostModule, "Failed to create host module.");

  // prepare contract module to resolve links against host module
  wavm_host_module::HeraWavmResolver resolver;
  // TODO: move this into the constructor?
  resolver.moduleNameToInstanceMap.set("ethereum", ethereumHostModule);
  Runtime::LinkResult linkResult = Runtime::linkModule(moduleIR, resolver);
  heraAssert(linkResult.success, "Couldn't link contract against host module.");

  // compile the module from IR to LLVM bitcode
  Runtime::GCPointer<Runtime::Module> module = Runtime::compileModule(moduleIR);
  heraAssert(module, "Couldn't compile IR to bitcode.");

  // instantiate contract module
  Runtime::GCPointer<Runtime::ModuleInstance> moduleInstance = Runtime::instantiateModule(compartment, module, move(linkResult.resolvedImports), "<ewasmcontract>");
  heraAssert(moduleInstance, "Couldn't instantiate contact module.");

  // get memory for easy access in host functions
  wavm_host_module::interface.top()->setWasmMemory(asMemory(Runtime::getInstanceExport(moduleInstance, "memory")));

  // invoke the main function
  Runtime::GCPointer<Runtime::FunctionInstance> mainFunction = asFunctionNullable(Runtime::getInstanceExport(moduleInstance, "main"));
  ensureCondition(mainFunction, ContractValidationFailure, "\"main\" not found");

  // this is how WAVM's try/catch for exceptions
  Runtime::catchRuntimeExceptions(
    [&] {
      try {
        vector<IR::Value> invokeArgs;
        Runtime::invokeFunctionChecked(wavm_context, mainFunction, invokeArgs);
      } catch (EndExecution const&) {
        // This exception is ignored here because we consider it to be a success.
        // It is only a clutch for POSIX style exit()
      }
    },
    [&](Runtime::Exception&& exception) {
      // FIXME: decide if each of the exception fit into VMTrap/InternalError
      ensureCondition(false, VMTrap, Runtime::describeException(exception));
    }
  );

  return result;
}

} // namespace hera
