//===- NVVMDialect.cpp - NVVM IR Ops and Dialect registration -------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file defines the types and operation details for the NVVM IR dialect in
// MLIR, and the LLVM IR dialect.  It also registers the dialect.
//
// The NVVM dialect only contains GPU specific additions on top of the general
// LLVM dialect.
//
//===----------------------------------------------------------------------===//

#include "mlir/Dialect/LLVMIR/NVVMDialect.h"

#include "mlir/Conversion/ConvertToLLVM/ToLLVMInterface.h"
#include "mlir/Dialect/GPU/IR/GPUDialect.h"
#include "mlir/Dialect/Utils/StaticValueUtils.h"
#include "mlir/IR/Builders.h"
#include "mlir/IR/BuiltinAttributes.h"
#include "mlir/IR/BuiltinTypes.h"
#include "mlir/IR/DialectImplementation.h"
#include "mlir/IR/MLIRContext.h"
#include "mlir/IR/Operation.h"
#include "mlir/IR/OperationSupport.h"
#include "mlir/Support/LogicalResult.h"
#include "llvm/ADT/TypeSwitch.h"
#include "llvm/AsmParser/Parser.h"
#include "llvm/IR/Attributes.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Type.h"
#include "llvm/Support/Casting.h"
#include "llvm/Support/SourceMgr.h"
#include <optional>
#include <string>

using namespace mlir;
using namespace NVVM;

#include "mlir/Dialect/LLVMIR/NVVMOpsDialect.cpp.inc"
#include "mlir/Dialect/LLVMIR/NVVMOpsEnums.cpp.inc"

//===----------------------------------------------------------------------===//
// Printing/parsing for NVVM ops
//===----------------------------------------------------------------------===//

static void printNVVMIntrinsicOp(OpAsmPrinter &p, Operation *op) {
  p << " " << op->getOperands();
  if (op->getNumResults() > 0)
    p << " : " << op->getResultTypes();
}

// <operation> ::= `llvm.nvvm.vote.ballot.sync %mask, %pred` : result_type
ParseResult VoteBallotOp::parse(OpAsmParser &parser, OperationState &result) {
  MLIRContext *context = parser.getContext();
  auto int32Ty = IntegerType::get(context, 32);
  auto int1Ty = IntegerType::get(context, 1);

  SmallVector<OpAsmParser::UnresolvedOperand, 8> ops;
  Type type;
  return failure(parser.parseOperandList(ops) ||
                 parser.parseOptionalAttrDict(result.attributes) ||
                 parser.parseColonType(type) ||
                 parser.addTypeToList(type, result.types) ||
                 parser.resolveOperands(ops, {int32Ty, int1Ty},
                                        parser.getNameLoc(), result.operands));
}

void VoteBallotOp::print(OpAsmPrinter &p) { printNVVMIntrinsicOp(p, *this); }

LogicalResult CpAsyncBulkTensorGlobalToSharedClusterOp::verify() {
  if (getCoordinates().size() > 5)
    return emitError("Maximum 5 coordinates and dimension is supported.");
  return success();
}

LogicalResult CpAsyncOp::verify() {
  if (getModifier() != LoadCacheModifierKind::CG &&
      getModifier() != LoadCacheModifierKind::CA)
    return emitError("Only CG and CA cache modifiers are supported.");
  if (getSize() != 4 && getSize() != 8 && getSize() != 16)
    return emitError("expected byte size to be either 4, 8 or 16.");
  if (getModifier() == LoadCacheModifierKind::CG && getSize() != 16)
    return emitError("CG cache modifier is only support for 16 bytes copy.");
  return success();
}

// Given the element type of an operand and whether or not it is an accumulator,
// this function returns the PTX type (`NVVM::MMATypes`) that corresponds to the
// operand's element type.
std::optional<mlir::NVVM::MMATypes>
MmaOp::inferOperandMMAType(Type operandElType, bool isAccumulator) {
  auto half2Type =
      LLVM::getFixedVectorType(Float16Type::get(operandElType.getContext()), 2);
  if (operandElType.isF64())
    return NVVM::MMATypes::f64;
  if (operandElType.isF16() || operandElType == half2Type)
    return NVVM::MMATypes::f16;
  if (operandElType.isF32() && isAccumulator)
    return NVVM::MMATypes::f32;
  if (operandElType.isF32() && !isAccumulator)
    return NVVM::MMATypes::tf32;
  if (llvm::isa<IntegerType>(operandElType)) {
    if (isAccumulator)
      return NVVM::MMATypes::s32;
    return std::nullopt;
  }

  if (auto structType = llvm::dyn_cast<LLVM::LLVMStructType>(operandElType)) {
    if (structType.getBody().empty())
      return std::nullopt;
    return inferOperandMMAType(structType.getBody()[0], isAccumulator);
  }

  return std::nullopt;
}

