include "mlir/IR/OpBase.td"
include "infrt/dialect/cinn_base.td"


class CINN_Op<string mnemonic, list<OpTrait> traits = []> :
    Op<CINN_Dialect, mnemonic, traits>;