static bool isInt4PtxType(MMATypes type) {
  return (type == MMATypes::u4 || type == MMATypes::s4);
}

static bool isInt8PtxType(MMATypes type) {
  return (type == MMATypes::u8 || type == MMATypes::s8);
}

static bool isIntegerPtxType(MMATypes type) {
  return isInt4PtxType(type) || isInt8PtxType(type) || type == MMATypes::b1 ||
         type == MMATypes::s32;
}

MMATypes MmaOp::accumPtxType() {
  std::optional<mlir::NVVM::MMATypes> val = inferOperandMMAType(
      getODSOperands(2).getTypes().front(), /*isAccum=*/true);
  assert(val.has_value() && "accumulator PTX type should always be inferrable");
  return val.value();
}

MMATypes MmaOp::resultPtxType() {
  std::optional<mlir::NVVM::MMATypes> val =
      inferOperandMMAType(getResult().getType(), /*isAccum=*/true);
  assert(val.has_value() && "result PTX type should always be inferrable");
  return val.value();
}

void MmaOp::print(OpAsmPrinter &p) {
  SmallVector<Type, 4> regTypes;
  struct OperandFragment {
    StringRef operandName;
    StringRef ptxTypeAttr;
    SmallVector<Value, 4> regs;
    explicit OperandFragment(StringRef name, StringRef ptxTypeName)
        : operandName(name), ptxTypeAttr(ptxTypeName) {}
  };

  std::array<OperandFragment, 3> frags{
      OperandFragment("A", getMultiplicandAPtxTypeAttrName()),
      OperandFragment("B", getMultiplicandBPtxTypeAttrName()),
      OperandFragment("C", "")};
  SmallVector<StringRef, 4> ignoreAttrNames{
      mlir::NVVM::MmaOp::getOperandSegmentSizeAttr()};

  for (unsigned fragIdx = 0; fragIdx < frags.size(); fragIdx++) {
    auto &frag = frags[fragIdx];
    auto varOperandSpec = getODSOperandIndexAndLength(fragIdx);
    for (auto operandIdx = varOperandSpec.first;
         operandIdx < varOperandSpec.first + varOperandSpec.second;
         operandIdx++) {
      frag.regs.push_back(this->getOperand(operandIdx));
      if (operandIdx == 0) {
        regTypes.push_back(this->getOperand(operandIdx).getType());
      }
    }
    std::optional<MMATypes> inferredType =
        inferOperandMMAType(regTypes.back(), /*isAccum=*/fragIdx >= 2);
    if (inferredType)
      ignoreAttrNames.push_back(frag.ptxTypeAttr);
  }

  auto printMmaOperand = [&](const OperandFragment &frag) -> void {
    p << " " << frag.operandName;
    p << "[";
    p.printOperands(frag.regs);
    p << "] ";
  };

  for (const auto &frag : frags) {
    printMmaOperand(frag);
  }

  p.printOptionalAttrDict(this->getOperation()->getAttrs(), ignoreAttrNames);

  // Print the types of the operands and result.
  p << " : "
    << "(";
  llvm::interleaveComma(SmallVector<Type, 3>{frags[0].regs[0].getType(),
                                             frags[1].regs[0].getType(),
                                             frags[2].regs[0].getType()},
                        p);
  p << ")";
  p.printArrowTypeList(TypeRange{this->getRes().getType()});
}

void MmaOp::build(OpBuilder &builder, OperationState &result, Type resultType,
                  ValueRange operandA, ValueRange operandB, ValueRange operandC,
                  ArrayRef<int64_t> shape, std::optional<MMAB1Op> b1Op,
                  std::optional<MMAIntOverflow> intOverflow,
                  std::optional<std::array<MMATypes, 2>> multiplicandPtxTypes,
                  std::optional<std::array<MMALayout, 2>> multiplicandLayouts) {

  assert(shape.size() == 3 && "expected shape to have size 3 (m, n, k)");
  MLIRContext *ctx = builder.getContext();
  result.addAttribute(
      "shape", builder.getAttr<MMAShapeAttr>(shape[0], shape[1], shape[2]));

  result.addOperands(operandA);
  result.addOperands(operandB);
  result.addOperands(operandC);

  if (multiplicandPtxTypes) {
    result.addAttribute("multiplicandAPtxType",
                        MMATypesAttr::get(ctx, (*multiplicandPtxTypes)[0]));
    result.addAttribute("multiplicandBPtxType",
                        MMATypesAttr::get(ctx, (*multiplicandPtxTypes)[1]));
  } else {
    if (auto res = inferOperandMMAType(operandA[0].getType(), false))
      result.addAttribute("multiplicandAPtxType", MMATypesAttr::get(ctx, *res));
    if (auto res = inferOperandMMAType(operandB[0].getType(), false))
      result.addAttribute("multiplicandBPtxType", MMATypesAttr::get(ctx, *res));
  }

  if (multiplicandLayouts) {
    result.addAttribute("layoutA",
                        MMALayoutAttr::get(ctx, (*multiplicandLayouts)[0]));
    result.addAttribute("layoutB",
                        MMALayoutAttr::get(ctx, (*multiplicandLayouts)[1]));
  } else {
    result.addAttribute("layoutA", MMALayoutAttr::get(ctx, MMALayout::row));
    result.addAttribute("layoutB", MMALayoutAttr::get(ctx, MMALayout::col));
  }

  if (intOverflow.has_value())
    result.addAttribute("intOverflowBehavior",
                        MMAIntOverflowAttr::get(ctx, *intOverflow));
  if (b1Op.has_value())
    result.addAttribute("b1Op", MMAB1OpAttr::get(ctx, *b1Op));

  result.addTypes(resultType);
  result.addAttribute(
      MmaOp::getOperandSegmentSizeAttr(),
      builder.getDenseI32ArrayAttr({static_cast<int32_t>(operandA.size()),
                                    static_cast<int32_t>(operandB.size()),
                                    static_cast<int32_t>(operandC.size())}));
}

// <operation> :=
//   A `[` $operandA `]` B `[` $operandB `]` C `[` $operandC `]`
//   attr-dict : (type($operandA[0]), type($operandB[0]), type($operandC[0]))
//     `->` type($res)
ParseResult MmaOp::parse(OpAsmParser &parser, OperationState &result) {
  struct OperandFragment {
    std::optional<MMATypes> elemtype;
    SmallVector<OpAsmParser::UnresolvedOperand, 4> regs;
    SmallVector<Type> regTypes;
  };

  Builder &builder = parser.getBuilder();
  std::array<OperandFragment, 4> frags;

  NamedAttrList namedAttributes;

  // A helper to parse the operand segments.
  auto parseMmaOperand = [&](StringRef operandName,
                             OperandFragment &frag) -> LogicalResult {
    if (parser.parseKeyword(operandName).failed())
      return failure();
    if (parser
            .parseOperandList(frag.regs, OpAsmParser::Delimiter::OptionalSquare)
            .failed())
      return failure();
    return success();
  };

  // Parse the operand segments.
  if (parseMmaOperand("A", frags[0]).failed())
    return failure();
  if (parseMmaOperand("B", frags[1]).failed())
    return failure();
  if (parseMmaOperand("C", frags[2]).failed())
    return failure();

  if (parser.parseOptionalAttrDict(namedAttributes).failed())
    return failure();

  // Parse the type specification and resolve operands.
  SmallVector<Type, 3> operandTypes;
  if (failed(parser.parseColon()))
    return failure();
  if (failed(parser.parseLParen()))
    return failure();
  if (failed(parser.parseTypeList(operandTypes)))
    return failure();
  if (failed(parser.parseRParen()))
    if (operandTypes.size() != 3)
      return parser.emitError(
          parser.getNameLoc(),
          "expected one type for each operand segment but got " +
              Twine(operandTypes.size()) + " types");
  for (const auto &iter : llvm::enumerate(operandTypes)) {
    auto &frag = frags[iter.index()];
    frag.regTypes.resize(frag.regs.size(), iter.value());
    if (failed(parser.resolveOperands(frag.regs, frag.regTypes,
                                      parser.getNameLoc(), result.operands)))
      return failure();
    frag.elemtype =
        inferOperandMMAType(frag.regTypes[0], /*isAccum=*/iter.index() < 2);
  }

  Type resultType;
  if (parser.parseArrow() || parser.parseType(resultType))
    return failure();
  frags[3].elemtype = inferOperandMMAType(resultType, /*isAccum=*/true);

  std::array<StringRef, 2> names{"multiplicandAPtxType",
                                 "multiplicandBPtxType"};
  for (unsigned idx = 0; idx < names.size(); idx++) {
    const auto &frag = frags[idx];
    std::optional<NamedAttribute> attr = namedAttributes.getNamed(names[idx]);
    if (!frag.elemtype.has_value() && !attr.has_value()) {
      return parser.emitError(
          parser.getNameLoc(),
          "attribute " + names[idx] +
              " is not provided explicitly and cannot be inferred");
    }
    if (!attr.has_value())
      result.addAttribute(
          names[idx], MMATypesAttr::get(parser.getContext(), *frag.elemtype));
  }

  result.addTypes(resultType);
  if (!namedAttributes.empty())
    result.addAttributes(namedAttributes);
  result.addAttribute(MmaOp::getOperandSegmentSizeAttr(),
                      builder.getDenseI32ArrayAttr({
                          static_cast<int32_t>(frags[0].regs.size()),
                          static_cast<int32_t>(frags[1].regs.size()),
                          static_cast<int32_t>(frags[2].regs.size()),
                      }));
  return success();
}

LogicalResult MmaOp::verify() {
  MLIRContext *context = getContext();
  auto f16Ty = Float16Type::get(context);
  auto i32Ty = IntegerType::get(context, 32);
  auto f16x2Ty = LLVM::getFixedVectorType(f16Ty, 2);
  auto f32Ty = Float32Type::get(context);
  auto f16x2x4StructTy = LLVM::LLVMStructType::getLiteral(
      context, {f16x2Ty, f16x2Ty, f16x2Ty, f16x2Ty});

  auto s32x4StructTy =
      LLVM::LLVMStructType::getLiteral(context, {i32Ty, i32Ty, i32Ty, i32Ty});
  auto f32x8StructTy =
      LLVM::LLVMStructType::getLiteral(context, SmallVector<Type>(8, f32Ty));
  auto f16x2x2StructTy =
      LLVM::LLVMStructType::getLiteral(context, {f16x2Ty, f16x2Ty});
  auto f32x4StructTy =
      LLVM::LLVMStructType::getLiteral(context, {f32Ty, f32Ty, f32Ty, f32Ty});
  auto s32x2StructTy =
      LLVM::LLVMStructType::getLiteral(context, {i32Ty, i32Ty});

  std::array<int64_t, 3> mmaShape{getShapeAttr().getM(), getShapeAttr().getN(),
                                  getShapeAttr().getK()};

  // These variables define the set of allowed data types for matrices A, B, C,
  // and result.
  using AllowedShapes = SmallVector<std::array<int64_t, 3>, 2>;
  using AllowedTypes = SmallVector<SmallVector<Type, 4>, 2>;
  AllowedShapes allowedShapes;
  AllowedTypes expectedA;
  AllowedTypes expectedB;
  AllowedTypes expectedC;
  SmallVector<Type> expectedResult;

  // When M = 16, we just need to calculate the number of 8xk tiles, where
  // k is a factor that depends on the data type.
  if (mmaShape[0] == 16) {
    int64_t kFactor;
    Type multiplicandFragType;
    switch (*getMultiplicandAPtxType()) {
    case MMATypes::tf32:
      kFactor = 4;
      multiplicandFragType = i32Ty;
      expectedResult.push_back(LLVM::LLVMStructType::getLiteral(
          context, {f32Ty, f32Ty, f32Ty, f32Ty}));
      break;
    case MMATypes::f16:
    case MMATypes::bf16:
      kFactor = 8;
      multiplicandFragType = f16x2Ty;
      expectedResult.push_back(f16x2x2StructTy);
      expectedResult.push_back(f32x4StructTy);
      break;
    case MMATypes::s4:
    case MMATypes::u4:
      kFactor = 32;
      break;
    case MMATypes::b1:
      kFactor = 128;
      break;
    case MMATypes::s8:
    case MMATypes::u8:
      kFactor = 16;
      break;
    default:
      return emitError("invalid shape or multiplicand type: " +
                       stringifyEnum(getMultiplicandAPtxType().value()));
    }

    if (isIntegerPtxType(getMultiplicandAPtxType().value())) {
      expectedResult.push_back(s32x4StructTy);
      expectedC.emplace_back(4, i32Ty);
      multiplicandFragType = i32Ty;
    } else {
      expectedC.emplace_back(2, f16x2Ty);
      expectedC.emplace_back(4, f32Ty);
    }

    int64_t unitA = (mmaShape[0] / 8) * (mmaShape[2] / kFactor);
    int64_t unitB = (mmaShape[1] / 8) * (mmaShape[2] / kFactor);
    expectedA.emplace_back(unitA, multiplicandFragType);
    expectedB.emplace_back(unitB, multiplicandFragType);
    allowedShapes.push_back({16, 8, kFactor});
    allowedShapes.push_back({16, 8, kFactor * 2});
  }

  // In the M=8 case, there is only 1 possible case per data type.
  if (mmaShape[0] == 8) {
    if (*getMultiplicandAPtxType() == MMATypes::f16) {
      expectedA.emplace_back(2, f16x2Ty);
      expectedB.emplace_back(2, f16x2Ty);
      expectedResult.push_back(f16x2x4StructTy);
      expectedResult.push_back(f32x8StructTy);
      expectedC.emplace_back(4, f16x2Ty);
      expectedC.emplace_back(8, f32Ty);
      allowedShapes.push_back({8, 8, 4});
    }
    if (*getMultiplicandAPtxType() == MMATypes::f64) {
      Type f64Ty = Float64Type::get(context);
      expectedA.emplace_back(1, f64Ty);
      expectedB.emplace_back(1, f64Ty);
      expectedC.emplace_back(2, f64Ty);
      // expectedC.emplace_back(1, LLVM::getFixedVectorType(f64Ty, 2));
      expectedResult.emplace_back(LLVM::LLVMStructType::getLiteral(
          context, SmallVector<Type>(2, f64Ty)));
      allowedShapes.push_back({8, 8, 4});
    }
    if (isIntegerPtxType(getMultiplicandAPtxType().value())) {
      expectedA.push_back({i32Ty});
      expectedB.push_back({i32Ty});
      expectedC.push_back({i32Ty, i32Ty});
      expectedResult.push_back(s32x2StructTy);
      if (isInt4PtxType(getMultiplicandAPtxType().value()))
        allowedShapes.push_back({8, 8, 32});
      if (isInt8PtxType(getMultiplicandAPtxType().value()))
        allowedShapes.push_back({8, 8, 16});
      if (getMultiplicandAPtxType().value() == MMATypes::b1)
        allowedShapes.push_back({8, 8, 128});
    }
  }

  std::string errorMessage;
  llvm::raw_string_ostream errorStream(errorMessage);

  // Check that we matched an existing shape/dtype combination.
  if (expectedA.empty() || expectedB.empty() || expectedC.empty() ||
      !llvm::is_contained(allowedShapes, mmaShape)) {
    errorStream << "unimplemented variant for MMA shape <";
    llvm::interleaveComma(mmaShape, errorStream);
    errorStream << ">";
    return emitOpError(errorMessage);
  }

  // Verify the operand types for segments of A, B, and C operands.
  std::array<StringRef, 3> operandNames{"A", "B", "C"};
  for (const auto &iter : llvm::enumerate(
           SmallVector<AllowedTypes, 3>{expectedA, expectedB, expectedC})) {
    auto spec = this->getODSOperandIndexAndLength(iter.index());
    SmallVector<Type, 4> operandTySeg(operand_type_begin() + spec.first,
                                      operand_type_begin() + spec.first +
                                          spec.second);
    bool match = llvm::is_contained(iter.value(), operandTySeg);

    if (!match) {
      errorStream << "Could not match types for the "
                  << operandNames[iter.index()]
                  << " operands; expected one of ";
      for (const auto &x : iter.value()) {
        errorStream << x.size() << "x" << x[0] << " ";
      }
      errorStream << "but got ";
      llvm::interleaveComma(operandTySeg, errorStream);
      return emitOpError(errorStream.str());
    }
  }

  // Check the result type
  if (!llvm::any_of(expectedResult, [&](Type expectedResultType) {
        return expectedResultType == getResult().getType();
      })) {
    errorStream
        << "Could not match allowed types for the result; expected one of ";
    llvm::interleaveComma(expectedResult, errorStream);
    errorStream << " but got " << getResult().getType();
    return emitOpError(errorStream.str());
  }

  // Ensure that binary MMA variants have a b1 MMA operation defined.
  if (getMultiplicandAPtxType() == MMATypes::b1 && !getB1Op()) {
    return emitOpError("op requires " + getB1OpAttrName().strref() +
                       " attribute");
  }

  // Ensure int4/int8 MMA variants specify the accum overflow behavior
  // attribute.
  if (isInt4PtxType(*getMultiplicandAPtxType()) ||
      isInt8PtxType(*getMultiplicandAPtxType())) {
    if (!getIntOverflowBehavior())
      return emitOpError("op requires " +
                         getIntOverflowBehaviorAttrName().strref() +
                         " attribute");
  }

  return success();
}

LogicalResult ShflOp::verify() {
  if (!(*this)->getAttrOfType<UnitAttr>("return_value_and_is_valid"))
    return success();
  auto type = llvm::dyn_cast<LLVM::LLVMStructType>(getType());
  auto elementType = (type && type.getBody().size() == 2)
                         ? llvm::dyn_cast<IntegerType>(type.getBody()[1])
                         : nullptr;
  if (!elementType || elementType.getWidth() != 1)
    return emitError("expected return type to be a two-element struct with "
                     "i1 as the second element");
  return success();
}

std::pair<mlir::Type, unsigned> NVVM::inferMMAType(NVVM::MMATypes type,
                                                   NVVM::MMAFrag frag, int nRow,
                                                   int nCol,
                                                   MLIRContext *context) {
  unsigned numberElements = 0;
  Type elementType;
  OpBuilder builder(context);
  Type f16x2 = VectorType::get(2, builder.getF16Type());
  if (type == NVVM::MMATypes::f16) {
    elementType = f16x2;
    if (frag == NVVM::MMAFrag::a || frag == NVVM::MMAFrag::b)
      numberElements = 8;
    else
      numberElements = 4;
  } else if (type == NVVM::MMATypes::f32) {
    elementType = builder.getF32Type();
    numberElements = 8;
  } else if (type == NVVM::MMATypes::tf32) {
    elementType = builder.getI32Type();
    numberElements = 4;
  } else if (type == NVVM::MMATypes::s8 || type == NVVM::MMATypes::u8) {
    elementType = builder.getI32Type();
    int parallelSize = 0;
    if (frag == NVVM::MMAFrag::a)
      parallelSize = nRow;
    if (frag == NVVM::MMAFrag::b)
      parallelSize = nCol;

    // m == 16 && n == 16 && k == 16
    if (parallelSize == 16)
      numberElements = 2;
    // m == 8 && n == 32 && k == 16 or m == 32 && n == 8 && k == 16
    else if (parallelSize == 8)
      numberElements = 1;
    else if (parallelSize == 32)
      numberElements = 4;
  } else if (type == NVVM::MMATypes::s32) {
    elementType = builder.getI32Type();
    numberElements = 8;
  }
  assert(numberElements != 0 && elementType != nullptr);
  return std::make_pair(elementType, numberElements);
}

static std::pair<mlir::Type, unsigned>
inferMMATypeFromMNK(NVVM::MMATypes type, NVVM::MMAFrag frag, int m, int n,
                    int k, MLIRContext *context) {
  int nRow, nCol;
  if (frag == NVVM::MMAFrag::a) {
    nRow = m;
    nCol = k;
  } else if (frag == NVVM::MMAFrag::b) {
    nRow = k;
    nCol = n;
  } else {
    nRow = m;
    nCol = n;
  }
  assert(nRow && nCol);
  return inferMMAType(type, frag, nRow, nCol, context);
}

LogicalResult NVVM::WMMALoadOp::verify() {
  unsigned addressSpace =
      llvm::cast<LLVM::LLVMPointerType>(getPtr().getType()).getAddressSpace();
  if (addressSpace != 0 && addressSpace != 1 && addressSpace != 3)
    return emitOpError("expected source pointer in memory "
                       "space 0, 1, 3");

  if (NVVM::WMMALoadOp::getIntrinsicID(getM(), getN(), getK(), getLayout(),
                                       getEltype(), getFrag()) == 0)
    return emitOpError() << "invalid attribute combination";
  std::pair<Type, unsigned> typeInfo = inferMMATypeFromMNK(
      getEltype(), getFrag(), getM(), getN(), getK(), getContext());
  Type dstType = LLVM::LLVMStructType::getLiteral(
      getContext(), SmallVector<Type, 8>(typeInfo.second, typeInfo.first));
  if (getType() != dstType)
    return emitOpError("expected destination type is a structure of ")
           << typeInfo.second << " elements of type " << typeInfo.first;
  return success();
}

LogicalResult NVVM::WMMAStoreOp::verify() {
  unsigned addressSpace =
      llvm::cast<LLVM::LLVMPointerType>(getPtr().getType()).getAddressSpace();
  if (addressSpace != 0 && addressSpace != 1 && addressSpace != 3)
    return emitOpError("expected operands to be a source pointer in memory "
                       "space 0, 1, 3");

  if (NVVM::WMMAStoreOp::getIntrinsicID(getM(), getN(), getK(), getLayout(),
                                        getEltype()) == 0)
    return emitOpError() << "invalid attribute combination";
  std::pair<Type, unsigned> typeInfo = inferMMATypeFromMNK(
      getEltype(), NVVM::MMAFrag::c, getM(), getN(), getK(), getContext());
  if (getArgs().size() != typeInfo.second)
    return emitOpError() << "expected " << typeInfo.second << " data operands";
  if (llvm::any_of(getArgs(), [&typeInfo](Value operands) {
        return operands.getType() != typeInfo.first;
      }))
    return emitOpError() << "expected data operands of type " << typeInfo.first;
  return success();
}

LogicalResult NVVM::WMMAMmaOp::verify() {
  if (NVVM::WMMAMmaOp::getIntrinsicID(getM(), getN(), getK(), getLayoutA(),
                                      getLayoutB(), getEltypeA(),
                                      getEltypeB()) == 0)
    return emitOpError() << "invalid attribute combination";
  std::pair<Type, unsigned> typeInfoA = inferMMATypeFromMNK(
      getEltypeA(), NVVM::MMAFrag::a, getM(), getN(), getK(), getContext());
  std::pair<Type, unsigned> typeInfoB = inferMMATypeFromMNK(
      getEltypeA(), NVVM::MMAFrag::b, getM(), getN(), getK(), getContext());
  std::pair<Type, unsigned> typeInfoC = inferMMATypeFromMNK(
      getEltypeB(), NVVM::MMAFrag::c, getM(), getN(), getK(), getContext());
  SmallVector<Type, 32> arguments;
  arguments.append(typeInfoA.second, typeInfoA.first);
  arguments.append(typeInfoB.second, typeInfoB.first);
  arguments.append(typeInfoC.second, typeInfoC.first);
  unsigned numArgs = arguments.size();
  if (getArgs().size() != numArgs)
    return emitOpError() << "expected " << numArgs << " arguments";
  for (unsigned i = 0; i < numArgs; i++) {
    if (getArgs()[i].getType() != arguments[i])
      return emitOpError() << "expected argument " << i << " to be of type "
                           << arguments[i];
  }
  Type dstType = LLVM::LLVMStructType::getLiteral(
      getContext(), SmallVector<Type, 8>(typeInfoC.second, typeInfoC.first));
  if (getType() != dstType)
    return emitOpError("expected destination type is a structure of ")
           << typeInfoC.second << " elements of type " << typeInfoC.first;
  return success();
}

LogicalResult NVVM::LdMatrixOp::verify() {
  unsigned addressSpace =
      llvm::cast<LLVM::LLVMPointerType>(getPtr().getType()).getAddressSpace();
  if (addressSpace != 3)
    return emitOpError("expected source pointer in memory space 3");

  if (getNum() != 1 && getNum() != 2 && getNum() != 4)
    return emitOpError("expected num attribute to be 1, 2 or 4");

  Type i32 = IntegerType::get(getContext(), 32);
  if (getNum() == 1 && getType() != i32)
    return emitOpError("expected destination type is i32");
  if (getNum() == 2 || getNum() == 4) {
    Type dstType = LLVM::LLVMStructType::getLiteral(
        getContext(), SmallVector<Type>(getNum(), i32));
    if (getType() != dstType)
      return emitOpError("expected destination type is a structure of ")
             << getNum() << " elements of type i32";
  }
  return success();
}

//===----------------------------------------------------------------------===//
// NVVMDialect initialization, type parsing, and registration.
//===----------------------------------------------------------------------===//

// TODO: This should be the llvm.nvvm dialect once this is supported.
void NVVMDialect::initialize() {
  addOperations<
#define GET_OP_LIST
#include "mlir/Dialect/LLVMIR/NVVMOps.cpp.inc"
      >();
  addAttributes<
#define GET_ATTRDEF_LIST
#include "mlir/Dialect/LLVMIR/NVVMOpsAttributes.cpp.inc"
      >();

  // Support unknown operations because not all NVVM operations are
  // registered.
  allowUnknownOperations();
  declarePromisedInterface<ConvertToLLVMPatternInterface>();
  declarePromisedInterface<gpu::TargetAttrInterface>();
}

LogicalResult NVVMDialect::verifyOperationAttribute(Operation *op,
                                                    NamedAttribute attr) {
  StringAttr attrName = attr.getName();
  // Kernel function attribute should be attached to functions.
  if (attrName == NVVMDialect::getKernelFuncAttrName()) {
    if (!isa<LLVM::LLVMFuncOp>(op)) {
      return op->emitError() << "'" << NVVMDialect::getKernelFuncAttrName()
                             << "' attribute attached to unexpected op";
    }
  }
  // If maxntid and reqntid exist, it must be an array with max 3 dim
  if (attrName == NVVMDialect::getMaxntidAttrName() ||
      attrName == NVVMDialect::getReqntidAttrName()) {
    auto values = llvm::dyn_cast<ArrayAttr>(attr.getValue());
    if (!values || values.empty() || values.size() > 3)
      return op->emitError()
             << "'" << attrName
             << "' attribute must be integer array with maximum 3 index";
    for (auto val : llvm::cast<ArrayAttr>(attr.getValue())) {
      if (!llvm::dyn_cast<IntegerAttr>(val))
        return op->emitError()
               << "'" << attrName
               << "' attribute must be integer array with maximum 3 index";
    }
  }
  // If minctasm and maxnreg exist, it must be an array with max 3 dim
  if (attrName == NVVMDialect::getMinctasmAttrName() ||
      attrName == NVVMDialect::getMaxnregAttrName()) {
    if (!llvm::dyn_cast<IntegerAttr>(attr.getValue()))
      return op->emitError()
             << "'" << attrName << "' attribute must be integer constant";
  }

  return success();
}

//===----------------------------------------------------------------------===//
// NVVM target attribute.
//===----------------------------------------------------------------------===//
LogicalResult
NVVMTargetAttr::verify(function_ref<InFlightDiagnostic()> emitError,
                       int optLevel, StringRef triple, StringRef chip,
                       StringRef features, DictionaryAttr flags,
                       ArrayAttr files) {
  if (optLevel < 0 || optLevel > 3) {
    emitError() << "The optimization level must be a number between 0 and 3.";
    return failure();
  }
  if (triple.empty()) {
    emitError() << "The target triple cannot be empty.";
    return failure();
  }
  if (chip.empty()) {
    emitError() << "The target chip cannot be empty.";
    return failure();
  }
  if (files && !llvm::all_of(files, [](::mlir::Attribute attr) {
        return attr && mlir::isa<StringAttr>(attr);
      })) {
    emitError() << "All the elements in the `link` array must be strings.";
    return failure();
  }
  return success();
}

#define GET_OP_CLASSES
#include "mlir/Dialect/LLVMIR/NVVMOps.cpp.inc"

#define GET_ATTRDEF_CLASSES
#include "mlir/Dialect/LLVMIR/NVVMOpsAttributes.cpp.inc"
